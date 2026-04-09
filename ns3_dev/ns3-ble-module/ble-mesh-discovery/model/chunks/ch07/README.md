# ch07: `BleMeshMetricsCollector`

This README documents only files in `model/chunks/ch07`.

## Scope and File Inventory

This chunk contains exactly:

- `ble-mesh-metrics-collector.h`
- `ble-mesh-metrics-collector.cc`

Current class behavior is a scaffold:

- stores one string field (`m_outputPrefix`)
- provides setter/getter
- no metric aggregation, scenario control, trace ingestion, or export

## Requirement Sources (PDF vs `split.md`)

Chunk 7 requirements are primarily from `split.md`, not from a detailed PDF benchmarking section.

- The PDF describes protocol behavior and protocol-level metric concepts.
- The PDF does not define the Chunk 7 instrumentation contract (scenario presets, CSV/JSON artifact schema, CI-scale export gates).
- Therefore this README treats `split.md` Chunk 7 and Section 5 as the canonical requirements for this chunk.

## Current API and Behavior

Class: `ns3::BleMeshMetricsCollector` (`ns3::Object`)

- `GetTypeId()`: registers `"ns3::BleMeshMetricsCollector"` in group `"BleMeshDiscovery"`.
- constructor: initializes `m_outputPrefix = "ble_mesh_metrics"`.
- `SetOutputPrefix(const std::string&)`: stores caller value with no validation.
- `GetOutputPrefix() const`: returns current prefix by value.

Current implementation has no:

- ns-3 Attributes
- trace sources
- counters or aggregation state
- file outputs
- tests in this chunk

## Integration Contracts (Cross-Chunk)

### Upstream Producers Already Available

`ch05::BleDiscoveryEngine` already exposes trace producers this chunk should consume:

- `MetricsUpdate` trace
  - payload type: `ble_connectivity_metrics_t`
  - fields include:
    - `direct_connections`, `total_neighbors`, `crowding_factor`, `connection_noise_ratio`, `geographic_distribution`
    - `messages_forwarded`, `messages_received`, `forwarding_success_rate`
    - `slots_tx`, `slots_rx`, `slots_collision`, `slots_empty`
- `SlotOutcome` trace
  - payload fields:
    - `nodeId`, `isClusterSlot`, `frameIndex`, `slotIndex`, `channelIndex`, `iteration`, `outcome`

Current `ch07` does not subscribe to either trace.

### Upstream Dependency for Synthetic Traffic Semantics

`split.md` requires default synthetic traffic:

- each edge emits `1 packet/sec` toward selected clusterhead after convergence.

This depends on cluster assignment/path outputs (Chunk 6 ownership).  
Because `ch06` is currently scaffold-level, `ch07` can only do:

- placeholder traffic generation against partial assignment state, or
- wait for full Chunk 6 path/assignment implementation for protocol-faithful traffic.

### Downstream Role

Chunk 7 is the primary consolidation point for reproducibility evidence and acceptance artifacts across earlier chunks, because it owns standardized CSV/JSON outputs and scenario-profile execution/reporting.

It is not the only possible way to prove D0 for a chunk, but it is the planned system-level path in `split.md`.

## Conformance Against `split.md` Chunk 7

Reference: `split.md` "Chunk 7: Scenario + Instrumentation for Scale" and Section 5.

| Requirement | Status | Evidence in this chunk |
|---|---|---|
| Scenario presets/runners (`20`, `150`, `1000+`, static + mobility) | Missing | no scenario runner/helper code in this chunk. |
| CSV + JSON exports | Missing | no serialization/output methods. |
| Metric computation (convergence, latency, PDR, overhead, cluster size, invariants) | Missing | no counters/formulas in collector. |
| Synthetic traffic model (`1 packet/sec` toward selected CH) | Missing | no traffic generation hooks. |
| D0/D1 reproducibility reporting integration | Missing | no artifact compare/report path. |
| `BleMeshMetricsCollector` scaffold existence | Implemented | type registration + prefix storage API. |

## Chunk 7 Exit Gate (Restated)

From `split.md`, Chunk 7 is complete only when:

1. All profiles emit parseable CSV+JSON artifacts.
2. 1000+ scenario completes with zero fatal invariant breaks.

Current status in this chunk: not met.

## Cross-Section Invariants To Count (Section 5)

Chunk 7 invariant counters must at minimum cover:

1. No forwarding when `ttl == 0`.
2. Max 3 forwarded messages per cycle.
3. No PSF loops in forwarded traffic.
4. Discovery/election single-channel operation in v1.
5. `PDSF` soft-cap retransmission stop enforced.
6. Hard membership cap violations tracked and zero at acceptance.
7. `D0` determinism holds for fixed static scenarios.

## Quantitative Acceptance Matrix (Locked CI Targets)

From `split.md` Section 5:

| Profile | Convergence Bound | P95 E2E Latency Bound | PDR Floor | Hard Cap Violations |
|---|---:|---:|---:|---:|
| 20-node static | <= 20 cycles | <= 2.0 s | >= 0.98 | 0 |
| 150-node static | <= 75 cycles | <= 5.0 s | >= 0.95 | 0 |
| 1000+-node static | <= 220 cycles | <= 8.0 s | >= 0.90 | 0 |

Chunk 7 exports must provide enough schema and fields to validate these thresholds directly.

## Determinism Scope and Chunk 7 Role

`split.md` determinism tiers:

- `D0`: identical traces and final assignments for repeated fixed-seed static runs.
- `D1`: mobility metrics (`convergence`, `PDR`, `overhead`) remain within +/-5%.

Chunk 7 is where these tiers become testable at integration level:

- stable artifact schema
- stable ordering of records/keys
- repeatable profile execution
- deterministic comparison tooling

Without this chunk implemented, earlier chunk D0 claims remain fragmented and hard to verify consistently.

## Metric Definition Notes

### Control Overhead

`split.md` requires control-overhead reporting but does not lock a formula.  
`ch03` exit criteria define relative overhead reduction (`>=30%` vs no-picky baseline), so Chunk 7 should at minimum emit a control-traffic count/time normalization sufficient to compute that comparison reproducibly.

### Prefix Semantics

Current `m_outputPrefix` semantics are undefined (filename stem vs directory path vs full path prefix).  
Chunk 7 should lock this explicitly. Recommended minimal convention:

- `<prefix>.csv`
- `<prefix>.json`

with deterministic suffixing for profile/seed if multiple runs are emitted.

## Risks (Prioritized)

1. Class/functionality mismatch: "metrics collector" currently stores only a string.
2. No trace ingestion from `ch05` (`MetricsUpdate`, `SlotOutcome`), so no actionable measurement path.
3. No export schema or output functions, so Chunk 7 exit-gate artifacts cannot be produced.
4. No invariant-counter implementation, blocking Section 5 cross-section checks.
5. Synthetic traffic model depends on cluster assignment/path state not fully available from current Chunk 6 scaffold.
6. No `m_outputPrefix` validation/sanitization and semantics undefined.
7. No ns-3 attributes, tests, logging, or trace outputs in this chunk.

## Practical Next Integration Step

Implement trace adapters in this chunk for `ch05` `MetricsUpdate` and `SlotOutcome` payloads first, then define a versioned CSV/JSON schema that includes:

- profile/seed/run metadata
- acceptance-matrix metrics
- all seven cross-section invariant counters
