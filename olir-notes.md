# Olir's Notes — BLE Mesh Discovery Protocol

Notes from a walkthrough conversation covering the protocol, codebase structure, and testing approach.

---

## What is Phase 1 (Discovery)?

Phase 1 exists to answer one question for every node: **"Who else is out there, and how crowded is it around me?"**

By the end of Phase 1, every node has two things:
1. **A neighbor table** — who it has heard from, at what signal strength, and through how many hops
2. **A crowding factor** — a number (0.0–1.0) capturing how dense the radio environment is locally

These are prerequisites for Phase 2 (Clusterhead Election). You can't elect a clusterhead without knowing who is well-connected. You can't adapt to network density without first measuring it.

### Why don't all nodes hear from all other nodes?

Two mechanisms prevent broadcast storms:
1. **TTL** — messages start at TTL=10 and decrement at each hop. Nodes more than 10 hops away never receive the message at all.
2. **Picky forwarding** — `P(forward) = 1 - crowding_factor`. In a dense area with crowding 0.8, only 20% of received messages get retransmitted. This prevents a single broadcast from saturating the channel.

The neighbor table is intentionally a **local neighborhood view**, not a complete map of the whole network.

---

## What are the 4 slots?

Each node gets a repeating **400ms time budget** split into 4 slots of 100ms each:
- **Slot 0** — transmit own discovery message
- **Slots 1–3** — forward up to 3 received messages (highest TTL first)

This is **not a BLE hardware concept**. It is the protocol's own rule sitting on top of BLE. The hardware knows nothing about these 4 slots. The slot limit caps any node at 4 transmissions per 400ms regardless of how many messages it received, preventing any one node from monopolizing the channel.

---

## How are messages stored and forwarded?

Each node has a `ble_message_queue_t` — a fixed array of 100 slots.

**When a message arrives**, three checks run before it is stored:
1. Loop check — is this node's own ID in the PSF? If yes, drop.
2. Duplicate check — has this message hash been seen before? If yes, drop.
3. Overflow check — is the queue at 100 messages? If yes, drop.

Priority is calculated as `255 - TTL`. Lower number = higher priority = forwarded first.

**When a forwarding slot fires**, `dequeue()` scans the array, finds the lowest priority number (highest TTL), and that message gets transmitted.

Receiving and forwarding are completely decoupled in time. Messages not forwarded in the current cycle carry over to the next.

---

## When does Phase 1 end?

**Phase 1 (DISCOVERY) never ends on its own.** It runs indefinitely once started.

What does end are the two pre-phases that come before it:
- **NOISY phase** — runs for exactly `noise_slot_count` slots (default 10 × 200ms = 2s), then automatically transitions to NEIGHBOR
- **NEIGHBOR phase** — runs for exactly `neighbor_slot_count` slots (default 200 × 10ms = 2s), then automatically transitions to DISCOVERY

The transition *out of* DISCOVERY into Phase 2 is not yet implemented. It is left to a higher-level layer (`BleDiscoveryProtocol`) that doesn't exist yet. In the current simulations, the sim script itself stops after a fixed duration.

---

## Are the NOISY and NEIGHBOR pre-phases required?

No — they are **optimizations, not prerequisites**.

- **NOISY phase** produces the crowding factor. Without it, the default is 0.5, so picky forwarding still works — just suboptimally.
- **NEIGHBOR phase** pre-populates the neighbor table. Without it, the table starts empty and gets built from scratch during discovery, which is slightly slower but reaches the same result.

---

## How does the protocol code run on a real board?

The C code is **purely logic** — it has no clock, no hardware references, and no BLE API calls. The only includes are `<stdint.h>` and `<stdbool.h>`, which exist on every C compiler on every platform.

The key design: **callbacks flip the dependency around**. The C code doesn't call a timer — something external calls the C code when a slot fires. On a Nordic nRF52 that would be a Zephyr `k_timer` interrupt. In NS-3 it is `Simulator::Schedule`. The C code is identical in both cases.

Similarly, the C code doesn't call `bt_le_adv_start()` — it fires a send callback with the serialized packet bytes. The platform-specific code decides what to do with those bytes.

### BLE channels and slots

The protocol's "slots" and BLE's hardware slots are completely different things. BLE advertising automatically cycles through channels 37, 38, 39 — the application doesn't control this. The protocol's `ble_broadcast_timing` just outputs a boolean: broadcast this slot? or listen? The BLE stack handles everything below that.

One real-world complication: BLE advertising adds a random 0–10ms jitter per event to reduce collisions. The protocol's slot timing assumes rough synchronization. This jitter is something the `SimpleVirtualChannel` silently ignores but a real BLE PHY simulation would expose.

---

## Why is SimpleVirtualChannel used instead of real BLE nodes?

The BLE module (Stijn's ns3-ble-module) **is** already integrated at `ns3_dev/ns-3-dev/src/ble/`. The `ble-mesh-discovery` module depends on it.

`SimpleVirtualChannel` is deliberate **scaffolding for protocol development**:
- It lets the protocol logic be developed and tested without the complexity of real PHY
- RSSI is hardcoded to `-45 dBm` (no propagation model)
- Delivery is guaranteed with a fixed 1ms delay (no collisions, no packet loss)
- The adjacency table never changes (no mobility)

NS-3 is currently being used only as a **discrete event scheduler** (`Simulator::Schedule`), not for physical modeling.

---

## What physical factors affect reliability, and does NS-3 handle them?

| Factor | NS-3 BLE module | Current sim |
|--------|----------------|-------------|
| Path loss (distance attenuation) | `PropagationLossModel` — built in | Hardcoded `-45 dBm` |
| Multipath fading | `NakagamiFading` — optional | Not modeled |
| Packet collisions | `BleErrorModel` + PHY state machine | Not modeled |
| Node mobility | Full mobility module | Static positions |
| Obstacles/shadowing | `BuildingsPropagationLossModel` | Not modeled |
| Energy/battery | Energy module | Not modeled |

All of these become available automatically once the engine is wired into real `ns3::Node` + `BleNetDevice` instead of `SimpleVirtualChannel`.

---

## What are the next steps to test physical modeling?

Three steps in order:

**Step 1: Write `BleDiscoveryProtocol`**
The missing glue layer between `BleDiscoveryEngineWrapper` and `BleNetDevice`. Wires the engine's send callback to `BleNetDevice::Send()` and hooks `NotifyReceptionEndOk` to feed packets into `BleDiscoveryEngineWrapper::Receive()`.

**Step 2: Write `BleMeshHelper`**
Sets up `ns3::Node` + `BleNetDevice` + `BleDiscoveryProtocol` in one call. Follows the pattern of the existing `BleHelper`.

**Step 3: Write a new sim script using real nodes**
Replaces `SimpleVirtualChannel` with a real `SpectrumChannel` + `PropagationLossModel`. Once this exists, physical modeling works automatically.

---

## What is the point of the engine-core?

`ble_discovery_engine.h/.c` is a **pure C coordinator** that wires all the protocol-core modules together into a single running state machine. It manages the three phases (NOISY → NEIGHBOR → DISCOVERY), drives ticks, and calls the right callbacks at the right times.

It exists in C (not C++) for the same reason the protocol-core does — **it needs to run on real hardware too**, not just in NS-3. The NS-3 wrapper (`BleDiscoveryEngineWrapper`) just gives it a clock via `Simulator::Schedule` and translates its send callback into an NS-3 `Packet`.

NS-3 is not being used for its PHY simulation capabilities in the current sims. It is being used as a scheduler. Physical modeling is the **eventual** point of the NS-3 integration, but the project isn't there yet.

---

## Recommended approach for iterative phase testing (from scratch)

### The four layers

| Layer | Tool | Question answered |
|-------|------|-------------------|
| 1 | `gcc` C unit tests | Does each module's logic work? |
| 2 | Pure C multi-node simulator | Does the full protocol work end-to-end? |
| 3 | NS-3 + SimpleVirtualChannel | Does NS-3 scheduling fire at the right times? |
| 4 | NS-3 + BleNetDevice | Does the protocol survive real radio conditions? |

**Layer 2 is the missing piece** in the current project. A simple `sim.c` with a fake clock loop calling `ble_engine_tick()` on N node structs and passing packets between them via function calls would have been the right first step before reaching for NS-3 at all.

The current project skipped Layer 2 and went straight to NS-3's `SimpleVirtualChannel`, which is essentially the same fake simulator but buried inside C++ NS-3 objects — paying NS-3 complexity without getting NS-3 PHY benefits.

**Layer 3 is a debugging convenience, not a fundamental requirement.** It can be merged into Layer 4 if you're confident in the NS-3 wrapper code. The only reason to keep it separate is that Layer 4 introduces many variables at once; Layer 3 gives you a checkpoint that isolates NS-3 integration issues from physical modeling issues.

### Key principle for iterative phase testing

Each phase needs a **well-defined input/output contract**. A Phase N+1 test should be able to construct Phase N's outputs directly — without running Phase N — so phases can be tested in isolation.

Example: Phase 2 (election) should accept a hand-crafted neighbor table and crowding factor as inputs. You should be able to test election logic fully without ever running Phase 1.

The current engine bundles all phases together in one `ble_engine_t` struct and advances through them automatically, which makes this harder.

---

## Measurable test outputs

### Layer 1 & 2 (exact, deterministic)
- A message with TTL=10 reaches exactly nodes within 10 hops — no more, no less
- A node not heard for 3 cycles is evicted from the neighbor table
- A duplicate message increments `total_duplicates` and is not queued
- A message with own node ID in PSF increments `total_loops` and is dropped
- After exactly `noise_slot_count` ticks, `engine->phase == BLE_ENGINE_PHASE_NEIGHBOR`
- With a seeded RNG and known RSSI inputs, the crowding factor is a deterministic exact value

### Layer 4 (statistical, ranges and trends)
- **Packet delivery ratio (PDR)** — should be >95% at short range in open space, degrading gracefully with distance
- **RSSI vs distance curve** — should match the configured propagation model's predictions
- **Neighbor table accuracy** — in a known fixed topology, every node's table should contain exactly the nodes physically within range
- **Crowding factor under load** — 100 nodes in 10m² should produce a measurably higher crowding factor than 10 nodes in the same area
- **Forwarding rate vs crowding** — picky forwarding should produce fewer transmissions per node in dense scenarios than sparse ones

---

## What the C code actually is

The C code is the **complete decision-making brain** of a single node. It is a software model of the protocol — all the rules the protocol defines expressed as executable code.

Every struct is one node's data. Every function operates from the perspective of one node. In a multi-node simulation you create N separate instances of these structs — one complete set per node — and they never share memory.

**What it decides:**
- Should this message be forwarded?
- Who counts as a neighbor, when are they evicted?
- What is the crowding factor?
- Should this node become a clusterhead candidate?
- Which state should this node be in?
- Which queued messages get forwarded this cycle?

**What it does not decide:**
- When to actually transmit (fires a callback, platform handles it)
- Whether the packet actually arrives (path loss, collisions — platform's job)
- What time it is (platform passes `now_ms` in)
- Which BLE channel to use (hardware handles it)

The same C code runs unchanged on a Nordic board, in the pure C simulator, and in NS-3. Only the environment changes.
