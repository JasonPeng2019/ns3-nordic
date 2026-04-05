# ch03: BLE Forwarding Logic (C Core + ns-3 C++ Wrapper)

## Scope
This README documents only files in `model/chunks/ch03`:

- `ble_forwarding_logic.h`
- `ble_forwarding_logic.c`
- `ble-forwarding-logic.h`
- `ble-forwarding-logic.cc`

The chunk implements BLE mesh discovery forwarding decisions using three metrics:

1. Crowding-aware probabilistic forwarding ("picky forwarding")
2. GPS proximity filtering
3. TTL gating and priority mapping

## File Roles

| File | Role |
|---|---|
| `ble_forwarding_logic.h` | Public C API for forwarding logic (portable core interface). |
| `ble_forwarding_logic.c` | Pure C implementation of the forwarding algorithm and helpers. |
| `ble-forwarding-logic.h` | ns-3 `Object` wrapper API around the C core. |
| `ble-forwarding-logic.cc` | ns-3 wrapper implementation, attributes, logging, RNG integration behavior. |

## Architecture

- The C layer is the protocol engine and owns core forwarding behavior.
- The C++ layer is an adapter for ns-3 (`Vector`, `Object`, `TypeId`, attributes, logging).
- Most C++ methods pass data directly into the C API after light conversion.
- One important exception: `ShouldForwardCrowding(double,uint32_t)` in C++ reimplements the probability calculation when `m_randomStream` is set (instead of delegating to C RNG).

## Forwarding Decision Pipeline

`ble_forwarding_should_forward(...)` in C is the canonical full decision path:

1. Reject null packet.
2. Reject `ttl == 0`.
3. Run crowding probability gate.
4. If packet GPS is available and current location is provided, apply distance threshold gate.
5. Forward only if all checks pass.

Priority is separate (`ble_forwarding_calculate_priority`):

- `ttl == 0` -> `255` (lowest priority)
- otherwise `priority = 255 - ttl` (higher TTL => smaller number => higher priority)

## Conformance to `split.md` Chunk 3

Reference plan: `model/chunks/split.md` (Chunk 3: Forwarding Policy Pipeline, last updated 2026-03-31).

Overall verdict: **partial fit**. This chunk implements key forwarding primitives, but it does not yet satisfy full Chunk 3 integration and gate requirements.

| Planned requirement (Chunk 3) | Current ch03 status | Notes |
|---|---|---|
| Picky forwarding filter | Partial | Implemented in C core and wrapper, but RNG behavior differs by API path and uses global C RNG state by default. |
| GPS proximity filter (bypass when GPS unavailable) | Implemented | Present and functionally matches bypass behavior. |
| TTL sort + top-3 forwarding selection | Missing | Only TTL==0 drop and TTL->priority mapping are present; no queue sorting/top-3 selection logic in this chunk. |
| All policy thresholds centralized in config | Missing | Proximity/default-neighbor are exposed in wrapper attributes; core thresholds are hardcoded (`RSSI_MIN/MAX`, crowding cutoffs, `2/neighbors`). |
| Structured forwarding reason telemetry (`drop_*`, `forwarded`) | Missing | No structured counters/trace schema in this chunk; only log statements. |
| Deterministic forwarding pipeline integrated into discovery cycle | Partial | Decision primitives exist, but cycle-level deterministic integration/selection is not implemented in this folder. |
| Density sweep + quantitative gate evidence | Missing | No test artifacts in this chunk for 30% overhead reduction / >=90% reachability floor checks. |

### Main Deviations

1. Missing slot-capacity behavior required by Chunk 3 (`TTL sort and top-3 forwarding selection`).
2. Missing structured telemetry reason counters/traces required by Chunk 3.
3. Policy parameters are not fully centralized/config-driven.
4. Determinism model conflicts with plan guidance due to shared global RNG state in C core.

## Prerequisites (Embedded Deployment Requirements)

The following are strict requirements to treat this chunk as production-ready for embedded BLE mesh firmware.

### Required Protocol/Behavior Compliance

1. Enforce forwarding pipeline order exactly as planned:
- Crowding filter -> GPS filter -> TTL ranking + top-3 forwarded-per-cycle cap.
2. Enforce invariants at runtime:
- Never forward with `ttl == 0`.
- Never exceed `3` forwarded messages per cycle.
- Reject PSF loops before forwarding.
3. Emit mandatory forwarding reason telemetry counters:
- `drop_ttl0`, `drop_loop`, `drop_duplicate`, `drop_crowding`, `drop_gps`, `drop_capacity`, `forwarded`.

### Required Determinism and RNG Design

1. Remove/avoid process-global RNG for forwarding decisions.
2. Use per-node RNG state ownership (or per-instance stream injection) with explicit seed control from centralized config.
3. Demonstrate `D0` deterministic replay for fixed seed/timeline on target host architecture.

### Required Configuration Discipline

1. Move all forwarding policy constants to centralized config:
- RSSI normalization bounds, crowding thresholds, neighbor-scaling constant, proximity threshold, and selection cap.
2. Keep C core behavior driven by config inputs, not hidden compile-time literals.
3. Ensure wrapper and C core consume the same config values to prevent behavior drift.

### Required Embedded Runtime Constraints

1. Bound memory and runtime in hot path:
- No unbounded dynamic allocation in forwarding decision/selection path.
- Bounded queue lengths and deterministic worst-case processing per cycle.
2. Define and enforce execution budget for a 4-slot cycle on target MCU class.
3. If target lacks hardware FPU, replace or gate double-precision math with fixed-point or validated float profile.

### Required Robustness and Safety Practices

1. Make forwarding code re-entrant or explicitly single-thread confined; no hidden shared mutable state across nodes/tasks.
2. Validate all external inputs (null pointers, malformed packet fields, pathological thresholds).
3. Compile with strict warnings and static analysis as blocking gates:
- `-Wall -Wextra -Wpedantic -Werror`
- C/C++ static analysis (`clang-tidy`, `cppcheck`, or equivalent)
- Sanitizers in host tests (`ASan/UBSan`) before firmware release.

### Required Verification Before Device Rollout

1. Unit tests per filter and combined selection logic.
2. Deterministic queue-selection tests (fixed seeds).
3. Density sweep (sparse/medium/dense, >=20 seeds each) with statistical reporting.
4. Quantitative acceptance checks from plan:
- High-density control overhead reduction >= 30% vs no-picky baseline.
- High-density reachability >= 90% of baseline.

## Compile-Time Dependencies (Per File)

## `ble_forwarding_logic.h`

- `<stdint.h>`
- `<stdbool.h>`
- `ble_discovery_packet.h`
- `extern "C"` guards for C++ compatibility

Exports the C API signatures and C data dependency on:

- `ble_discovery_packet_t`
- `ble_gps_location_t`

## `ble_forwarding_logic.c`

- Includes local C header: `ble_forwarding_logic.h`
- `<math.h>` for `sqrt`
- `<limits.h>` for `UINT32_MAX`

Internal dependencies:

- Static global RNG state `g_forwarding_rng_state`
- Internal helper functions:
  - `ble_forwarding_random_value()`
  - `calculate_mean_rssi(...)`

## `ble-forwarding-logic.h`

- `ns3/object.h`
- `ns3/vector.h`
- `ble-discovery-header-wrapper.h`
- `ns3/random-variable-stream.h`
- C API bridge:
  - `extern "C" { #include "ns3/ble_forwarding_logic.h" }`

Declares `ns3::BleForwardingLogic : public Object` and wrapper methods.

## `ble-forwarding-logic.cc`

- Local wrapper header: `ble-forwarding-logic.h`
- ns-3 logging and attribute helpers:
  - `ns3/log.h`
  - `ns3/double.h`
  - `ns3/uinteger.h`
- STL:
  - `<algorithm>` (used for `std::max`, `std::min`)
  - `<limits>` (included, not used)

Runtime dependencies across wrapper methods:

- C API functions from `ble_forwarding_logic.c`
- `BleDiscoveryHeaderWrapper::GetCPacket()` and `GetTtl()`
- Optional `RandomVariableStream` instance

## Full Function Inventory and Behavior

## C Header/API: `ble_forwarding_logic.h`

### `void ble_forwarding_set_random_seed(uint32_t seed)`
- Purpose: Seed internal C RNG.
- Behavior: `seed == 0` resets to built-in default constant; otherwise uses provided seed.

### `double ble_forwarding_calculate_crowding_factor(const int8_t *rssi_samples, uint32_t num_samples)`
- Purpose: Convert RSSI sample set into crowding factor [0.0, 1.0].
- Behavior:
  - Null/empty -> `0.0`
  - Mean RSSI normalized linearly between -90 dBm (0.0) and -40 dBm (1.0)
  - Values outside bounds saturate at 0 or 1

### `double ble_forwarding_calculate_noise_level(const int8_t *rssi_samples, uint32_t num_samples)`
- Purpose: Crowding factor scaled to [0, 100].
- Behavior: `noise = crowding * 100`.

### `bool ble_forwarding_should_forward_crowding(double crowding_factor, uint32_t direct_neighbors)`
- Purpose: Probabilistic forward/drop decision from crowding + neighbor count.
- Behavior:
  - Clamps crowding to [0,1]
  - Uses `neighbors = max(direct_neighbors,1)`
  - Base probability in crowded regime: `min(1, 2/neighbors)`
  - Piecewise interpolation:
    - crowding <= 0.1 -> probability 1.0
    - crowding >= 0.9 -> probability base
    - in between -> linear interpolation from 1.0 down to base
  - Compares against RNG sample

### `double ble_forwarding_calculate_distance(const ble_gps_location_t *loc1, const ble_gps_location_t *loc2)`
- Purpose: 3D Euclidean distance.
- Behavior: Null pointer input returns `0.0`.

### `bool ble_forwarding_should_forward_proximity(const ble_gps_location_t *current_location, const ble_gps_location_t *last_hop_location, double proximity_threshold)`
- Purpose: GPS distance gate.
- Behavior:
  - If either pointer missing -> returns `true` (skip proximity filtering)
  - Otherwise forwards when `distance > threshold`

### `bool ble_forwarding_should_forward(const ble_discovery_packet_t *packet, const ble_gps_location_t *current_location, double crowding_factor, double proximity_threshold, uint32_t direct_neighbors)`
- Purpose: Full 3-metric decision.
- Behavior:
  - Rejects null packet
  - Rejects TTL 0
  - Applies crowding decision
  - Applies proximity only when packet marks GPS available and current location pointer is non-null

### `uint8_t ble_forwarding_calculate_priority(uint8_t ttl)`
- Purpose: Convert TTL to queue/scheduling priority metric.
- Behavior: `ttl==0 => 255`, else `255-ttl`.

## C Source Internals: `ble_forwarding_logic.c`

### `static double ble_forwarding_random_value(void)`
- Internal xorshift32 RNG step.
- Updates global state and returns normalized sample in roughly [0,1].

### `static double calculate_mean_rssi(const int8_t *samples, uint32_t count)`
- Internal helper for average RSSI.
- Returns `0.0` on null/empty.

## C++ Wrapper API: `ble-forwarding-logic.h` / `.cc`

### `static TypeId BleForwardingLogic::GetTypeId(void)`
- Registers class with ns-3 type system.
- Adds attributes:
  - `ProximityThreshold` default `10.0`, checker `>= 0.0`
  - `DefaultDirectNeighbors` default `20`, checker `>= 1`

### `BleForwardingLogic::BleForwardingLogic()` / `~BleForwardingLogic()`
- Constructor initializes `m_proximityThreshold` to `10.0`.
- Both methods log via `NS_LOG_FUNCTION`.

### `double CalculateCrowdingFactor(const std::vector<int8_t>& rssiSamples) const`
- Delegates to C crowding function.
- Returns `0.0` for empty vectors.

### `bool ShouldForwardCrowding(double crowdingFactor, uint32_t directNeighbors)`
- Two execution paths:
  - If `m_randomStream` exists: performs local probability math in C++ and uses `m_randomStream->GetValue()`.
  - Else: calls C `ble_forwarding_should_forward_crowding(...)`.

### `bool ShouldForwardCrowding(double crowdingFactor)`
- Legacy overload using `m_defaultNeighbors`.

### `double CalculateDistance(Vector loc1, Vector loc2) const`
- Converts `ns3::Vector` to `ble_gps_location_t` and calls C distance function.

### `bool ShouldForwardProximity(Vector currentLocation, Vector lastHopLocation, double proximityThreshold) const`
- Converts vectors, calls C proximity function, recomputes distance for logging.

### `bool ShouldForward(const BleDiscoveryHeaderWrapper& header, Vector currentLocation, double crowdingFactor, double proximityThreshold, uint32_t directNeighbors)`
- Obtains C packet via `header.GetCPacket()`.
- Converts `currentLocation`.
- Delegates to C full decision function.

### `bool ShouldForward(const BleDiscoveryHeaderWrapper& header, Vector currentLocation, double crowdingFactor, double proximityThreshold)`
- Legacy overload using `m_defaultNeighbors`.

### `uint8_t CalculatePriority(const BleDiscoveryHeaderWrapper& header) const`
- Uses `header.GetTtl()` and delegates to C priority function.

### `void SetProximityThreshold(double threshold)` / `double GetProximityThreshold() const`
- Simple setter/getter for wrapper member.
- Setter does not enforce the non-negative checker itself.

### `void SeedRandom(uint32_t seed)`
- Directly seeds C RNG.

### `void SetRandomStream(Ptr<RandomVariableStream> stream)`
- Stores stream pointer.
- If non-null, draws one integer from the stream and seeds C RNG with it (forcing seed 1 when integer is 0).

## Function-Level Dependency Map

This table lists direct dependencies (functions called, global state used, or external objects consulted) for each function.

| Function | Direct dependencies |
|---|---|
| `ble_forwarding_set_random_seed` | Writes `g_forwarding_rng_state` |
| `ble_forwarding_calculate_crowding_factor` | `calculate_mean_rssi` |
| `ble_forwarding_calculate_noise_level` | `ble_forwarding_calculate_crowding_factor` |
| `ble_forwarding_should_forward_crowding` | `ble_forwarding_random_value`, `g_forwarding_rng_state` (via RNG) |
| `ble_forwarding_calculate_distance` | `sqrt` from `<math.h>` |
| `ble_forwarding_should_forward_proximity` | `ble_forwarding_calculate_distance` |
| `ble_forwarding_should_forward` | `ble_forwarding_should_forward_crowding`, `ble_forwarding_should_forward_proximity`, packet fields (`ttl`, `gps_available`, `gps_location`) |
| `ble_forwarding_calculate_priority` | None (pure arithmetic) |
| `ble_forwarding_random_value` (static) | `g_forwarding_rng_state`, `UINT32_MAX` |
| `calculate_mean_rssi` (static) | None (array iteration) |
| `BleForwardingLogic::GetTypeId` | ns-3 `TypeId`, `DoubleValue`, `UintegerValue`, accessor/checker builders |
| `BleForwardingLogic::BleForwardingLogic` | Initializes `m_proximityThreshold`; `NS_LOG_FUNCTION` |
| `BleForwardingLogic::~BleForwardingLogic` | `NS_LOG_FUNCTION` |
| `BleForwardingLogic::CalculateCrowdingFactor` | `ble_forwarding_calculate_crowding_factor`, `NS_LOG_*` |
| `BleForwardingLogic::ShouldForwardCrowding(double,uint32_t)` | Either `RandomVariableStream::GetValue` + local probability math, or `ble_forwarding_should_forward_crowding`; `NS_LOG_*`; `std::max/min` |
| `BleForwardingLogic::ShouldForwardCrowding(double)` | `BleForwardingLogic::ShouldForwardCrowding(double,uint32_t)` using `m_defaultNeighbors` |
| `BleForwardingLogic::CalculateDistance` | `ble_forwarding_calculate_distance` after `Vector -> ble_gps_location_t` conversion |
| `BleForwardingLogic::ShouldForwardProximity` | `ble_forwarding_should_forward_proximity`, `ble_forwarding_calculate_distance`, `NS_LOG_*` |
| `BleForwardingLogic::ShouldForward(...,uint32_t)` | `BleDiscoveryHeaderWrapper::GetCPacket`, `ble_forwarding_should_forward`, `NS_LOG_*` |
| `BleForwardingLogic::ShouldForward(...)` overload | `BleForwardingLogic::ShouldForward(...,uint32_t)` using `m_defaultNeighbors` |
| `BleForwardingLogic::CalculatePriority` | `BleDiscoveryHeaderWrapper::GetTtl`, `ble_forwarding_calculate_priority` |
| `BleForwardingLogic::SetProximityThreshold` | Writes `m_proximityThreshold`; `NS_LOG_FUNCTION` |
| `BleForwardingLogic::GetProximityThreshold` | Reads `m_proximityThreshold` |
| `BleForwardingLogic::SeedRandom` | `ble_forwarding_set_random_seed` |
| `BleForwardingLogic::SetRandomStream` | Stores `m_randomStream`; `RandomVariableStream::GetInteger`; `ble_forwarding_set_random_seed` |

## Call Graph (Key Paths)

- `BleForwardingLogic::ShouldForward(...)`
  -> `ble_forwarding_should_forward(...)`
  -> `ble_forwarding_should_forward_crowding(...)`
  -> `ble_forwarding_random_value()`
  -> optional `ble_forwarding_should_forward_proximity(...)`
  -> `ble_forwarding_calculate_distance(...)`

- `BleForwardingLogic::ShouldForwardCrowding(...)`
  - with `m_randomStream`: C++ local math + `RandomVariableStream::GetValue()`
  - without `m_randomStream`: `ble_forwarding_should_forward_crowding(...)`

- `BleForwardingLogic::CalculatePriority(...)`
  -> `ble_forwarding_calculate_priority(...)`

- `BleForwardingLogic::CalculateDistance(...)`
  -> `ble_forwarding_calculate_distance(...)`

## Highlights

- Strong separation of concerns: protocol logic in C, simulator integration in C++.
- C core is portable and free from ns-3/C++ STL dependencies.
- Decision rules are explicit and easy to reason about (piecewise crowding probability, deterministic TTL mapping).
- Wrapper supports ns-3 attribute system for tuning (`ProximityThreshold`, `DefaultDirectNeighbors`).
- Wrapper provides RNG hook (`RandomVariableStream`) for ns-3-aligned randomness.

## Potential Issues / Risks

1. Global C RNG state is process-global, not per-instance.
- `g_forwarding_rng_state` is static in C; all users share one stream.
- Can create coupling across nodes/instances and is not thread-safe.

2. RNG behavior differs by entry point in C++ wrapper.
- `ShouldForwardCrowding(...)` can use ns-3 RNG directly when `m_randomStream` is set.
- `ShouldForward(...)` still delegates to C full decision path, which uses C RNG state.
- Result: random source differs depending on method used.

3. Algorithm duplication in C++ for crowding path.
- Probability math is duplicated in wrapper (`ShouldForwardCrowding`) and C core.
- Future changes in C logic could drift from C++ copy.

4. Header include hygiene risk.
- `ble-forwarding-logic.h` uses `std::vector<int8_t>` but does not directly include `<vector>`.
- Build may rely on transitive includes.

5. Threshold validation gap in direct setter.
- `SetProximityThreshold(double)` writes value directly with no local clamp/check.
- Negative values are prevented only via ns-3 attribute checker, not direct API calls.

6. Proximity gate bypass behavior is permissive.
- C proximity function returns `true` when any location pointer is null.
- Full decision also skips proximity when current location pointer is absent.
- This is intentional fallback behavior but can forward with incomplete location data.

7. Boundary semantics may surprise users.
- Proximity forwarding requires `distance > threshold` (not `>=`).
- At exact threshold, packet is dropped.

8. Minor API/doc consistency issue.
- Comment in `ble_forwarding_logic.h` for noise-level function says “single RSSI reading” while signature uses sample array.

9. Minor cleanup opportunity.
- `<limits>` is included in `ble-forwarding-logic.cc` but unused.

## Practical Notes for Maintenance

- If forwarding probability policy changes, update both:
  - C core (`ble_forwarding_should_forward_crowding`)
  - C++ RNG override path (`BleForwardingLogic::ShouldForwardCrowding`)
- Prefer consolidating RNG policy so all decision entry points use one random source model.
- Decide explicitly whether null-location behavior should fail-open (current) or fail-closed.
- Add/keep tests that compare wrapper crowding decisions against C decisions for identical RNG streams.
