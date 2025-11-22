# BLE Mesh Discovery Module Snapshot (Pre-Merge)

## 1. Architecture Overview

- **Portable C core** (under `src/ble-mesh-discovery/model/protocol-core/`).
- **Engine core** (`model/engine-core/ble_discovery_engine.{c,h}`) orchestrates the Phase‑2 behavior entirely in C.
- **NS-3 wrappers** (`model/*.cc`) expose the C functionality as `ns3::Object`s.
- **Tests** exist in both pure C (`test/ble-*-c-test.c`) and NS‑3 (`test/*.cc`) layers.

This snapshot captures the state before a branch merge so newcomers can diff later.

## 2. Pure C Core Components

| File | Purpose |
|------|---------|
| `ble_discovery_packet.{c,h}` | Defines discovery/election packets: fields (ID, TTL, PSF, GPS, election extension), serialization/deserialization, TTL + path operations, GPS setters, ΣΠ-based PDSF helper with Last Π tracking, score/hash helpers. |
| `ble_discovery_cycle.{c,h}` | 4-slot discovery cycle state machine with callbacks per slot and cycle-complete callback; configurable slot duration. |
| `ble_message_queue.{c,h}` | Fixed-size queue with loop detection, dedup cache, TTL-based priority, stats counters. |
| `ble_forwarding_logic.{c,h}` | Implements crowding-factor calculation (RSSI → 0..1), probabilistic forwarding with internal RNG, GPS proximity filtering, TTL gating, and noise-level helper. |
| `ble_broadcast_timing.{c,h}` | Adaptive scheduling for noisy/RSSI sampling (default heavy TX) and neighbor discovery (200-slot listen-heavy schedule with crowding-aware TX caps via `ble_broadcast_timing_set_crowding`). |
| `ble_mesh_node.{c,h}` | Node state machine: neighbor table, GPS cache, statistics, noise storage, dynamic candidacy threshold (6→3→1) with helpers, edge/candidate checks, candidacy score. |

All core files are portable C (no NS-3 deps) and are already unit-tested.

## 3. Engine Core

`model/engine-core/ble_discovery_engine.{c,h}` provides:
- Engine config struct (node ID, slot duration, TTL, proximity threshold, callbacks).
- Engine context structure with discovery cycle, message queue, mesh node, crowding factor, etc.
- APIs:
  - `ble_engine_config_init`, `ble_engine_init`, `ble_engine_reset`.
  - `ble_engine_tick` (advances one slot).
  - `ble_engine_receive_packet` (ingest RX packets + RSSI).
  - `ble_engine_set_noise_level`, `ble_engine_set_crowding_factor`, `ble_engine_set_gps`.
  - `ble_engine_mark_candidate_heard`, `ble_engine_seed_random`, `ble_engine_get_node`.
- Slot handlers: own-message TX (fills TTL/PSF/GPS), queue forwarding (using forwarding logic, TTL decrement, path update).
- Hooks to propagate stats (inc sent/received/forwarded/dropped).

**Current limitation:** Engine only covers Phase‑2 discovery features; state transitions and election handling are deferred to future phases.

## 4. NS-3 Wrappers

| File | Functionality |
|------|---------------|
| `ble-discovery-header-wrapper.{h,cc}` | NS-3 `Header` mapping to the C packet; handles serialization, path/GPS helpers, election fields (PDSF history + Last Π access/update helpers). |
| `ble-message-queue.{h,cc}` | `ns3::Object` around C queue; `Enqueue/Dequeue/Peek/Stats`. |
| `ble-forwarding-logic.{h,cc}` | Wrapper around C forwarding logic; converts `std::vector<int8_t>` + `Vector` to C types; logs decisions. |
| `ble-discovery-cycle-wrapper.{h,cc}` | Wraps discovery cycle; schedules slots via `Simulator`. |
| `ble-mesh-node-wrapper.{h,cc}` | Exposes node state, neighbor ops, GPS updates, candidacy score, noise setters. |
| `ble-discovery-engine-wrapper.{h,cc}` | Drives the C engine from NS-3 event loop; offers attributes (slot duration, TTL, proximity threshold, node ID), start/stop, RX injection, crowding/noise updates, TX callback for packets, RNG seeding. |

## 5. Current Test Coverage

- **Pure C tests** (`test/ble-discovery-packet-c-test.c`, `ble-mesh-node-c-test.c`, `ble-discovery-cycle-c-test.c`, `ble-forwarding-logic-c-test.c`, `ble-discovery-header-test.cc`): Validate serialization, TTL/PSF, candidacy thresholds, RNG behavior, ΣΠ+LastΠ tracking, and wrapper round-trips.
- **NS-3 tests** (`test/ble-forwarding-logic-test.cc`, `ble-message-queue-test.cc`, `ble-mesh-node-test.cc`, `ble-discovery-cycle-test.cc`): Exercise wrappers and integration logic.

No dedicated engine tests exist yet (future work).

## 6. Pending / Planned Work (Phase 3+)

- **State machine transitions & candidacy invocation in engine**: call `ble_mesh_node_should_become_edge()` / `_candidate()` after each cycle and update node state accordingly.
- **Election handling**:
  - Generate election announcements when node enters `BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE`.
  - Parse election packets (PDSF comparison, score/conflict resolution, renouncement).
  - Enforce PDSF-based capacity limit when forwarding.
- **Three-round announcement flooding**: track per-node round counter (0–2) and re-send announcements in consecutive cycles.
- **Geographic distribution + forwarding success metrics**.
- **Routing/multipath (Phase 5)** and logging/helpers.
- **Engine + wrapper tests/examples** once election features land.

## 7. Documentation & Planning Artifacts

- `TODO.md` – canonically tracks every phase/task; highlights the ΣΠ/Last Π election design, adaptive broadcast timing requirements, and outstanding engine work.
- `discovery_protocol.txt` – narrative spec; now includes LP (Last Π) description plus the two-phase stochastic slotting design (crowding-heavy RSSI sampling phase vs. neighbor-discovery listen-heavy phase).
- `merge_report.md` – enumerates upstream changes since the last snapshot so you can diff after pulling.
- `future_report.md` – lists pending engine features (state transitions, election TX/RX wiring, multi-round flooding) with suggested entry points.
- `C-engine_TODO.md` – phased plan for the pure-C engine state machine, used when incrementally implementing/test-driving helpers.
- `bug_report.md` – placeholder for open protocol defects (currently “no bugs reported”).

Use this snapshot to understand the baseline before merging/pulling new work. It highlights what exists and what remains unfinished per the protocol specification.
