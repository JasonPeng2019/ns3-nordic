# ch06: `BleClusterManager`

## Scope and File Inventory

This chunk contains only:

- `ble-cluster-manager.h`
- `ble-cluster-manager.cc`

`BleClusterManager` currently implements a minimal `nodeId -> clusterheadId` assignment map.

## Current Implementation Summary

- Data model: `std::map<uint32_t, uint32_t> m_assignment`
- Public operations:
  - `SetClusterhead(nodeId, clusterheadId, hops, directCount)`
  - `GetClusterhead(nodeId, clusterheadIdOut)`
  - `Clear()`
- Current behavior:
  - `hops` and `directCount` are accepted but ignored.
  - latest write silently overwrites any previous assignment for the same `nodeId`.
  - no bounded path store, graph model, Dijkstra path insertion, loop checks, or recompute policy.

## PDF and `split.md` Mapping

### PDF Section 3 Step 7 (Cluster Formation)

The protocol source PDF states:

> "Edge nodes align themselves with the Clusterhead from which they received the lower length path or the highest direct count message."

It also states that nodes memorize paths, build a tree, and use Dijkstra when there are multiple interconnected paths to one cluster.

### Ambiguity Resolution Locked by `split.md`

The PDF phrase "lower length path or highest direct count" is ambiguous.  
`split.md` Chunk 6 resolves this into a deterministic lexicographic ordering:

1. shortest path,
2. then highest direct-count source,
3. then lower clusterhead ID.

This README follows the `split.md` locked ordering as canonical for implementation and tests.

### Memorized vs Feasible Paths (PDF Terminology)

- Memorized paths: all discovered/observed paths to clusterheads.
- Feasible paths: bounded routable subset used by cluster-formation/routing logic.

`split.md` bounds (`max_paths_per_edge_per_clusterhead=4`, `max_clusterheads_tracked_per_edge=3`) apply to the feasible-path working set.

### Dijkstra Scope Clarification

PDF wording indicates Dijkstra is used for multiple interconnected paths to one cluster (per-cluster intent), not necessarily a full global all-target shortest-path computation every update.

## Conformance Against `split.md` Chunk 6

Reference: `model/chunks/split.md` -> "Chunk 6: Cluster Formation + Path Management (Scalable Bounds)".

| Requirement | Status | Evidence in this chunk |
|---|---|---|
| Edge alignment rule (path, direct-count, clusterhead ID tie-break) | Missing | `SetClusterhead` ignores `hops`/`directCount`; simple overwrite only. |
| Bounded feasible-path storage | Missing | no path store; only one `nodeId -> clusterheadId` map. |
| Graph builder + Dijkstra shortest feasible route insertion | Missing | no graph representation or shortest-path logic. |
| Loop-free routing tree entries per edge | Missing | no tree structure or loop validation. |
| Recompute policy with debounce and topology-delta bypass | Missing | no recompute scheduler or topology-change tracking. |
| Deliverables (`BleClusterManager` assignment tables + bounded path store + graph + loop checks) | Partial | class scaffold exists; required path-management behavior absent. |

## Chunk 6 Exit Gate (Restated)

From `split.md`, Chunk 6 is complete only when all are true:

1. 100% reachable edge nodes are assigned.
2. Loop count in resulting trees is zero.
3. Path-storage and recompute bounds are never violated in 20/150/1000 profiles.

Current status in this chunk: not met; this scaffold alone cannot satisfy these gates.

## Integration Ownership and Cross-Chunk Fit

- Upstream decision logic overlap:
  - `ch05` currently applies edge-alignment style ranking in `ble_engine_update_clusterhead_selection` (hop count, then direct connections, then sender ID).
  - `ch06` currently does not make the decision; it stores whatever assignment it is given.
- Wiring status:
  - no in-repo call sites currently invoke `BleClusterManager::SetClusterhead`.
  - module `wscript` still lists `ble-cluster-manager.*` as future/commented entries.
- Downstream consumer status:
  - no routing/metrics export path currently consumes this manager state in `ch06`.

So at present, `ch06` behaves as an isolated storage scaffold, not the active Cluster 6 decision engine.

## API Notes (Behavioral Contracts)

### `SetClusterhead(uint32_t nodeId, uint32_t clusterheadId, uint16_t hops, uint32_t directCount)`

- Current implementation stores only `nodeId -> clusterheadId`.
- `hops` and `directCount` are currently dead parameters.
- overwrite policy is unconditional and silent.

### `GetClusterhead(uint32_t nodeId, uint32_t &clusterheadId) const`

- Returns `true` and writes output on hit.
- Returns `false` on miss and leaves `clusterheadId` unchanged.
- Caller must check the boolean return to avoid stale output usage.

### `Clear()`

- Removes all assignments.
- No API exists for per-node invalidation/removal.

## Open Spec Questions to Resolve in Chunk 6 Implementation

1. Topology-delta definition for `>10%` bypass:
   - percent of what exactly (neighbor edges, assignment changes, feasible-path entries, or another metric)?
2. Dijkstra invocation scope:
   - per-cluster only (PDF intent) vs global graph recompute.
3. Assignment validity contract:
   - is this map storing only EDGE->CH assignments, or full role bindings including CH self-records?
4. Ownership boundary with `ch05`:
   - should selection logic move into `ch06`, or should `ch06` be defined as a passive store with explicit producer/consumer contracts?

## Embedded Prerequisites and Current Contradictions

Mandatory for embedded-ready Chunk 6 behavior:

- fixed bounds and preallocated storage for nodes/paths/clusterheads.
- deterministic ranking and deterministic iteration order.
- bounded recompute work with explicit debounce/reason counters.
- loop-freedom checks and malformed-path rejection.
- deterministic diagnostics and invariant counters.

Current scaffold contradiction:

- `std::map` is heap-backed and unbounded by default, which conflicts with no-heap/steady-state deterministic-memory expectations for embedded deployment.

## Risks

1. Ignored `hops`/`directCount` can mislead callers into assuming ranking behavior exists.
2. Integration gap: no active producer/consumer wiring means this class can silently diverge from runtime behavior.
3. `std::map` introduces heap churn and violates embedded bounded-memory expectations.
4. Silent overwrite hides churn and can mask instability under frequent reassignment.
5. Validation is too weak:
   - real invariant is not simply `nodeId != clusterheadId`; assignment should target a known/valid clusterhead set for the chosen state model.
6. `GetClusterhead` miss path is a footgun if callers ignore the boolean and read stale output.
7. No per-node removal API forces full reset or external workarounds.
8. No observability hooks (counters/traces) for assignment changes or invalid updates.

## v2 Slotting Note

The PDF describes FDMA/TDMA slotting after cluster formation, but also states this is out of project scope in v1.  
For future integration, `ch01` already exposes hash-slot helpers (`ble_hash_map_edge_slot`, `ble_hash_next_slot_time_ms`) that `ch06`/routing logic can consume when this extension is enabled.
