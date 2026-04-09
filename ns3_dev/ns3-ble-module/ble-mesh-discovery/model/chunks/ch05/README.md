# ch05 BLE Discovery Engine (Portable C Core + ns-3 Wrapper)

## Scope
This README covers only files in `ch05/`:
- `ble_discovery_engine.h` (portable C API)
- `ble_discovery_engine.c` (portable C implementation)
- `ble-discovery-engine.h` (ns-3 wrapper API)
- `ble-discovery-engine.cc` (ns-3 wrapper implementation)

Cross-chunk references are explicitly called out when they affect correctness.

## Chunk 5 Ownership and PDF Mapping
`split.md` Chunk 5 is "Election Announcement (3 Rounds) + Conflict Logic."  
This directory implements that behavior inside `BleDiscoveryEngine` (integrated path), which is allowed by `split.md` "Component Placement and Ownership (Locked)."

### PDF Section 3 Step Mapping
| PDF Step | Behavior | Implemented Here | Notes |
|---|---|---|---|
| Step 4 | Election announcement broadcast | Yes | `ble_engine_start_election_rounds`, `ble_engine_should_send_election`, `ble_engine_send_election_packet` |
| Step 5 | Conflict resolution (higher direct_connections; tie lower ID) | Yes | Implemented in `ble_engine_handle_election_packet` for candidate/clusterhead state |
| Step 6 | Re-announcement / renouncement after loss | Yes | `ble_engine_start_renouncement_rounds`, `ble_engine_prepare_renouncement_packet`, `ble_engine_send_renouncement_packet` |
| Step 7 | Edge alignment to chosen clusterhead | Partially integrated | `ble_engine_update_clusterhead_selection` uses hop-count-first ordering (Chunk 6 style rule) |

### Important Rule Separation
- Election conflict rule (Chunk 5): `direct_connections` wins; tie -> lower sender ID.
- Edge alignment rule (Chunk 6): shortest path first, then direct_connections, then lower clusterhead ID.

`ble_engine_handle_election_packet` applies the Chunk 5 conflict rule.  
`ble_engine_update_clusterhead_selection` applies the Chunk 6 edge-alignment rule.  
These are different rules and should not be conflated.

## v1 Scope Boundary (split.md Hard Constraints)
- v1 scope includes discovery/election/cluster-formation behavior.
- Full FDMA/TDMA data-plane scheduling is explicitly out of scope in v1.
- This chunk still contains a substantial data-phase subsystem (`Mode1/Mode2`, TDMA/FDMA slot gating). Treat this as ahead-of-scope implementation and do not count it as required Chunk 5 acceptance evidence.

## What This Chunk Implements
- C engine (`ble_discovery_engine.c/.h`):
  - `NOISY -> NEIGHBOR -> DISCOVERY` phase loop
  - 4-slot discovery dispatch
  - election announcement forwarding path with PDSF updates
  - renouncement flow
  - state evaluation and clusterhead selection integration
  - optional data-phase slot gating and slot-outcome counters
- ns-3 wrapper (`ble-discovery-engine.cc/.h`):
  - `TypeId` attributes and trace hooks
  - scheduler wiring (`ScheduleNextTick` -> `RunTick`)
  - packet/header conversion between ns-3 and C structs

## Runtime Flow
1. `Initialize` / `ble_engine_init`
   - Initializes queue/cycle/node/election timing, assigns self slot, starts in `NOISY`.
2. `RunTick` / `ble_engine_tick`
   - Advances data-phase windows.
   - Advances `NOISY` and `NEIGHBOR` micro-slots.
   - Executes discovery cycle slots in `DISCOVERY`.
3. `Receive` / `ble_engine_receive_packet`
   - Classifies discovery/election packet.
   - Runs election RX handling when applicable.
   - Updates noisy/neighbor sampling and queues for forwarding.
4. `ble_engine_cycle_complete`
   - Increments cycle, prunes stale neighbors, publishes metrics.
   - Calls `ble_engine_evaluate_state`.
   - Re-enters `NOISY`.

## Election Round Semantics (Current Behavior)
- `ble_engine_start_election_rounds` sets `election_rounds_remaining = 3` and sends immediately.
- `ble_engine_should_send_election` gates to at most one election send per cycle.
- `ble_engine_try_promote_clusterhead` promotes when `current_cycle > last_election_cycle_sent` and does not check `election_rounds_remaining`.

This means the implementation can promote before all 3 rounds are sent.  
Chunk 5 therefore remains **partial** for "exactly 3 rounds" until lifecycle tests prove otherwise and/or logic is tightened.

## State Machine Mapping (BleMeshNodeState)
State transitions are driven in `ble_engine_evaluate_state` using shared-node predicates (`ble_mesh_node_should_become_edge`, `ble_mesh_node_should_become_candidate`) from `model/shared/ble_mesh_node.c`.

| From | Condition | To | Function Path |
|---|---|---|---|
| `INIT` | first evaluation | `DISCOVERY` | `ble_engine_evaluate_state` |
| `DISCOVERY` or `EDGE` | candidate predicate true | `CLUSTERHEAD_CANDIDATE` | `ble_engine_evaluate_state` |
| any non-edge | edge predicate true | `EDGE` | `ble_engine_evaluate_state` |
| `CLUSTERHEAD_CANDIDATE` | stronger candidate heard | `EDGE` + renouncement rounds | `ble_engine_handle_election_packet` |
| `CLUSTERHEAD_CANDIDATE` | promotion check passes | `CLUSTERHEAD` | `ble_engine_try_promote_clusterhead` |
| `CLUSTERHEAD` | stronger peer heard | `EDGE` | `ble_engine_handle_election_packet` |
| aligned node | selected CH renounces | `DISCOVERY` | `ble_engine_handle_clusterhead_renouncement` |

### Candidacy Gate Cross-Chunk Note
- This chunk does **not** call `ch04` `ble_election_should_become_candidate`.
- It currently relies on shared-node candidacy logic (`ble_mesh_node_should_become_candidate`) in `model/shared/ble_mesh_node.c`.

## File Inventory and Key Responsibilities

### `ble_discovery_engine.h`
- Defines `ble_engine_config_t`, `ble_engine_t`, callbacks, API surface.
- Exposes constants including `BLE_ENGINE_MAX_ELECTION_ROUNDS`.

### `ble_discovery_engine.c`
- Implements election scheduling, conflict logic, renouncement flow, forwarding updates, state transitions, and phase timing.

### `ble-discovery-engine.h`
- Exposes ns-3 object API and trace interfaces.

### `ble-discovery-engine.cc`
- Maps ns-3 attributes to C config, schedules periodic ticks, and handles packet conversions.

## Key API/Function Notes

### Portable C API (selected behavior-critical functions)
- `ble_engine_reset`
  - Re-initializes node and clears slots via `ble_mesh_node_clear_slots`.
  - Does not reassign self slot (unlike `ble_engine_init`), creating init/reset asymmetry.
- `ble_engine_receive_packet`
  - For election packets, dispatches to `ble_engine_handle_election_packet`.
  - Then enqueues using `ble_queue_enqueue`.
- `ble_engine_forward_next_message`
  - Applies forwarding decision.
  - Enforces PDSF soft cap stop (`pdsf >= BLE_DISCOVERY_MAX_CLUSTER_SIZE`) for election forwarding.
  - Updates PDSF via `ble_election_update_pdsf`.
  - This is soft-cap behavior only; hard membership cap validation is not implemented here.
- `ble_engine_count_already_reached`
  - Counts direct neighbors already present in packet path.
  - This is the implementation of the PDF clause "excluding devices message has reached previously."
- `ble_engine_update_clusterhead_selection`
  - Implements edge alignment ranking: hop count, then direct_connections, then lower sender ID.
  - Treat as Chunk 6-style behavior integrated into this runtime.
- `ble_engine_handle_election_packet`
  - Marks candidate heard.
  - Handles incoming renouncement (`ble_engine_handle_clusterhead_renouncement`).
  - In candidate/clusterhead state: compares local vs remote by direct_connections then sender ID.
  - On loss: demotes to `EDGE`, cancels election rounds, starts renouncement rounds, aligns to winner.
- `ble_engine_try_promote_clusterhead`
  - Candidate promotion gate based on elapsed cycle since last election send.
  - Does not require `election_rounds_remaining == 0`.

### ns-3 Wrapper (`BleDiscoveryEngine`)
- `ScheduleNextTick` always uses `SlotDuration`.
- `ble_engine_tick` internally advances NOISY/NEIGHBOR micro-phases using independent durations.
- If `SlotDuration`, `NoiseSlotDuration`, and `NeighborSlotDuration` are not harmonized, timeline distortion can occur.

## Cross-File Integration Contracts and Hazards
- C engine requires `config.node_id != 0` and `config.send_cb != NULL`; wrapper enforces both.
- Election packet handling depends on base-layout cast compatibility between `ble_discovery_packet_t` and `ble_election_packet_t`.
- Wrapper RX classification is `header.IsElectionMessage()`. If the header mode flag drifts from wire type, wrong packet interpretation follows.
  - Known upstream hazard: `ch01/ble-discovery-header-wrapper.cc` can reassign `m_isElection` from `is_clusterhead_message` during deserialize paths.
- Renouncement correctness depends on wire flag initialization.
  - Known upstream hazard: `ch01/ble_discovery_packet.c` `ble_election_packet_init` does not explicitly initialize `is_renouncement`.
  - This chunk sets the flag explicitly for renouncement packets, but stale-memory behavior upstream can still leak incorrect renouncement state.

## Chunk 5 Conformance (Against `chunks/split.md`)

| Requirement | Status | Evidence / Gap |
|---|---|---|
| Exactly 3 election rounds | Partial | Round counter exists, but promotion gate can preempt remaining rounds. |
| Propagate/update `class_id`, `direct_connections`, `pdsf`, `last_pi`, `score`, `hash`, `path` | Implemented | Built in `ble_engine_prepare_election_packet`; updated during forwarding. |
| Conflict rule `direct_connections` then lower ID | Implemented (for candidate/clusterhead conflict) | `ble_engine_handle_election_packet`. |
| Renouncement after loss | Implemented | `ble_engine_start_renouncement_rounds` + renouncement send path. |
| PDSF soft-cap retransmission stop | Implemented | Checked pre/post PDSF update in `ble_engine_forward_next_message`. |
| Separate hard membership cap validation/reporting | Missing | No explicit hard-cap validation counters in this chunk. |
| Required metrics: `pdsf_cap_stop_events`, `cluster_size_hard_cap_violations` | Missing | Not present in this directory. |
| Deliverable naming (`BleElectionEngine`) | Satisfied via integrated runtime | Allowed by split "Component Placement and Ownership (Locked)". |

## Chunk 5 Exit Gate Checklist
From `split.md` Chunk 5 Exit Gate:

1. No unresolved `CLUSTERHEAD_CANDIDATE` after final round + 1 settle cycle.
2. In cap-stop tests, forwarded election packets after `pdsf >= cap` are zero.

Current status in `ch05` README scope:
- Not proven by local `ch05` test artifacts.
- Must be shown with deterministic test evidence before claiming Chunk 5 complete.

## Potential Issues / Risks
1. Promotion can occur before 3-round completion.
2. `ble_engine_reset` clears slots but does not reassign self slot.
3. Wrapper tick cadence (`SlotDuration`) may not match micro-slot timing (`NoiseSlotDuration`, `NeighborSlotDuration`).
4. Data-phase restart ordering can bypass intended `Mode2` wait in some timelines.
5. Data-phase subsystem is ahead-of-v1 scope and may create confusion in acceptance reviews.
6. Conflict rule and edge-alignment rule coexist in one chunk; reviewers can mis-audit if not separated.
7. RX/TX cast contract is strict; layout mismatch is unsafe.
8. Header mode-flag drift in `ch01` can misclassify packet type at wrapper receive entry.
9. Renouncement flag initialization dependency in `ch01` can create stale-flag hazards.
10. Collision model (`last-slot` local bookkeeping) is a lightweight approximation, not full multi-node collision modeling.

## Cross-Chunk Audit Notes (2026-04-08)
- `split.md` permits integrated placement; this chunk hosting election logic in `BleDiscoveryEngine` is acceptable.
- Chunk 5 soft-cap/hard-cap metric separation remains incomplete.
- State transition logic and candidacy predicate are sourced from shared node runtime (`model/shared/ble_mesh_node.*`), not directly from `ch04` election helper APIs.

## Practical Reading Order
1. `ble_discovery_engine.h` for types/config/API.
2. `ble_engine_init`, `ble_engine_tick`, `ble_engine_receive_packet` in `ble_discovery_engine.c`.
3. `ble_engine_handle_election_packet`, `ble_engine_forward_next_message`, and election/renouncement helpers.
4. `ble-discovery-engine.cc` for wrapper timing and packet conversion.
