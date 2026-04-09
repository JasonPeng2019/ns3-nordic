# BLE Mesh Discovery + Clusterhead Election Project Split (ns-3)

## 0. Document Control

- Specification ID: `ble-mesh-discovery-v1-plan`
- Status: `active`
- Last updated: `2026-04-05`
- Protocol source of truth: `Clusterhead___BLE_Mesh_discovery_process (5) (2).pdf`
- Local implementation target path: `ns3_dev/ns3-ble-module/ble-mesh-discovery`
- Upstream mirror target path (when upstreaming): `ns-3-dev/src/ble-mesh-discovery`

## 1. Goal and Constraints

### Goal
Implement and validate the BLE Mesh Discovery + Clusterhead Election protocol from the PDF so it scales from small to large networks while preserving practical latency and cluster-size control.

### Hard Constraints
- Primary implementation target for this repo: `ns3_dev/ns3-ble-module/ble-mesh-discovery`.
- If code is moved upstream, mirror to: `ns-3-dev/src/ble-mesh-discovery`.
- Use existing `ble` module as lower-layer PHY/MAC foundation.
- Discovery and election in v1 are **single-channel** operations.
- Keep wire format frozen to the canonical contract in Section 2.
- No new on-wire fields in v1 beyond Section 2.
- Scope only includes PDF-defined core discovery/election/cluster-formation behavior.
- Explicitly out of scope in v1:
  - Full FDMA/TDMA data-plane scheduling behavior after discovery/election.
  - Clusterhead-to-clusterhead relay/path exchange fabric.
- Progression gates are strictly sequential: **20 -> 150 -> 1000+ nodes**.

### Slot Indexing Convention (Locked)
- Protocol-language slots are 1-based:
  - `S1`: own discovery transmission.
  - `S2-S4`: forwarding opportunities.
- Implementation-language slots are 0-based:
  - `slot0 == S1`, `slot1 == S2`, `slot2 == S3`, `slot3 == S4`.
- All tests/docs must use both labels when ambiguity is possible.

### Determinism Scope (Locked)
- `D0` deterministic replay: same binary + same host architecture + same seed + same scenario timeline must produce bit-identical event traces and final cluster assignments.
- `D1` cross-host reproducibility: results may differ at bit level, but key metrics must stay within configured tolerances.

### Capacity Semantics (Locked)
- `PDSF cap` is a **propagation control estimate** (soft cap for retransmission).
- `Cluster membership cap` is an **actual assignment invariant** validated from converged cluster assignments.
- Both must be reported separately in metrics and gates.

### Required Public APIs / Types
- `BleMeshDiscoveryConfig`
- `BleMeshNodeState` enum: `DISCOVERY`, `EDGE`, `CLUSTERHEAD_CANDIDATE`, `CLUSTERHEAD`
- Runtime components:
  - `BleDiscoveryEngine`
  - `BleElectionEngine`
  - `BleClusterManager`
  - `BleMeshMetricsCollector`
- Scenario/helper entrypoints for install/configure/run and trace export (CSV/JSON).

### Component Placement and Ownership (Locked)
- Chunk IDs define required behavior, tests, and exit gates; they do **not** require strict file/folder ownership.
- Implementations may span `model/chunks/*` and shared support modules (for example `model/shared/*`) if:
  - Section 2 wire-contract constraints are preserved.
  - Required public APIs/types in this section remain available.
  - Chunk exit gates are still met with traceable pass/fail evidence.
- Deliverables that name a runtime class (for example `BleElectionEngine`) may be satisfied by an equivalent integrated runtime path (for example election logic hosted in `BleDiscoveryEngine`) when behavior and gates are unchanged.
- Any cross-chunk placement must be documented in the corresponding chunk README(s) with explicit implementation pointers.

## 2. Canonical v1 Wire Contract Freeze

### 2.1 Contract Version
- Wire contract ID: `ble-mesh-wire-v1.0.0`
- Effective date: `2026-03-31`
- Any field/encoding change requires version bump (`v1.x -> v2.0.0`) and migration note.

### 2.2 Discovery Message (Base) Contract

| Field | Type | Size (bytes) | Required | Notes |
|---|---|---:|---|---|
| `message_type` | `uint8` | 1 | yes | `0=DISCOVERY`, `1=ELECTION_ANNOUNCEMENT` |
| `is_clusterhead_message` | `bool/u8` | 1 | yes | Explicit clusterhead/election flag |
| `sender_id` | `uint32` | 4 | yes | Node ID of packet origin sender |
| `ttl` | `uint8` | 1 | yes | Decrement each hop, do not forward if `0` |
| `path_length` | `uint16` | 2 | yes | PSF length |
| `path` | `uint32[]` | `4 * path_length` | yes | Path So Far sequence |
| `gps_available` | `bool/u8` | 1 | yes | If false, GPS coordinates omitted |
| `gps_x` | `double` | 8 | conditional | Present only when `gps_available=1` |
| `gps_y` | `double` | 8 | conditional | Present only when `gps_available=1` |
| `gps_z` | `double` | 8 | conditional | Present only when `gps_available=1` |

### 2.3 Election Announcement Extension Contract

| Field | Type | Size (bytes) | Required | Notes |
|---|---|---:|---|---|
| `is_renouncement` (flag bit) | `uint8` | 1 | yes | Renouncement state |
| `class_id` | `uint16` | 2 | yes | Cluster class identifier |
| `direct_connections` | `uint32` | 4 | yes | Mandatory for conflict resolution |
| `pdsf` | `uint32` | 4 | yes | Predicted devices so far (soft estimate) |
| `last_pi` | `uint32` | 4 | yes | Running product term for PDSF |
| `score` | `double` | 8 | yes | Candidate quality score |
| `hash` | `uint32` | 4 | yes | Cluster hash identifier |
| `pdsf_history_hop_count` | `uint16` | 2 | yes | Number of hop contributions |
| `pdsf_history[]` | `uint32[]` | `4 * hop_count` | yes | Per-hop direct-count contributions |

### 2.4 Contract Rules
- This is the only accepted v1 wire schema.
- The phrase "no new on-wire fields" means no additions/removals/reordering/encoding changes beyond this schema.
- Conflict resolution is defined against `direct_connections`; therefore `direct_connections` is not optional.
- Renouncement is encoded only via `is_renouncement` flag and existing election payload.

## 3. Sequential Implementation Chunks (Quantitative Gates)

Each chunk is independently mergeable and has measurable pass/fail gates.

---

## Chunk 1: Protocol Contract Freeze + Baseline Harness

### Requirements
- Publish canonical wire contract table (Section 2) and lock version metadata.
- Route protocol defaults/tunables through `BleMeshDiscoveryConfig` (single source of truth).
- Add deterministic controls:
  - Seed handling for all stochastic modules.
  - Deterministic cycle/slot timing controls.
  - Independent RNG stream ownership (no hidden global shared RNG use).
- Reconcile local module layout and build wiring with active repo path and upstream mirror path.

### Deliverables
- Versioned wire contract (`ble-mesh-wire-v1.0.0`).
- Config type + defaults + mapping to C core and wrappers.
- Deterministic replay harness and reference trace artifacts.
- Build-layout note for `local path` vs `upstream path`.

### Section Test Plan
- Unit tests:
  - Discovery and election serialize/deserialize roundtrip on 1,000 randomized packets each.
  - Boundary/path length limits.
  - GPS available/unavailable behavior.
  - TTL edge cases.
- Determinism tests (`D0`):
  - 30 repeated runs with same seed and timeline produce byte-identical traces and identical final assignments.

### Exit Gate
- Packet tests pass 100%.
- `D0` determinism pass rate: 30/30.
- Contract version + schema are documented and referenced by tests.

---

## Chunk 2: Node Runtime + 4-Slot Discovery Cycle

### Requirements
- Implement per-node runtime state container with `BleMeshNodeState`.
- Implement 4-slot cycle scheduler with locked slot mapping (`S1..S4` <-> `slot0..slot3`).
- Implement RX queue and dedupe cache with explicit dedupe identity and expiry:
  - Dedupe key must avoid mutable forwarding fields (`ttl`, trailing `path` growth).
  - Dedupe key default fields:
    - `message_type`, `sender_id`, `is_clusterhead_message`,
    - `class_id`, `hash`, `is_renouncement` (election only),
    - `path[0]` (origin anchor, if present).
  - Cache expiry default: `2 * cycle_duration * initial_ttl`.
- Enforce PSF loop rejection.
- Enforce TTL decrement and expiration handling.
- Define and use explicit convergence criterion for 20-node gate.

### Deliverables
- `BleDiscoveryEngine` (or equivalent integrated runtime component) cycle/timer control.
- Queue + dedupe + loop/TTL gating logic.
- Convergence definition artifact for smoke tests.

### Section Test Plan
- Unit tests:
  - State initialization/transitions.
  - Slot budget enforcement (`<=1 own + <=3 forwarded` per cycle).
  - Dedupe hit/miss and expiry behavior.
  - PSF loop rejection correctness.
  - TTL decrement and drop at zero.
- Scenario smoke test (20-node static, fixed seed):
  - No invariant violations for 50 consecutive cycles.
  - Convergence detected by configured criterion.

### Exit Gate
- All invariants hold.
- 20-node smoke test passes with deterministic replay (`D0`).

---

## Chunk 3: Forwarding Policy Pipeline

### Requirements
- Implement forwarding decision pipeline in exact order:
  1. Picky forwarding (crowding-based probabilistic filter).
  2. GPS proximity filter (bypass when GPS unavailable).
  3. TTL sort and top-3 forwarding selection.
- Expose all policy thresholds through centralized config.
- Add structured forwarding reason telemetry:
  - `drop_ttl0`, `drop_loop`, `drop_duplicate`, `drop_crowding`, `drop_gps`, `drop_capacity`, `forwarded`.

### Deliverables
- Deterministic forwarding pipeline integrated into discovery cycle.
- Telemetry counters + trace output.

### Section Test Plan
- Unit tests per filter.
- Combined deterministic queue-selection tests.
- Density sweep (at least sparse/medium/dense, 20 seeds each):
  - Compare with no-picky baseline.
  - Use confidence intervals for stochastic checks.

### Exit Gate
- Filter and integration tests pass 100%.
- At high density: control overhead reduced by at least 30% versus no-picky baseline.
- Reachability in high density remains at least 90% of baseline.

---

## Chunk 4: Connectivity Metrics + Candidacy Decision

### Requirements
- Implement noisy broadcast/listen phase for local crowding sensing.
- Track candidacy metrics:
  - `direct_neighbors`
  - `unique_paths`
  - `geographic_distribution`
  - `forwarding_success_rate`
  - `connection_noise_ratio`
- Lock formulas:
  - `connection_noise_ratio = direct_neighbors / (1.0 + crowding_factor)`
  - `forwarding_success_rate = forwarded / max(1, received)`
  - `geographic_distribution`: 8-sector occupancy ratio around node (`occupied_sectors / 8.0`)
  - `unique_paths`: cardinality of distinct path signatures over sliding window
    - default signature: `(path[0], path[path_length-1], path_length_bucket)`
- Lock score function:
  - `score = w1*norm(direct_neighbors) + w2*norm(connection_noise_ratio) + w3*geographic_distribution + w4*forwarding_success_rate + w5*norm(unique_paths)`
  - default weights: `w1=0.30, w2=0.30, w3=0.15, w4=0.15, w5=0.10`.
- Lock dynamic candidacy threshold:
  - `min_direct = clamp(round(base_min_direct + crowding_factor * crowding_slope), min_direct_floor, min_direct_ceil)`.

### Deliverables
- `BleElectionEngine` metric collector and candidacy evaluator (or equivalent integrated election runtime path).
- Candidate transition path `DISCOVERY -> CLUSTERHEAD_CANDIDATE` (may be realized through shared node-runtime state machine integration).

### Section Test Plan
- Unit tests for all metric functions and threshold logic.
- Topology tests (clustered vs uniform vs sparse).

### Exit Gate
- For fixed seed (`D0`), candidate sets are identical across 30 runs.
- Clustered vs uniform scenarios produce statistically different candidacy distributions (`p < 0.05`).

---

## Chunk 5: Election Announcement (3 Rounds) + Conflict Logic

### Requirements
- Implement election announcements over exactly 3 rounds.
- Propagate and update election payload:
  - `class_id`, `direct_connections`, `pdsf`, `last_pi`, `score`, `hash`, `path`.
- Capacity behavior:
  - Enforce `PDSF` soft cap for retransmission stop.
  - Track/report actual converged cluster size separately for hard-cap validation.
- Conflict resolution:
  - Higher `direct_connections` wins.
  - Tie -> lower `node_id` wins.
- Implement candidacy renouncement broadcasts after loss.

### Deliverables
- Round scheduler and flood logic in election runtime (`BleElectionEngine` and/or integrated `BleDiscoveryEngine` path).
- Conflict resolver + renouncement path.
- Separate metrics for `pdsf_cap_stop_events` and `cluster_size_hard_cap_violations`.

### Section Test Plan
- Unit tests:
  - Exactly 3 rounds.
  - Conflict/tie-break correctness.
  - PDSF cap stop behavior.
- Scenario tests:
  - Overlapping candidates converge deterministically.
  - Renouncement cleans stale candidates.

### Exit Gate
- No unresolved `CLUSTERHEAD_CANDIDATE` after final round + 1 settle cycle.
- In cap-stop tests, forwarded election packets after `pdsf >= cap` are zero.

---

## Chunk 6: Cluster Formation + Path Management (Scalable Bounds)

### Requirements
- Implement edge-node alignment rule:
  - Prefer shortest path.
  - If equal, prefer highest direct-count source.
  - If still equal, lower clusterhead ID.
- Maintain feasible paths with explicit bounds:
  - `max_paths_per_edge_per_clusterhead` default `4`.
  - `max_clusterheads_tracked_per_edge` default `3`.
- Run Dijkstra on discovered graph for shortest feasible route insertion.
- Build loop-free routing tree entries per edge.
- Recompute policy:
  - Event-driven with debounce (`>=5 cycles` between recomputes unless >10% topology delta).

### Deliverables
- `BleClusterManager` assignment tables + bounded path store.
- Graph builder + Dijkstra integration + loop checks.

### Section Test Plan
- Unit tests:
  - Path ranking/tie-break.
  - Dijkstra correctness.
  - Bounds enforcement.
  - Loop-freedom checks.
- Integration tests:
  - Multipath/redundant links.
  - Reassignment when better path appears.

### Exit Gate
- 100% reachable edge nodes assigned.
- Loop count in resulting trees is zero.
- Path storage/recompute bounds are never violated in 20/150/1000 profiles.

---

## Chunk 7: Scenario + Instrumentation for Scale

### Requirements
- Provide reusable scenario runner/helper presets:
  - `20-node`, `150-node`, `1000+-node`.
  - static and mobility modes.
- Implement `BleMeshMetricsCollector` exports (CSV and JSON).
- Define metric sources and formulas:
  - Convergence time (cycles and seconds).
  - End-to-end latency and PDR from synthetic data-plane traffic.
  - Control overhead.
  - Cluster size distribution.
  - Invariant counters.
- Add explicit synthetic traffic model for E2E/PDR:
  - default: each edge emits `1 packet/sec` toward selected clusterhead after convergence.

### Deliverables
- Scenario entrypoints + standardized artifact schema.
- Repeatable benchmark scripts/profiles.

### Section Test Plan
- Reproducibility:
  - `D0` for static profiles.
  - `D1` tolerance checks for mobility.
- Regression:
  - 20-node and 150-node baseline runs.
- Large-scale:
  - 1000+ run with complete metrics and invariant reporting.

### Exit Gate
- All profiles emit parseable CSV+JSON artifacts.
- 1000+ scenario completes with zero fatal invariant breaks.

---

## Chunk 8: End-to-End Validation + Parameter Tuning

### Requirements
- Run structured parameter sweeps for:
  - Crowding-forwarding behavior.
  - GPS proximity threshold.
  - Candidacy thresholds/weights.
  - Path bounds/recompute debounce.
- Lock stable default profiles per scale tier.
- Publish limitations and recommended run commands.
- Use predeclared acceptance criteria to avoid seed/topology overfitting.

### Deliverables
- Tuned config profiles (`small-20`, `mid-150`, `large-1000`).
- Validation report + runbook.

### Section Test Plan
- Acceptance matrix on `20 -> 150 -> 1000+` with fixed seed suites.
- Compare tuned vs baseline metrics with confidence intervals.

### Exit Gate
- Tuned profiles meet acceptance thresholds:
  - Overhead improvement >= 15% vs baseline at equal or better convergence.
  - Reachability/PDR non-regression within configured floors.
  - Latency remains in seconds-scale target.

## 4. Status-Based Gap Checklist (Repo Snapshot: 2026-03-31)

Status labels: `[DONE]`, `[PARTIAL]`, `[MISSING]`.

### Chunk 1 Status: PARTIAL
- `[DONE]` Public `BleMeshDiscoveryConfig` type exists.
- `[PARTIAL]` Defaults/tunables are not fully centralized across C core + wrappers.
- `[MISSING]` Versioned canonical wire-contract lock metadata tied to tests.
- `[MISSING]` Deterministic replay evidence for `D0` across repeated runs.

### Chunk 2 Status: PARTIAL
- `[PARTIAL]` 4-slot scheduler, queue, PSF loop/TTL logic exist.
- `[MISSING]` Convergence criterion codified and enforced in CI gate.
- `[MISSING]` Dedupe identity/expiry semantics are explicitly specified and tested at scale.

### Chunk 3 Status: PARTIAL
- `[PARTIAL]` Forwarding pipeline primitives exist.
- `[MISSING]` Full config centralization of policy parameters.
- `[MISSING]` Forwarding reason telemetry counters/traces.
- `[MISSING]` Density sweep with quantitative reachability/overhead thresholds.

### Chunk 4 Status: PARTIAL
- `[PARTIAL]` Crowding/direct/geographic metrics exist.
- `[MISSING]` Explicit `unique_paths` metric implementation.
- `[PARTIAL]` Candidacy score/threshold formulas are not fully aligned with locked formula set.
- `[MISSING]` Automated topology-distribution statistical tests.

### Chunk 5 Status: PARTIAL
- `[PARTIAL]` Election rounds/conflict/renouncement paths exist in engine logic.
- `[MISSING]` Exact 3-round lifecycle CI tests and unresolved-candidate end-state assertions.
- `[MISSING]` Explicit separation and validation of `PDSF soft cap` vs `hard membership cap`.

### Chunk 6 Status: PARTIAL
- `[DONE]` `BleClusterManager` scaffold exists.
- `[MISSING]` Bounded multipath memory + graph + Dijkstra + loop-free tree implementation.
- `[MISSING]` Recompute debounce and scale-bound enforcement tests.

### Chunk 7 Status: PARTIAL
- `[DONE]` `BleMeshMetricsCollector` scaffold exists.
- `[MISSING]` Full CSV/JSON schema implementation and scale scenario presets.
- `[MISSING]` Synthetic data-plane traffic for E2E latency/PDR measurement.

### Chunk 8 Status: PARTIAL
- `[PARTIAL]` Profile lock-point exists (`default-profiles.md`).
- `[MISSING]` Structured sweep execution, tuned profile locking, and acceptance artifacts.

### Cross-Section Verification Status: MISSING
- `[MISSING]` Consolidated pass/fail evidence for all chunk exit gates on active host architecture.

## 5. Cross-Section Integration Plan

### Integration Order
1. Integrate Chunks 1-3 into end-to-end discovery.
2. Integrate Chunks 4-5 for election completion and conflict closure.
3. Integrate Chunk 6 for assignment/path outputs.
4. Integrate Chunk 7 instrumentation across all prior flows.
5. Finalize with Chunk 8 tuning and acceptance reruns.

### Cross-Section Invariants (must hold at every stage)
- No forwarding when `ttl == 0`.
- Max 3 forwarded messages per cycle.
- No PSF loops in forwarded traffic.
- Discovery/election use single-channel operation in v1.
- `PDSF soft cap` retransmission stop is enforced.
- `Hard membership cap` violations are tracked and must be zero at acceptance.
- `D0` determinism holds for fixed static scenarios.

### Quantitative Acceptance Matrix (locked for CI)

| Profile | Convergence Bound | P95 E2E Latency Bound | PDR Floor | Hard Cap Violations |
|---|---:|---:|---:|---:|
| 20-node static | <= 20 cycles | <= 2.0 s | >= 0.98 | 0 |
| 150-node static | <= 75 cycles | <= 5.0 s | >= 0.95 | 0 |
| 1000+-node static | <= 220 cycles | <= 8.0 s | >= 0.90 | 0 |

### Reproducibility Criteria
- `D0`: identical traces and final assignments for repeated fixed-seed static runs.
- `D1`: mobility metrics (`convergence`, `PDR`, `overhead`) remain within +/-5% across supported host architectures.

## 6. Definition of Done

Project is complete when all conditions below are met:
- All eight chunks are implemented and pass quantitative exit gates.
- Canonical wire contract `ble-mesh-wire-v1.0.0` is locked and test-enforced.
- Full integration matrix passes across 20, 150, and 1000+ profiles.
- Tuned defaults are locked and documented for each scale tier.
- Scenario runner and metrics exports are reproducible.
- Limitations and deferred v2 items are explicitly documented.

## 7. Notes for v2 (Deferred)

The following remain intentionally deferred from v1:
- Full FDMA/TDMA scheduling implementation for post-discovery data plane.
- Clusterhead-to-clusterhead relay/path exchange fabric.
- Any wire-format changes beyond `ble-mesh-wire-v1.0.0`.
- Optional multi-channel discovery/election variants.
