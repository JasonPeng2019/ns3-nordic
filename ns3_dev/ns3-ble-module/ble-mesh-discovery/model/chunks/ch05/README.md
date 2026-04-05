# ch05 BLE Discovery Engine (Portable C Core + ns-3 Wrapper)

## Scope
This README is intentionally scoped **only** to files in this directory:

- `ble_discovery_engine.h` (portable C public API)
- `ble_discovery_engine.c` (portable C implementation)
- `ble-discovery-engine.h` (ns-3 C++ wrapper API)
- `ble-discovery-engine.cc` (ns-3 C++ wrapper implementation)

No assumptions here require reading files outside `ch05`; external modules are described from their usage/signatures in these files.

## What This Chunk Implements
`ch05` is the integration layer that turns multiple lower-level BLE mesh helpers into one runnable discovery/data engine.

- The **C engine** (`ble_discovery_engine.c/.h`) owns protocol state and logic:
  - phase machine (`NOISY -> NEIGHBOR -> DISCOVERY`)
  - discovery-cycle slot dispatch and forwarding queue processing
  - election/renouncement handling and clusterhead alignment
  - optional data-phase slot gating and slot-outcome accounting
  - metrics publishing and callback hooks
- The **ns-3 wrapper** (`ble-discovery-engine.cc/.h`) exposes this C engine as an `ns3::Object`, maps ns-3 attributes into `ble_engine_config_t`, schedules periodic ticks, and converts packet/header formats across C and ns-3 boundaries.

## High-Level Runtime Flow
1. `Initialize()` / `ble_engine_init()` configure callbacks, defaults, queue, node, election state, slot assignments, and phase timing.
2. `RunTick()` / `ble_engine_tick()` execute on each scheduled slot:
   - updates `last_tick_time_ms`
   - manages data-phase start/end/restart windows
   - advances NOISY/NEIGHBOR micro-phase slots until DISCOVERY phase
   - runs discovery slot actions (own-message slot or forwarding slots)
3. RX path (`Receive()` / `ble_engine_receive_packet()`):
   - optional data-phase gating
   - election packet handling
   - noisy/neighbor sampling updates
   - enqueue for forwarding
4. End of each discovery cycle (`ble_engine_cycle_complete()`):
   - advance cycle counter, prune stale neighbors
   - refresh election neighbor cache timeout
   - publish metrics callback
   - evaluate state transitions and re-enter NOISY phase

## File-by-File Dependency Map

### `ble_discovery_engine.h`
Direct includes:
- `<stdint.h>`
- `<stdbool.h>`
- `ble_broadcast_timing.h`
- `ble_discovery_cycle.h`
- `ble_discovery_packet.h`
- `ble_election.h`
- `ble_forwarding_logic.h`
- `ble_message_queue.h`
- `ble_mesh_node.h`

Primary dependency role:
- Defines the central `ble_engine_t` that embeds types from all modules above.
- Defines callbacks that expose `ble_discovery_packet_t`, `ble_connectivity_metrics_t`, and slot-event metadata.

### `ble_discovery_engine.c`
Direct includes:
- `ble_discovery_engine.h`
- `<string.h>` (`memset`)
- `<limits.h>` (`UINT32_MAX`, `UINT16_MAX`)

External module dependencies used by symbol family:
- `ble_discovery_cycle_*`
- `ble_queue_*` (`ble_message_queue`)
- `ble_mesh_node_*`
- `ble_election_*`
- `ble_discovery_*` packet/path/GPS/TTL helpers
- `ble_forwarding_*`
- `ble_broadcast_timing_*`

### `ble-discovery-engine.h` (ns-3 wrapper header)
Direct includes:
- `ns3/object.h`, `ns3/callback.h`, `ns3/event-id.h`, `ns3/packet.h`, `ns3/vector.h`, `ns3/nstime.h`, `ns3/traced-callback.h`
- `ble-discovery-header-wrapper.h`
- `extern "C" { #include "ns3/ble_discovery_engine.h" }`

Primary dependency role:
- Bridges ns-3 object system, tracing, events, and packet types to the C engine API.

### `ble-discovery-engine.cc` (ns-3 wrapper impl)
Direct includes:
- `ble-discovery-engine.h`
- `ns3/log.h`, `ns3/simulator.h`, `ns3/double.h`, `ns3/integer.h`, `ns3/uinteger.h`, `ns3/boolean.h`

Primary dependency role:
- Implements `TypeId` attributes/traces.
- Converts ns-3 time to `uint32_t ms` for the C engine.
- Converts between `BleDiscoveryHeaderWrapper` and C packet structs.

## Public Data Types and Constants (`ble_discovery_engine.h`)

### Callback Types
- `ble_engine_send_callback`: engine -> platform TX hook
- `ble_engine_log_callback`: optional log hook
- `ble_engine_metrics_callback`: optional metrics hook
- `ble_engine_slot_callback`: optional slot-outcome hook

### Key Constants
- Noise phase defaults: `BLE_ENGINE_DEFAULT_NOISE_SLOTS`, `BLE_ENGINE_DEFAULT_NOISE_SLOT_DURATION_MS`
- Neighbor phase defaults: `BLE_ENGINE_DEFAULT_NEIGHBOR_SLOTS`, `BLE_ENGINE_DEFAULT_NEIGHBOR_SLOT_DURATION_MS`, `BLE_ENGINE_DEFAULT_NEIGHBOR_TIMEOUT_CYCLES`
- Election defaults: `BLE_ENGINE_MAX_ELECTION_ROUNDS`
- Data-phase defaults: `BLE_ENGINE_DEFAULT_TDMA_SLOTS`, `BLE_ENGINE_DEFAULT_FDMA_CHANNELS`, `BLE_ENGINE_DEFAULT_FRAME_DURATION_MS`, `BLE_ENGINE_DEFAULT_MODE_DURATION_MS`

### Core Structs
- `ble_engine_config_t`: static params + callbacks
- `ble_engine_t`: full runtime state (cycle, queue, node, election, phase, data-slot bookkeeping, metrics snapshot)
- `ble_engine_slot_event_t`: frame/slot/channel/iteration/outcome for tracing

### Enums
- `ble_engine_phase_t`: `NOISY`, `NEIGHBOR`, `DISCOVERY`
- `ble_engine_slot_outcome_t`: `EMPTY`, `TX`, `RX`, `COLLISION`

## Function Reference: Portable C API

### Exported functions (`ble_discovery_engine.h` + implemented in `.c`)
- `ble_engine_config_init`
  - Purpose: zero config and apply defaults.
  - Direct deps: `memset`, compile-time defaults.
- `ble_engine_init`
  - Purpose: validate config, initialize cycle/queue/node/election/timing, assign self slot, start in NOISY phase.
  - Direct deps: discovery cycle setup, queue init, node init/state/slot assignment, election init, broadcast timing init, slot refresh.
- `ble_engine_reset`
  - Purpose: clear runtime state and return to NOISY phase.
  - Direct deps: queue clear, cycle stop, node re-init/clear slots, election + timing re-init, slot refresh.
- `ble_engine_tick`
  - Purpose: one scheduling step; drives phase transitions, data-phase windows, and discovery cycle slots.
  - Direct deps: node state query, data-phase helpers, phase-slot runner, discovery-cycle execute/advance.
- `ble_engine_receive_packet`
  - Purpose: ingest one incoming packet with RSSI/time, update election/neighbor sampling, and queue for forwarding.
  - Direct deps: slot gating, election handler, neighbor/election update helpers, queue enqueue, node counters.
- `ble_engine_set_noise_level`
  - Purpose: store measured noise at node model.
  - Direct deps: `ble_mesh_node_set_noise_level`.
- `ble_engine_mark_candidate_heard`
  - Purpose: flag candidate visibility at node model.
  - Direct deps: `ble_mesh_node_mark_candidate_heard`.
- `ble_engine_set_crowding_factor`
  - Purpose: clamp/store crowding factor and update neighbor timing crowding.
  - Direct deps: `ble_broadcast_timing_set_crowding`.
- `ble_engine_seed_random`
  - Purpose: seed forwarding randomness.
  - Direct deps: `ble_forwarding_set_random_seed`.
- `ble_engine_set_gps`
  - Purpose: set/clear local GPS.
  - Direct deps: node GPS setters/clear.
- `ble_engine_get_node`
  - Purpose: read-only access to embedded node struct.
- `ble_engine_record_slot_event`
  - Purpose: manual slot outcome recording for assigned slot at `now_ms`.
  - Direct deps: slot math, slot counters, optional slot callback.
- `ble_engine_start_data_phase`
  - Purpose: activate data-phase bookkeeping and reset slot counters.
  - Direct deps: slot refresh.
- `ble_engine_end_data_phase`
  - Purpose: deactivate data phase.
- `ble_engine_advance_slot_iteration`
  - Purpose: force iteration `(iter + 1) % 3`.
- `ble_engine_gate_and_record_slot`
  - Purpose: enforce slot ownership and optional collision model, emit slot event, return outcome.
  - Direct deps: slot-window check, collision bookkeeping, node collision stats, slot event emit.

### Internal static functions (`ble_discovery_engine.c`)
- `ble_engine_log`: forwards to `log_cb` when set.
- `ble_engine_slot_dispatch`: maps discovery slots to `ble_engine_transmit_own_message` or `ble_engine_forward_next_message`.
- `ble_engine_cycle_complete`: end-of-cycle maintenance and phase restart.
- `ble_engine_transmit_own_message`: sends renouncement/election if pending, else local discovery packet.
- `ble_engine_forward_next_message`: dequeue policy + TTL/path/PDSF updates + optional data-slot gating.
- `ble_engine_enter_phase`: initialize per-phase timing/listen behavior.
- `ble_engine_run_phase_slot`: consumes one NOISY/NEIGHBOR micro-slot and transitions phase when complete.
- `ble_engine_neighbor_timeout_ms`: computes election neighbor timeout from cycle duration.
- `ble_engine_publish_metrics`: snapshots election metrics + local slot counters and publishes callback.
- `ble_engine_evaluate_state`: state-machine transitions (DISCOVERY/EDGE/CANDIDATE/CLUSTERHEAD).
- `ble_engine_start_election_rounds`: arm rounds and immediately send first election packet.
- `ble_engine_cancel_election_rounds`: clear election-round counters.
- `ble_engine_should_send_election`: per-cycle election send gate.
- `ble_engine_prepare_election_packet`: fill election packet fields/score/PDSF.
- `ble_engine_send_election_packet`: transmit prepared election packet and update counters.
- `ble_engine_should_send_renouncement`: per-cycle renouncement send gate.
- `ble_engine_prepare_renouncement_packet`: fill renouncement packet fields.
- `ble_engine_send_renouncement_packet`: transmit renouncement and update counters.
- `ble_engine_start_renouncement_rounds`: arm renouncement retries.
- `ble_engine_cancel_renouncement_rounds`: clear renouncement retries.
- `ble_engine_clear_selected_clusterhead`: clear CH alignment/slot and reset CH-selection metrics.
- `ble_engine_try_promote_clusterhead`: candidate -> clusterhead promotion once one cycle elapsed after election send.
- `ble_engine_update_clusterhead_selection`: select better CH by hop count, direct-connections, then lower sender ID.
- `ble_engine_handle_clusterhead_renouncement`: clear alignment if selected CH renounces.
- `ble_engine_count_already_reached`: count direct neighbors already in packet path.
- `ble_engine_refresh_slots`: recompute next scheduled self+cluster slot times.
- `ble_engine_emit_slot_event`: update slot counters and dispatch `slot_cb` event.
- `ble_engine_record_slot_outcome`: increment TX/RX/COLLISION/EMPTY counters.
- `ble_engine_is_in_slot`: frame/slot arithmetic for slot-ownership check.
- `ble_engine_autonomous_slot_tick`: emits `EMPTY` outcomes when currently in assigned slots.
- `ble_engine_handle_election_packet`: state-dependent reaction to election or renouncement messages.

## Function Reference: ns-3 Wrapper Class
Class: `ns3::BleDiscoveryEngine` in `ble-discovery-engine.h/.cc`.

### Public methods
- `GetTypeId`:
  - Registers attributes: `SlotDuration`, `InitialTtl`, `ProximityThreshold`, `NoiseSlotCount`, `NoiseSlotDuration`, `NeighborSlotCount`, `NeighborSlotDuration`, `NeighborTimeoutCycles`, `FdmaChannels`, `TdmaSlots`, `FrameDuration`, `Mode1Duration`, `Mode2Duration`, `EnableCollisionModel`, `EnableDataPhase`, `NodeId`.
  - Registers trace sources: `MetricsUpdate`, `SlotOutcome`.
- `BleDiscoveryEngine` (ctor): sets defaults, calls `ble_engine_config_init`.
- `~BleDiscoveryEngine`: calls `Stop`.
- `Initialize`: maps ns-3 attributes into `ble_engine_config_t`, installs static C callbacks, calls `ble_engine_init`.
- `Start`: lazy-initializes if needed, schedules immediate first tick.
- `Stop`: cancels scheduled event.
- `SetSendCallback`: stores outbound packet callback.
- `Receive`: unwraps header (discovery vs election) and calls `ble_engine_receive_packet` with current sim time.
- `SetCrowdingFactor`, `SetNoiseLevel`, `MarkCandidateHeard`, `SetGpsLocation`, `SeedRandom`: thin passthroughs to C API.
- `GetNode`: exposes `ble_engine_get_node`.

### Protected/private methods
- `DoDispose`: stop scheduling then chain to `Object::DoDispose`.
- `ScheduleNextTick`: schedule `RunTick` after `m_slotDuration`.
- `RunTick`: call `ble_engine_tick(now_ms)` then reschedule.
- `EngineSendHook`: static C callback trampoline to `HandleEngineSend`.
- `EngineLogHook`: static C callback -> `NS_LOG_DEBUG`.
- `EngineMetricsHook`: static C callback trampoline to `HandleMetricsUpdate`.
- `EngineSlotHook`: static C callback that converts C slot event into traced `SlotOutcomeEvent`.
- `HandleEngineSend`: C packet -> `BleDiscoveryHeaderWrapper` -> `ns3::Packet` and invoke `m_txCallback`.
- `HandleMetricsUpdate`: fire metrics trace callback.

## Cross-File Integration Contracts
- C engine requires:
  - `config.node_id != 0`
  - `config.send_cb != NULL`
- Wrapper enforces this by:
  - requiring `NodeId` before `Initialize()`
  - always wiring send/log/metrics/slot callbacks.
- Election packets are passed by treating `ble_election_packet_t` as having `ble_discovery_packet_t` base layout at offset 0 (via casts in both RX and TX paths).

## Chunk 5 Plan Fit (Against `chunks/split.md`)
Reference: `split.md` section "Chunk 5: Election Announcement (3 Rounds) + Conflict Logic".

### What Matches
- **3-round election scheduling exists**:
  - `BLE_ENGINE_MAX_ELECTION_ROUNDS = 3`
  - `ble_engine_start_election_rounds()` arms 3 rounds and immediately sends the first election packet.
  - `ble_engine_should_send_election()` + `ble_engine_send_election_packet()` enforce at most one election send per cycle until rounds are exhausted.
- **Election payload propagation/update is implemented**:
  - Source build: `class_id`, `direct_connections`, `pdsf`, `last_pi`, `score`, `hash`, `path`.
  - Forward path: TTL decrement, path append, PDSF update (`ble_election_update_pdsf`), cap check.
- **Conflict resolver logic is implemented**:
  - Candidate/clusterhead conflict winner: higher `direct_connections`; tie -> lower `sender_id`.
- **Renouncement after loss is implemented**:
  - Losing candidate starts renouncement rounds and broadcasts renouncement packets (`is_renouncement` set).

### Where It Deviates / Is Incomplete
- **Missing explicit hard-cap membership validation path in this chunk**:
  - `PDSF` soft-cap forwarding stop exists, but this chunk does not expose separate hard cluster-membership cap invariants or violation counters.
- **Missing explicit metrics required by chunk05 deliverables**:
  - No dedicated counters in this chunk for:
    - `pdsf_cap_stop_events`
    - `cluster_size_hard_cap_violations`
- **No chunk05 exit-gate enforcement in this directory**:
  - No local test artifacts in `ch05` proving:
    - exactly-3-round lifecycle in CI
    - "no unresolved candidate after final round + 1 settle cycle"
    - "forwarded election packets after `pdsf >= cap` are zero" as a gated assertion
- **Component boundary differs from split wording**:
  - `split.md` names flood scheduler deliverable as `BleElectionEngine`; this chunk implements election flow inside `BleDiscoveryEngine` with embedded `ble_election_state_t`.

## Highlights
- Clean separation between **platform-agnostic core** and **ns-3 integration layer**.
- Config defaults are centralized in `ble_engine_config_init`, then reinforced in `ble_engine_init` for zero values.
- Engine is callback-driven and test-friendly (send/log/metrics/slot hooks + RNG seeding).
- Discovery, election, forwarding, and slot-gating are integrated in a single state machine (`ble_engine_tick` + cycle callbacks).
- Wrapper exposes meaningful trace points (`MetricsUpdate`, `SlotOutcome`) for simulation instrumentation.

## Potential Issues / Implementation Risks
1. **Mode2 wait is effectively bypassed by current tick order.**
   - In `ble_engine_tick`, when `data_phase_active` is false and node is aligned/clusterhead, the early “start data phase” check can restart immediately on the next tick, before intended `mode2_duration_ms` wait logic matters.

2. **`ble_engine_reset` clears slots but does not reassign self slot (unlike init).**
   - `ble_engine_init` assigns self slot; `ble_engine_reset` calls `ble_mesh_node_clear_slots` and does not call `ble_mesh_node_assign_self_slot`.
   - Depending on external state transitions, this can leave slot gating behavior different before vs after reset.

3. **Tick cadence in wrapper may not match micro-slot durations.**
   - Wrapper always schedules ticks by `SlotDuration`, while NOISY/NEIGHBOR phases use independent micro-slot durations/counts in engine timing config.
   - If `SlotDuration` differs from micro-slot timings, simulated phase timing may be distorted.

4. **Collision model is single-node and last-event based.**
   - `ble_engine_gate_and_record_slot` marks collision only when current event matches same frame/slot/channel as the previous recorded event in that engine instance.
   - This is lightweight but can under/over-approximate real multi-node collisions.

5. **Election packet casting contract is strict.**
   - RX/TX cast between `ble_discovery_packet_t*` and `ble_election_packet_t*`; callers must provide matching layout for election message types.
   - Invalid caller-side packing/layout would break parsing.

6. **Header/docs wording drift around “placeholder” mode behavior.**
   - Headers/attributes describe mode durations as placeholders, but the C engine actively uses `mode1_duration_ms` / `mode2_duration_ms` in tick logic.

7. **Naming overlap can confuse includes.**
   - Both `ble_discovery_engine.h` (C) and `ble-discovery-engine.h` (C++) exist in same directory with near-identical names.
   - Build/include scripts must be explicit to avoid accidental header selection.

## Prerequisites (Embedded Deployment: Non-Negotiable Requirements)
The following are strict design requirements for deploying this chunk on embedded targets.

### Protocol and Behavior Requirements
- Keep v1 discovery/election **single-channel only**; do not enable multi-channel behavior in this phase.
- Keep v1 wire contract frozen (`ble-mesh-wire-v1.0.0`): no field additions/removals/reordering/encoding changes.
- Enforce election conflict rule exactly: higher `direct_connections`; tie -> lower `node_id`.
- Enforce `PDSF` soft-cap stop on election forwarding and track it with a dedicated counter.
- Enforce hard membership cap as a separate invariant and track violations with a dedicated counter.
- Preserve deterministic replay mode (`D0`) with fixed seed + fixed timing.

### Timing and Scheduler Requirements
- Provide a monotonic millisecond time source with bounded jitter relative to slot timing.
- Use one scheduler owner per node engine instance; no concurrent writes to `ble_engine_t`.
- Align tick cadence with phase micro-slot timing (`noise_slot_duration_ms`, `neighbor_slot_duration_ms`) or document and bound timing distortion.
- Handle 32-bit millisecond wrap-around explicitly in system-level integration tests.

### Memory and Data-Structure Requirements
- Use bounded/static memory for queue/path/neighbor storage; no unbounded growth at runtime.
- Define and enforce compile-time limits for:
  - max queue depth
  - max path length
  - max tracked neighbors
  - max retained election history entries
- Protect all length/count arithmetic against overflow and bounds violations.

### Concurrency, ISR, and Callback Requirements
- Treat engine API as single-threaded unless protected by explicit external synchronization.
- Do not call blocking I/O from `send_cb`, `log_cb`, `metrics_cb`, or `slot_cb`.
- If callbacks cross ISR/task boundaries, use lock-free or bounded-latency queues with backpressure policy.
- Define callback worst-case execution time budgets and verify they fit within slot deadlines.

### Portability and Binary-Contract Requirements
- Use fixed-width integer types only for wire-visible fields.
- Define endian policy for serialization and test roundtrip on target architecture.
- Add compile-time layout checks for base/extended packet compatibility used by casts.
- Compile with warnings-as-errors and UB-focused flags in CI for host builds.

### Safety, Reliability, and Quality Requirements
- Add watchdog-safe integration: engine tick/callback path must be non-blocking and bounded.
- Persist critical config defaults in one source of truth and version them.
- Add mandatory tests for chunk05 gates:
  - exactly 3 election rounds
  - renouncement completion behavior
  - zero forwarded election packets after `pdsf >= cap`
  - no unresolved candidates after final round + 1 settle cycle
- Run static analysis (at minimum clang-tidy/cppcheck; MISRA profile when safety/regulatory constraints apply).
- Provide field diagnostics for election state transitions, cap-stop events, and conflict outcomes.

## Practical Reading Order
1. `ble_discovery_engine.h` for types/config/state and API surface.
2. `ble_engine_init`, `ble_engine_tick`, `ble_engine_receive_packet` in `ble_discovery_engine.c` for core behavior.
3. `ble_engine_forward_next_message` + election helpers for forwarding/election details.
4. `ble-discovery-engine.cc` for ns-3 scheduling, attribute mapping, and packet conversion.
