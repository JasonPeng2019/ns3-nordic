# BLE Mesh Discovery + Clusterhead Election Project Split (ns-3)

## 1. Goal and Constraints

### Goal
Implement and validate the BLE Mesh Discovery + Clusterhead Election protocol (from `Clusterhead___BLE_Mesh_discovery_process (5).pdf`) in ns-3 so it can scale from small to large networks while preserving practical latency and cluster-size control.

### Hard Constraints
- Primary implementation target: `ns-3-dev/src/ble-mesh-discovery`.
- Use existing `ble` module as lower-layer transport/PHY foundation.
- Keep current discovery/election wire format in v1 (no new on-wire fields).
- Scope only includes PDF-defined core behavior.
- Exclude theoretical out-of-scope items for v1:
  - FDMA/TDMA operation details after discovery.
  - Clusterhead-to-clusterhead relay fabric beyond cluster discovery/formation outputs.
- Progression gates must be sequential at scale: **20 -> 150 -> 1000+ nodes**.

### Required Public APIs / Types
- `BleMeshDiscoveryConfig`
- `BleMeshNodeState` enum: `DISCOVERY`, `EDGE`, `CLUSTERHEAD_CANDIDATE`, `CLUSTERHEAD`
- Runtime components:
  - `BleDiscoveryEngine`
  - `BleElectionEngine`
  - `BleClusterManager`
  - `BleMeshMetricsCollector`
- Scenario/helper entrypoints for install/configure/run and trace export (CSV/JSON).

## 2. Sequential Implementation Chunks

Each chunk below is independently mergeable and includes an explicit exit gate.

---

## Chunk 1: Protocol Contract Freeze + Baseline Harness

### Requirements
- Freeze and document current packet contract:
  - Discovery fields: sender ID, TTL, path, GPS availability/location.
  - Election fields: class ID, PDSF, score, hash.
- Centralize protocol constants and defaults in `BleMeshDiscoveryConfig`.
- Add deterministic controls:
  - Seed handling.
  - Deterministic time/cycle configuration.
- Ensure C core and ns-3 wrapper stay behaviorally aligned.

### Deliverables
- Config type and defaults defined in module API.
- Packet format contract doc updated/finalized.
- Deterministic test harness utilities.

### Section Test Plan
- Unit tests:
  - Serialization/deserialization roundtrip (discovery and election).
  - Boundary/path length limits.
  - GPS available/unavailable behavior.
  - TTL edge cases.
- Determinism tests:
  - Same seed + same inputs -> identical packet outputs/order.

### Exit Gate
- All packet tests pass and determinism tests pass on repeated runs.

---

## Chunk 2: Node Runtime + 4-Slot Discovery Cycle

### Requirements
- Implement per-node runtime state container with `BleMeshNodeState`.
- Implement discovery cycle scheduler with 4 slots:
  - Slot 1: own discovery message.
  - Slots 2-4: forwarding candidates.
- Implement RX queue and dedupe cache.
- Enforce PSF loop rejection.
- Enforce TTL decrement and expiration handling.

### Deliverables
- `BleDiscoveryEngine` with cycle/timer control.
- Queue + dedupe + loop/TTL gating logic.

### Section Test Plan
- Unit tests:
  - State initialization and transitions.
  - Slot budget enforcement (max 1 own + 3 forwarded).
  - Dedupe cache hit/miss and expiration behavior.
  - PSF loop rejection correctness.
  - TTL decrement and drop at zero.
- Scenario smoke test:
  - 20-node static topology, fixed seed, cycle executes and converges without invariant violations.

### Exit Gate
- Slot and queue invariants hold; 20-node smoke test stable.

---

## Chunk 3: Forwarding Policy Pipeline

### Requirements
- Implement forwarding decision pipeline in this order:
  1. Picky Forwarding (crowding-factor-based percentage filter).
  2. GPS proximity filter (skip when GPS unavailable).
  3. TTL sorting and top-3 forwarding selection.
- Expose configurable thresholds/weights through config.
- Log forwarding decisions for diagnostics.

### Deliverables
- Deterministic forwarding pipeline implementation integrated into discovery cycle.
- Policy telemetry counters.

### Section Test Plan
- Unit tests per filter:
  - Picky forwarding acceptance probability under controlled crowding input.
  - GPS proximity pass/fail and GPS-unavailable bypass.
  - TTL ordering and top-3 selection.
- Combined pipeline tests:
  - Fixed input queue -> expected forwarded set.
- Density sweep regression:
  - Validate lower overhead at higher crowding while maintaining minimum reachability target.

### Exit Gate
- Policy unit tests pass and density sweep shows expected congestion-vs-coverage tradeoff.

---

## Chunk 4: Connectivity Metrics + Candidacy Decision

### Requirements
- Implement noisy broadcast/listen phase for local crowding and connectivity sensing.
- Track required candidacy metrics:
  - Direct neighbor count.
  - Unique paths discovered.
  - Geographic distribution metric.
  - Successful forwarding participation.
- Implement candidacy scoring and threshold decision using direct:noise ratio + distribution/forwarding criteria.

### Deliverables
- `BleElectionEngine` metric collector and candidacy evaluator.
- Candidate state transition path from `DISCOVERY` -> `CLUSTERHEAD_CANDIDATE`.

### Section Test Plan
- Unit tests:
  - Metric calculations with synthetic topologies.
  - Score/threshold behavior under sparse/dense/noisy inputs.
- Scenario tests:
  - Clustered topology vs uniform topology produce different candidacy distribution as expected.

### Exit Gate
- Candidacy logic produces stable, reproducible candidate sets by topology type.

---

## Chunk 5: Election Announcement (3 Rounds) + Conflict Logic

### Requirements
- Implement election announcements over 3 rounds.
- Propagate and update election payload (class ID, PDSF, score, hash, path).
- Enforce capacity stop behavior when PDSF reaches configured cluster cap (default 150).
- Conflict resolution:
  - Higher direct connection count wins.
  - Tie -> lower node ID wins.
- Implement candidacy renouncement broadcasts when losing to superior candidate.

### Deliverables
- Round scheduler and flood control logic in `BleElectionEngine`.
- Conflict resolver and renouncement path.

### Section Test Plan
- Unit tests:
  - 3-round timing correctness.
  - Conflict winner selection and tie-break correctness.
  - Capacity stop at cluster cap.
- Scenario tests:
  - Overlapping candidates converge to deterministic winner set.
  - Renouncement messages remove stale candidates.

### Exit Gate
- Candidate conflicts fully resolved; no unresolved candidacy remains after final round.

---

## Chunk 6: Cluster Formation + Path Management

### Requirements
- Implement edge-node cluster alignment rule:
  - Prefer shortest path.
  - If equal, prefer highest direct-count source.
- Memorize multiple feasible paths per clusterhead.
- Run Dijkstra over discovered graph to add shortest feasible route.
- Build routing tree entries for each edge to chosen clusterhead.
- Ensure loop-free route selection.

### Deliverables
- `BleClusterManager` cluster assignment and path table.
- Path memory + shortest-path computation integration.

### Section Test Plan
- Unit tests:
  - Path ranking and tie-break logic.
  - Dijkstra correctness on known graphs.
  - Loop-free checks on generated trees.
- Integration tests:
  - Multipath scenarios with redundant links.
  - Node reassignment when better path appears.

### Exit Gate
- Every edge node has valid cluster assignment and loop-free route set.

---

## Chunk 7: Scenario + Instrumentation for Scale

### Requirements
- Provide reusable scenario runner/helper with configuration presets:
  - 20-node, 150-node, 1000+-node.
  - Static and mobility modes.
- Implement `BleMeshMetricsCollector` with exports (CSV/JSON).
- Capture required metrics:
  - Convergence time.
  - End-to-end latency.
  - Packet delivery ratio.
  - Control overhead.
  - Cluster size distribution.
  - Protocol invariant counters.

### Deliverables
- Scenario entrypoints and standardized output artifacts.
- Repeatable benchmark scripts/config profiles.

### Section Test Plan
- Reproducibility tests:
  - Same seed -> same outcome within deterministic tolerance.
- Regression tests:
  - 20-node and 150-node baseline runs.
- Large-scale run:
  - Scheduled 1000+ run with metrics capture and no fatal invariant break.

### Exit Gate
- Scale harness runs produce complete, parseable metrics artifacts for all profiles.

---

## Chunk 8: End-to-End Validation + Parameter Tuning

### Requirements
- Tune default parameters for:
  - Crowding-forwarding behavior.
  - GPS proximity threshold.
  - Candidacy threshold behavior.
- Lock stable default profiles for each scale tier.
- Publish known limitations and recommended run commands.

### Deliverables
- Final tuned config profiles.
- Validation report and runbook.

### Section Test Plan
- Acceptance matrix on 20 -> 150 -> 1000+ with fixed seeds.
- Compare tuned vs baseline metrics.
- Confirm protocol invariants and convergence goals remain satisfied.

### Exit Gate
- Acceptance matrix passes with documented parameter sets and reproducible artifacts.

## 2.1 Current Gap Checklist (Exact Missing Work)

Status snapshot date: **2026-03-31**.

### Chunk 1: Protocol Contract Freeze + Baseline Harness (missing)
- Add a concrete public `BleMeshDiscoveryConfig` type (currently defaults are spread across macros and wrapper attributes).
- Migrate protocol defaults and tunables into that config so C core + ns-3 wrappers use one source of truth.
- Add deterministic replay tests that assert identical packet ordering/output for same seed + same timeline across repeated runs.
- Add explicit protocol-contract lock metadata (version/revision) tied to current discovery/election wire schema.

### Chunk 2: Node Runtime + 4-Slot Discovery Cycle (missing)
- Convert the 20-node smoke gate into an automated pass/fail test target (not only ad-hoc simulation/manual trace review).
- Add explicit engine-level assertions for slot budget: max 1 own TX + max 3 forwarded TX per cycle.
- Add integrated tests for dedupe-cache expiration behavior at engine level (not only queue unit scope).
- Add an explicit convergence criterion used by the 20-node gate and assert it in CI/test scripts.

### Chunk 3: Forwarding Policy Pipeline (missing)
- Route all forwarding policy thresholds/weights through centralized config (`BleMeshDiscoveryConfig`).
- Add forwarding-decision reason telemetry (drop/forward reason counters and traces) for diagnostics.
- Add integrated top-3 selection test proving queue > 3 candidates resolves by filter pipeline then TTL priority.
- Add density sweep regression with a defined reachability floor and overhead target.

### Chunk 4: Connectivity Metrics + Candidacy Decision (missing)
- Implement and persist an explicit **unique-path count** metric for candidacy.
- Update candidacy scoring to explicitly include direct:noise ratio + geographic distribution + forwarding success (weighted/configurable).
- Implement dynamic candidacy thresholds driven by local crowding (as required by the split and PDF behavior).
- Add automated topology-driven candidacy tests (clustered vs uniform) with deterministic expectations.

### Chunk 5: Election Announcement (3 Rounds) + Conflict Logic (missing)
- Add dedicated tests that verify exactly 3 election rounds are executed per candidacy lifecycle.
- Add gate test asserting no unresolved `CLUSTERHEAD_CANDIDATE` remains after final election/renouncement rounds.
- Add overlap-candidate convergence tests that validate tie-break and stale-candidate cleanup end-to-end.
- Add explicit multi-hop capacity-stop tests confirming retransmission halts once PDSF reaches cluster cap.

### Chunk 6: Cluster Formation + Path Management (missing)
- Implement `BleClusterManager` and integrate it into the module build.
- Implement per-clusterhead path memorization for all feasible discovered paths.
- Build/maintain discovered graph and run Dijkstra shortest-path insertion into feasible path sets.
- Build routing-tree entries per edge node to selected clusterhead and persist assignment tables.
- Implement loop-freedom validation and reassignment when a better path appears.
- Add required unit/integration tests for path ranking, Dijkstra correctness, multipath behavior, and loop freedom.

### Chunk 7: Scenario + Instrumentation for Scale (missing)
- Add reusable scenario helper/runner entrypoints for 20/150/1000+ profiles and static + mobility modes.
- Implement `BleMeshMetricsCollector` as a dedicated component.
- Export metrics in both CSV and JSON with standardized schema.
- Compute and persist all required metrics: convergence time, E2E latency, PDR, control overhead, cluster-size distribution, invariant counters.
- Add reproducibility/regression automation: fixed-seed repeatability checks and scheduled 1000+ run.

### Chunk 8: End-to-End Validation + Parameter Tuning (missing)
- Run structured parameter sweeps for crowding, GPS threshold, and candidacy thresholds.
- Lock and publish tuned default profiles for 20, 150, and 1000+ scale tiers.
- Produce acceptance-matrix artifacts for 20 -> 150 -> 1000+ with fixed seeds.
- Compare tuned vs baseline performance and record pass/fail thresholds.
- Publish final runbook commands and known-limitations section.

### Cross-Section Verification Gap (missing)
- Rebuild and run test/sim binaries on the active host architecture and capture pass/fail evidence for all chunk exit gates.

## 3. Cross-Section Integration Plan

### Integration Order
1. Integrate Chunks 1-3 into end-to-end discovery pipeline.
2. Integrate Chunks 4-5 for election completion and conflict closure.
3. Integrate Chunk 6 for assignment/path outputs.
4. Integrate Chunk 7 instrumentation across all prior flows.
5. Finalize with Chunk 8 tuning and acceptance reruns.

### Cross-Section Invariants (must hold at every stage)
- No forwarding when TTL = 0.
- Max 3 forwarded messages per cycle.
- No PSF loops in forwarded traffic.
- No cluster capacity above configured cap (default 150).
- Deterministic outcomes under fixed seed.

### Integration Test Matrix
- Functional:
  - All nodes end as `EDGE` or `CLUSTERHEAD`.
  - No unresolved `CLUSTERHEAD_CANDIDATE` after election rounds.
- Correctness:
  - Conflict resolution and renouncement produce stable results.
  - Routing/tree data has no loops.
- Performance:
  - Latency remains in seconds-scale target range.
  - Convergence time bounded per scenario profile.
- Scale progression:
  - 20-node gate pass required before 150-node gate.
  - 150-node gate pass required before 1000+-node gate.
- Reproducibility:
  - Repeat run with same seed yields matching clusterhead/path outcomes within tolerance.

## 4. Definition of Done

Project is considered complete when all conditions below are met:
- All eight chunks are implemented and pass their exit gates.
- Full integration test matrix passes across 20, 150, and 1000+ profiles.
- Default tuned configurations are locked and documented.
- Scenario runner and metrics exports are available and reproducible.
- Limitations and out-of-scope items are explicitly documented for v2 planning.

## 5. Notes for v2 (Deferred)

The following are intentionally deferred from v1 and should be handled in a later phase:
- Full FDMA/TDMA scheduling implementation after discovery.
- Clusterhead-to-clusterhead relay/path-exchange fabric.
- Any additional wire-format changes beyond current discovery/election headers.
