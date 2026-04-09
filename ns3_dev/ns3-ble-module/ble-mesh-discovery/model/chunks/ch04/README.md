# ch04 Overview: Broadcast Timing + Election Engine

This chunk contains two protocol subsystems, each implemented as a pure C core plus an NS-3 C++ wrapper:

- Broadcast timing (Task 12 and Task 14 behavior)
- Clusterhead election (Task 12-18 behavior)

Scope of this README is **only files inside `ch04/`**.

## File Inventory

- `ble_broadcast_timing.h`: C API and data model for slot scheduling.
- `ble_broadcast_timing.c`: C implementation of stochastic/noisy slot scheduling.
- `ble-broadcast-timing.h`: NS-3 C++ wrapper API for broadcast timing.
- `ble-broadcast-timing.cc`: NS-3 C++ wrapper implementation for broadcast timing.
- `ble_election.h`: C API and data model for neighbor tracking + candidacy metrics.
- `ble_election.c`: C implementation of election logic and metric computation.
- `ble-election-engine.h`: NS-3 C++ wrapper API for election engine.
- `ble-election-engine.cc`: NS-3 C++ wrapper implementation for election engine.

## Architecture and Data Flow

1. NS-3 code calls C++ wrapper objects:
   - `ns3::BleBroadcastTiming`
   - `ns3::BleElectionEngine`
2. Wrappers translate NS-3 types (`Time`, `Vector`, RNG streams) into C-core inputs.
3. C-core modules maintain protocol state and compute decisions/metrics.
4. Wrappers expose state back to NS-3 as C++ structs and return values.

The broadcast and election subsystems are separate in code (no direct function calls between the two C cores), but are conceptually coupled through crowding and clusterhead candidacy behavior.

Temporal note:
- `BleBroadcastTiming` slot scheduling is independent from `ch02` discovery-cycle slot state in this folder; timeline coordination is handled only in higher-level integrated runtime paths.

## PDF Section 3 Mapping (Election Sequence)

This chunk mostly covers PDF Section 3 steps (a)-(c), and defers later steps to subsequent chunks.

| PDF Section 3 step | `ch04` code mapping | Ownership/status |
|---|---|---|
| (a) noisy broadcast + random listen -> crowding factor `f(RSSI)` | `BleBroadcastTiming` noisy/stochastic schedule support + `ble_election_add_rssi_sample` + `ble_election_calculate_crowding` | Implemented in this chunk (with formula/config deviations noted below) |
| (b) clusterhead broadcast phase -> direct connection counting | `BleBroadcastTiming` stochastic slot behavior + `ble_election_update_neighbor` + `ble_election_count_direct_connections` | Implemented in structure, but directness classification is currently over-permissive |
| (c) candidacy from connection/noise relationship | `ble_election_should_become_candidate` + `connection_noise_ratio` metric path | Partially implemented (locked full-formula candidacy set not complete) |
| (d) candidate comparison + election announcement | Election round/conflict logic in later runtime paths | Owned by `ch05` |
| (e) cluster formation | Assignment/path management and cluster state realization | Owned by `ch06` |

## Dependencies (Per File)

### `ble_broadcast_timing.h`
- Includes: `<stdint.h>`, `<stdbool.h>`
- Exposes:
  - constants/macros (`BLE_BROADCAST_*`)
  - `ble_broadcast_schedule_type_t`
  - `ble_broadcast_timing_t`
  - public C APIs (`ble_broadcast_timing_*`)

### `ble_broadcast_timing.c`
- Includes: `"ble_broadcast_timing.h"`, `<string.h>`, `<math.h>`
- Uses:
  - `memset` (from `<string.h>`)
  - `ceil` (from `<math.h>`)
- Internal static helpers:
  - `ble_broadcast_apply_phase_defaults`
  - `ble_broadcast_clamp`
  - `ble_broadcast_compute_neighbor_tx_slots`
  - `ble_broadcast_apply_neighbor_profile`
- Public symbols consumed by wrapper: all `ble_broadcast_timing_*` functions.

### `ble-broadcast-timing.h`
- Includes: `"ns3/object.h"`, `"ns3/nstime.h"`, `"ns3/random-variable-stream.h"`, `"ns3/ble_broadcast_timing.h"`
- Depends on external ns-3 install/export of C header `ns3/ble_broadcast_timing.h`.
- Declares class `ns3::BleBroadcastTiming` with wrapper methods.

### `ble-broadcast-timing.cc`
- Includes: `"ble-broadcast-timing.h"`, `"ns3/log.h"`, `"ns3/random-variable-stream.h"`
- Uses ns-3 object/logging macros:
  - `NS_LOG_COMPONENT_DEFINE`
  - `NS_OBJECT_ENSURE_REGISTERED`
  - `NS_LOG_FUNCTION`, `NS_LOG_INFO`, `NS_LOG_DEBUG`
- Calls into C core via `ble_broadcast_timing_*` functions.

### `ble_election.h`
- Includes: `<stdint.h>`, `<stdbool.h>`, `"ble_discovery_packet.h"`
- Depends on external symbol/type provider:
  - `ble_gps_location_t` (used in neighbor/location fields)
- Exposes:
  - `BLE_MAX_NEIGHBORS`, `BLE_RSSI_BUFFER_SIZE`
  - structs `ble_election_neighbor_info_t`, `ble_connectivity_metrics_t`, `ble_election_state_t`
  - public APIs `ble_election_*`

### `ble_election.c`
- Includes: `"ble_election.h"`, `<string.h>`, `<math.h>`
- Uses:
  - `memset` (from `<string.h>`)
  - `sqrt` (from `<math.h>`)
- Calls an external function not defined in this chunk:
  - `ble_election_calculate_score(...)` (used by candidacy score path)
- Public symbols consumed by wrapper: all `ble_election_*` functions.

### `ble-election-engine.h`
- Includes: `"ns3/object.h"`, `"ns3/vector.h"`, `"ns3/nstime.h"`, `<vector>`, `<map>`, and extern C include `"ns3/ble_election.h"`
- Depends on external ns-3 export of C header `ns3/ble_election.h`.
- Declares C++ data structs `NeighborInfo`, `ConnectivityMetrics`, and wrapper class `ns3::BleElectionEngine`.

### `ble-election-engine.cc`
- Includes: `"ble-election-engine.h"`, `"ns3/log.h"`, `"ns3/simulator.h"`, `"ns3/uinteger.h"`, `"ns3/double.h"`
- Uses ns-3 object/attribute/logging APIs and simulation time.
- Calls into C core via `ble_election_*` functions.

## Function-by-Function Reference

## 1) C Broadcast Timing Core (`ble_broadcast_timing.c`)

### Internal helpers (file-local)
- `ble_broadcast_apply_phase_defaults(schedule_type, num_slots*, listen_ratio*)`
  - Applies schedule defaults when caller uses `BLE_BROADCAST_AUTO_SLOTS`/`BLE_BROADCAST_AUTO_RATIO`.
- `ble_broadcast_clamp(value, min, max)`
  - Utility clamp used by crowding/profile logic.
- `ble_broadcast_compute_neighbor_tx_slots(crowding_factor)`
  - Maps crowding `[0,1]` to TX slots in `[3,15]` (inverse relationship).
  - This `[3,15]` cap range is a local implementation policy in this chunk, not a locked value from PDF/split.
- `ble_broadcast_apply_neighbor_profile(state*)`
  - For STOCHASTIC schedule, enforces slot bounds and recomputes:
    - `max_broadcast_slots`
    - `listen_ratio = 1 - tx_slots/num_slots`

### Public API
- `ble_broadcast_timing_init(state*, schedule_type, num_slots, slot_duration_ms, listen_ratio)`
  - Zero-initializes state, resolves defaults, clamps inputs, sets retry/stat counters, and applies stochastic neighbor profile.
- `ble_broadcast_timing_set_seed(state*, seed)`
  - Sets internal LCG seed.
- `ble_broadcast_timing_rand(seed*)`
  - LCG update and return.
- `ble_broadcast_timing_rand_double(seed*)`
  - Converts LCG value to floating value by dividing by `UINT32_MAX`.
- `ble_broadcast_timing_advance_slot(state*)`
  - Advances slot index and decides broadcast/listen based on schedule + randomness.
  - STOCHASTIC mode enforces `max_broadcast_slots` per cycle via `broadcasts_this_cycle`.
- `ble_broadcast_timing_should_broadcast(state*)`
  - Returns current slot role.
- `ble_broadcast_timing_should_listen(state*)`
  - Logical inverse of broadcast role (defaults true on null state).
- `ble_broadcast_timing_record_success(state*)`
  - Increments success stats and resets retry counter.
- `ble_broadcast_timing_record_failure(state*)`
  - Increments failure stats/retry count; returns whether to retry.
- `ble_broadcast_timing_reset_retry(state*)`
  - Clears retry state and `message_sent` flag.
- `ble_broadcast_timing_get_success_rate(state*)`
  - `successful/(successful+failed)`.
- `ble_broadcast_timing_get_current_slot(state*)`
  - Returns slot index.
- `ble_broadcast_timing_get_actual_listen_ratio(state*)`
  - Returns observed listen-slot fraction from counters.
- `ble_broadcast_timing_set_crowding(state*, crowding_factor)`
  - Stores clamped crowding and reapplies STOCHASTIC neighbor profile.
- `ble_broadcast_timing_get_max_broadcast_slots(state*)`
  - Returns per-cycle broadcast cap.

## 2) NS-3 Broadcast Wrapper (`ble-broadcast-timing.cc`)

- `BleBroadcastTiming::GetTypeId()`
  - Registers ns-3 object type.
- `BleBroadcastTiming::BleBroadcastTiming()` / `~BleBroadcastTiming()`
  - Sets default `m_slotDuration`, logs lifecycle.
- `Initialize(scheduleType, numSlots, slotDuration, listenRatio)`
  - Saves `Time` and initializes C state via `ble_broadcast_timing_init`.
- `SetSeed(seed)`
  - Forwards to C-core seed setter.
- `SetRandomStream(stream)`
  - Stores optional ns-3 RNG source.
- `AdvanceSlot()`
  - If `m_rng` exists: performs manual wrapper-side slot decision.
  - Else: delegates entirely to C-core `ble_broadcast_timing_advance_slot`.
- `ShouldBroadcast()` / `ShouldListen()`
  - Forwards to C-core getters.
- `RecordSuccess()` / `RecordFailure()` / `ResetRetry()`
  - Forward to C-core retry/stat APIs.
- `SetCrowdingFactor(crowdingFactor)`
  - Forwards crowding updates to C core.
- `GetSuccessRate()` / `GetCurrentSlot()` / `GetActualListenRatio()`
  - Forwards to C-core read APIs.
- `GetSlotDuration()`
  - Returns wrapper-tracked `Time` object.
- `GetNumSlots()`
  - Reads `m_state.num_slots`.

## 3) C Election Core (`ble_election.c`)

### Public API
- `ble_election_init(state*)`
  - Clears full state and sets default candidacy thresholds.
- `ble_election_update_neighbor(state*, node_id, location*, rssi, current_time_ms)`
  - Finds/adds neighbor entry, updates location/RSSI/timestamps/message count, sets direct flag.
- `ble_election_add_rssi_sample(state*, rssi, current_time_ms)`
  - Adds sample to circular buffer only when measurement window is active.
- `ble_election_calculate_crowding(state*)`
  - Computes mean RSSI from sample buffer and maps `[-90,-40] dBm` to `[0,1]` crowding.
- `ble_election_count_direct_connections(state*)`
  - Counts neighbors with `is_direct=true`.
- `ble_election_calculate_geographic_distribution(state*)`
  - Computes centroid and distance variance for valid neighbor locations, normalizes by `std_dev/100`.
  - Valid-location filtering currently uses `(x,y,z)!=(0,0,0)` sentinel behavior rather than explicit GPS-availability semantics.
- `ble_election_update_metrics(state*)`
  - Updates direct/total/crowding/CN ratio/geographic metrics and forwarding success rate.
- `ble_election_calculate_candidacy_score(state*)`
  - Delegates to external `ble_election_calculate_score(direct_connections, crowding_factor)`.
  - External score helper lives in `ch01/ble_discovery_packet.c`, not `ch04`.
- `ble_election_reset_rssi_samples(state*)`
  - Clears RSSI circular buffer indices/count.
- `ble_election_begin_crowding_measurement(state*, window_ms)`
  - Clears RSSI buffer and enables measurement flag.
  - `window_ms` is currently ignored by C core implementation.
- `ble_election_end_crowding_measurement(state*)`
  - Finalizes crowding, caches it, updates metric, disables measurement, clears samples.
- `ble_election_is_crowding_measurement_active(state*)`
  - Returns measurement activity flag.
- `ble_election_should_become_candidate(state*)`
  - Refreshes metrics and checks configured thresholds before setting candidate state.
- `ble_election_set_thresholds(state*, min_neighbors, min_cn_ratio, min_geo_dist)`
  - Writes threshold fields in state.
- `ble_election_get_neighbor(state*, node_id)`
  - Returns pointer to neighbor struct if present.
- `ble_election_clean_old_neighbors(state*, current_time_ms, timeout_ms)`
  - Compacts neighbor array in-place and removes timed-out entries.

## 4) NS-3 Election Wrapper (`ble-election-engine.cc`)

- `BleElectionEngine::GetTypeId()`
  - Registers ns-3 object type and exposes two attributes:
    - `MinNeighborsForCandidacy`
    - `MinConnectionNoiseRatio`
- `BleElectionEngine::BleElectionEngine()` / `~BleElectionEngine()`
  - Logs lifecycle; constructor calls `ble_election_init`.
- `UpdateNeighbor(nodeId, location, rssi)`
  - Converts `Vector` to C location, uses simulator time, forwards to C core.
- `AddRssiSample(rssi)`
  - Uses simulator time, forwards sample to C core.
- `BeginCrowdingMeasurement(duration)` / `EndCrowdingMeasurement()`
  - Time conversion + direct C-core calls.
- `IsCrowdingMeasurementActive()`
  - C-core state query.
- `CalculateCrowding()`, `CountDirectConnections()`, `CalculateGeographicDistribution()`
  - C-core metric calls.
- `UpdateMetrics()` / `CalculateCandidacyScore()` / `ShouldBecomeCandidate()`
  - C-core decision pipeline.
- `GetMetrics()`
  - Copies C metrics struct into C++ `ConnectivityMetrics`.
- `GetNeighbors()`
  - Converts all C neighbor entries to `std::vector<NeighborInfo>`.
- `GetNeighbor(nodeId, info)`
  - Fetches one neighbor via C API and converts to C++ struct.
- `CleanOldNeighbors(timeout)`
  - Converts timeout/current simulator time to ms and calls C cleanup.
- `SetThresholds(minNeighbors, minCnRatio, minGeoDist)`
  - Forwards threshold changes to C core.
- `RecordMessageForwarded()` / `RecordMessageReceived()`
  - Directly increments C metric counters.
- `IsCandidate()` / `GetCandidacyScore()`
  - Direct reads from C state.

## Cross-File Integration Map

- `ble-broadcast-timing.cc` -> C core `ble_broadcast_timing.c` via `ns3/ble_broadcast_timing.h`.
- `ble-election-engine.cc` -> C core `ble_election.c` via `ns3/ble_election.h`.
- `ble_election.c` and `ble_election.h` additionally depend on external `ble_discovery_packet.h` for GPS type (and likely score helper definition or declaration).

## Conformance to `split.md` Chunk 4

Reference plan: `chunks/split.md`, Chunk 4 ("Connectivity Metrics + Candidacy Decision"), updated `2026-04-05`.

### What fits the plan

- Noisy/stochastic broadcast-listen behavior exists in `ble_broadcast_timing.*`.
- Local crowding sensing API exists (`begin/add/end crowding measurement`) in `ble_election.*`.
- Core metrics pipeline exists for:
  - `direct_neighbors` (implemented as `direct_connections`)
  - `connection_noise_ratio`
  - `geographic_distribution`
  - `forwarding_success_rate`
- `connection_noise_ratio` uses the locked formula: `direct_neighbors / (1 + crowding_factor)`.
- `BleElectionEngine` exposes collector/evaluator methods needed for candidacy decisions.

### Where it deviates from the plan

1. `unique_paths` metric is not implemented.
2. `geographic_distribution` formula is different from plan:
   - Plan: `8-sector occupancy ratio`.
   - Current code: centroid-distance variance normalized by `std_dev/100`.
3. Score function is not aligned with locked formula/weights (`w1..w5` over 5 normalized metrics).
   - Current code calls `ble_election_calculate_score(direct_connections, crowding_factor)` with only 2 inputs.
4. Dynamic candidacy threshold for `min_direct` based on crowding is not implemented.
5. `forwarding_success_rate` formula differs at `received == 0` edge:
   - Plan: `forwarded / max(1, received)`.
   - Current code: `0.0` when `received == 0`.
   - Practical nuance: these differ when `forwarded > 0 && received == 0` (possible for locally-originated traffic accounting paths), otherwise both often evaluate to `0.0`.
6. Candidate gate checks do not include all planned criteria; current gate enforces only:
   - min direct neighbors
   - min connection:noise ratio
7. `window_ms` and `current_time_ms` parameters in crowding window APIs are currently ignored, so crowding windows are event-count bounded (buffer-size bounded), not truly time bounded.
8. Planned candidate transition path `DISCOVERY -> CLUSTERHEAD_CANDIDATE` is not represented in this chunk as an explicit node-state transition; this chunk only sets `is_candidate` boolean in election state.
9. Broadcast timing policy contains unspec'd hardcoded slot-cap behavior:
   - `ble_broadcast_compute_neighbor_tx_slots` maps crowding `[0,1]` to transmit slots `[3,15]`.
   - PDF/split do not lock this exact range; this should be centralized config policy with explicit ownership.
10. Configuration surface exposed by `BleElectionEngine` is incomplete versus locked Chunk 4 formula set:
   - Wrapper attributes expose only `MinNeighborsForCandidacy` and `MinConnectionNoiseRatio`.
   - Locked formula family additionally requires tunables for weighted score (`w1..w5`), dynamic `min_direct` slope/floor/ceil behavior, and `unique_paths` window/signature controls.

### Cross-Chunk Audit Notes (2026-04-05)

- `ble_election_calculate_score(...)` is provided by `ch01/ble_discovery_packet.*`, not by files in `ch04`.
- Concrete score implementation reference: `ch01/ble_discovery_packet.c` -> `ble_election_calculate_score(uint32_t direct_connections, double noise_level)`.
- The explicit node-state transition `DISCOVERY -> CLUSTERHEAD_CANDIDATE` is implemented in the integrated engine path (`ch05` + shared `ble_mesh_node.*`), not in this chunk.
- Locked Chunk 4 formula elements (`unique_paths`, 8-sector geographic occupancy, 5-term weighted score, dynamic `min_direct`) were not found in `ch04`, `ch05`, or `model/shared` in this audit.
- Crowding normalization constants are duplicated across chunks (`ch03` forwarding and `ch04` election both use `RSSI_MIN=-90`, `RSSI_MAX=-40`) with no shared config source of truth.
- Temporal relationship between `BleBroadcastTiming` slots and `ch02` discovery-cycle slots is not defined in this folder; integration timing policy is deferred to higher-level engine orchestration.

## Implementation Highlights

- C-core protocol logic is cleanly separated from NS-3 wrappers, supporting portability.
- Both C cores use explicit state structs and null guards, making control flow easy to follow.
- Broadcast timing supports two operational profiles with adaptive crowding behavior in STOCHASTIC mode.
- Election core provides a full metrics pipeline (neighbors, crowding, geographic spread, forwarding success, candidacy gates).
- Wrappers are thin and mostly straightforward, minimizing NS-3-specific logic in core algorithms.

## Chunk 4 Exit Gate Checklist (`split.md`)

Required Chunk 4 gate conditions:

1. For fixed seed (`D0`), candidate sets are identical across 30 runs.
2. Clustered vs uniform scenarios produce statistically different candidacy distributions (`p < 0.05`).

Current folder-local evidence status:

- No folder-local artifact proving 30/30 identical candidate sets for fixed-seed (`D0`) runs.
- No folder-local statistical report showing clustered-vs-uniform candidacy distribution separation with `p < 0.05`.

## Potential Issues and Risks

1. Candidacy score depends on cross-chunk function ownership.
- `ble_election_calculate_candidacy_score()` calls `ble_election_calculate_score(...)` from `ch01`.
- This is not a missing symbol in the repo snapshot, but it is a coupling point where formula drift can occur if chunk ownership boundaries are unclear.

2. `ble_election_should_become_candidate()` only checks direct-neighbor and CN-ratio thresholds.
- Geographic threshold and forwarding-success criteria mentioned in comments are not enforced.
- `min_geographic_distribution` is stored/set but not used for candidacy gating.

3. Direct-neighbor classification is structurally incorrect for candidacy inputs.
- `ble_election_update_neighbor()` marks every updated neighbor `is_direct = true`.
- This effectively counts "all heard neighbors" as direct and can distort the direct-connections signal that feeds candidacy logic.

4. Crowding measurement APIs ignore timing parameters.
- `ble_election_begin_crowding_measurement(window_ms)` and `ble_election_add_rssi_sample(..., current_time_ms)` ignore time inputs.
- Current windowing is sample-count bounded, not truly time bounded.

5. Geographic validity uses a magic coordinate sentinel rather than explicit availability semantics.
- `(0,0,0)` is treated as invalid/no-GPS in geographic calculations.
- Wire contract already has explicit `gps_available`; filtering should use that semantics rather than coordinate-value assumptions.

6. Geographic metric semantics diverge from locked Chunk 4 formulation.
- Current metric is centroid-distance spread (then clamped to `[0,1]`), not 8-sector occupancy ratio.
- Even though clamped, threshold meaning remains formulation-specific and not directly portable from split defaults.

7. Crowding normalization constants are duplicated with `ch03`.
- Both forwarding (`ch03`) and election (`ch04`) crowding logic hardcode `RSSI_MIN=-90`, `RSSI_MAX=-40`.
- No shared config source of truth, increasing drift risk.

8. `ble_broadcast_compute_neighbor_tx_slots` encodes unspec'd policy constants.
- Crowding `[0,1]` is mapped to TX-slot cap `[3,15]` by hardcoded logic.
- This range/shape is not locked in PDF/split and should be explicit config policy.

9. `BleBroadcastTiming::AdvanceSlot()` has two code paths with behavioral drift:
   - `m_rng` path bypasses C-core STOCHASTIC constraints (`max_broadcast_slots`, `broadcasts_this_cycle`, cycle reset behavior).
   - no schedule-specific branching in wrapper RNG path.

10. Relationship between `ch04` broadcast timing slots and `ch02` discovery-cycle slots is not documented.
- Without explicit integration semantics, it's unclear whether these timelines are sequential phases, nested slots, or separate schedulers.

11. Neighbor cleanup compacts array in-place and changes indices.
- `ble_election_clean_old_neighbors` may move entries (`state->neighbors[new_count] = state->neighbors[i]`).
- Any future logic caching neighbor indices/pointers across cleanup boundaries can break.

12. New-neighbor insert path is only partially initialized.
- If `location == NULL`, location fields may retain stale data in reused slots after compaction.

13. `ble_broadcast_timing_rand_double()` can return `1.0` when RNG hits `UINT32_MAX`, despite docs saying `[0.0, 1.0)`.

14. `BleBroadcastTiming` constructor does not initialize `m_state`; calling methods before `Initialize()` risks undefined behavior.

15. Slot advancement performs modulo by `num_slots`; if `num_slots` is ever zero (mis-initialization), this is unsafe.

16. `ble-election-engine.h` includes `<map>` but does not use it (minor hygiene issue).

17. Time values are cast to `uint32_t` milliseconds in wrappers; long simulations can wrap around.

## Prerequisites (Embedded, Required)

The following are strict prerequisites for deploying this chunk on an embedded target (not optional for production-grade firmware):

1. Build partitioning MUST exclude NS-3 wrapper files (`ble-broadcast-timing.*`, `ble-election-engine.*`) from embedded firmware; only pure C core files (`ble_broadcast_timing.*`, `ble_election.*`) may ship on device.
2. Toolchain MUST compile C core with fixed language and warning policy (e.g., C11/C99, `-Wall -Wextra -Werror`) and MUST disable non-deterministic math optimizations (for example, `-ffast-math` is disallowed).
3. Numeric behavior MUST be deterministic per target profile; if no deterministic FPU path is guaranteed, fixed-point equivalents MUST be provided for crowding/geography/score math.
4. Runtime memory MUST be fully bounded and allocation-free in steady-state operation; no heap allocation is permitted in protocol hot paths.
5. Compile-time memory budget MUST be documented and enforced for `BLE_MAX_NEIGHBORS`, RSSI buffers, and all per-node state so worst-case RAM per node is known before release.
6. Timekeeping MUST use a monotonic source and overflow-safe arithmetic; production code SHOULD migrate internal time tracking from `uint32_t ms` to `uint64_t ms` (or equivalent wrap-safe epoch logic) for long uptimes.
7. RNG ownership MUST be explicit per node/module with injected seed; no shared global RNG state is permitted, and deterministic replay requirements must hold for fixed seed/timeline.
8. Concurrency model MUST be explicit; election/broadcast state updates require single-writer ownership or external synchronization (mutex/critical-section) when called from multiple contexts.
9. Input validation MUST be strict at all API boundaries (`num_slots > 0`, threshold ranges, crowding bounds, pointer validity) and invalid inputs MUST fail closed without undefined behavior.
10. Formula conformance MUST match locked Chunk 4 definitions before release:
    - `unique_paths` sliding-window metric implemented
    - `geographic_distribution` as 8-sector occupancy
    - 5-term weighted score with locked defaults (`w1=0.30`, `w2=0.30`, `w3=0.15`, `w4=0.15`, `w5=0.10`)
    - dynamic `min_direct` threshold tied to crowding
11. Verification MUST include host and target-level tests: metric unit tests, deterministic replay tests, boundary/overflow tests, and topology-shape regression tests required by Chunk 4 exit criteria.
12. Safety/compliance checks SHOULD include static analysis aligned to embedded practice (MISRA-C/CERT C policy or equivalent project standard), and all findings in protocol paths MUST be triaged before release.

## Practical Notes for Maintenance

- Prefer using C-core APIs for behavior-critical logic; keep wrappers as pure adapters.
- If ns-3 RNG customization is needed in `BleBroadcastTiming`, mirror all C-core schedule constraints in that code path or delegate randomness into the C core consistently.
- Resolve candidacy-rule drift by aligning comments, thresholds, and implementation in one place (`ble_election_should_become_candidate`).
- Confirm where `ble_election_calculate_score` is meant to come from and make linkage explicit.
