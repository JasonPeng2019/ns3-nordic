# ch06: `BleClusterManager`

This chunk contains a single ns-3 object that stores and retrieves node-to-clusterhead assignments.

## Scope and File Inventory

Only these files exist in this chunk:

- `ble-cluster-manager.h`
- `ble-cluster-manager.cc`

## High-Level Behavior

`BleClusterManager` is an in-memory lookup table:

- key: `nodeId` (`uint32_t`)
- value: `clusterheadId` (`uint32_t`)

It supports three operations:

- set/update a node's clusterhead
- query a node's clusterhead
- clear all assignments

There is no persistence, ranking logic, or history tracking in this chunk.

## Dependency Breakdown (All Files)

### `ble-cluster-manager.h`

Direct includes:

- `ns3/object.h`
  - Provides `ns3::Object` base class and ns-3 object/type infrastructure used by the class declaration.
- `<map>`
  - Provides `std::map` for internal assignment storage.
- `<vector>`
  - Included but not used in this header.

Declared relationships:

- `class BleClusterManager : public Object`
  - Inherits from ns-3 `Object`.

Private member dependencies:

- `std::map<uint32_t, uint32_t> m_assignment`

### `ble-cluster-manager.cc`

Direct includes:

- `"ble-cluster-manager.h"`
  - Pulls in all class declarations and transitive dependencies from the header.

Used ns-3 facilities:

- `NS_OBJECT_ENSURE_REGISTERED (BleClusterManager)`
  - Registers the class with ns-3 runtime type system.
- `TypeId`
  - Used in `GetTypeId()` for runtime type metadata.

Standard library usage in implementation:

- `std::map::operator[]` for insert/update in `SetClusterhead`
- `std::map::find` and iterator comparison for lookup in `GetClusterhead`
- `std::map::clear` in `Clear`

## API and Function-by-Function Implementation

### `static TypeId GetTypeId (void)`

Purpose:

- Exposes ns-3 type metadata for `BleClusterManager`.

Implementation details:

- Creates a static `TypeId` named `"ns3::BleClusterManager"`.
- Sets parent to `Object` via `.SetParent<Object>()`.
- Places class in group `"BleMeshDiscovery"` via `.SetGroupName(...)`.
- Registers default constructor via `.AddConstructor<BleClusterManager>()`.

### `BleClusterManager::BleClusterManager () = default;`

Purpose:

- Default constructor with no custom initialization logic.

Behavior:

- `m_assignment` starts empty.

### `BleClusterManager::~BleClusterManager () = default;`

Purpose:

- Default destructor override for `Object` polymorphic destruction.

Behavior:

- Relies on RAII cleanup of `std::map`.

### `void SetClusterhead (uint32_t nodeId, uint32_t clusterheadId, uint16_t hops, uint32_t directCount)`

Purpose:

- Assigns or updates the clusterhead for `nodeId`.

Implementation details:

- Current implementation ignores `hops` and `directCount` entirely.
- Stores assignment with:
  - `m_assignment[nodeId] = clusterheadId;`
- If `nodeId` already exists, prior value is overwritten.

### `bool GetClusterhead (uint32_t nodeId, uint32_t &clusterheadId) const`

Purpose:

- Retrieves clusterhead assignment for `nodeId`.

Implementation details:

- Performs `find(nodeId)` in `m_assignment`.
- Returns `false` if node is missing.
- On success:
  - writes result into output reference `clusterheadId`
  - returns `true`

Important detail:

- On `false`, `clusterheadId` is left unchanged by this function.

### `void Clear ()`

Purpose:

- Removes all stored assignments.

Implementation details:

- Calls `m_assignment.clear()`.

## Data Model and Control Flow

Write path:

1. Caller invokes `SetClusterhead(nodeId, clusterheadId, hops, directCount)`.
2. Manager inserts or updates `m_assignment[nodeId]`.

Read path:

1. Caller invokes `GetClusterhead(nodeId, outClusterheadId)`.
2. Manager checks map.
3. Returns `true` with output value if found; else returns `false`.

Reset path:

1. Caller invokes `Clear()`.
2. All assignments are erased.

## Highlights

- Minimal and deterministic implementation.
- O(log N) insert/update/lookup behavior due to `std::map`.
- Clean ns-3 type registration and construction path.
- Easy to integrate as a shared state container in simulation components.

## Conformance Against `split.md` Chunk 6 Plan

Reference plan section:

- `split.md` -> `Chunk 6: Cluster Formation + Path Management (Scalable Bounds)`

Conformance verdict:

- This implementation is **not yet conformant** with planned Chunk 6 behavior.
- It matches only the documented scaffold status (`BleClusterManager` existence), not the required path-management feature set.

Requirement-by-requirement comparison:

1. Edge alignment rule (shortest path -> highest direct-count -> lower clusterhead ID)
   - **Planned:** mandatory.
   - **Current:** not implemented.
   - **Deviation:** `SetClusterhead(...)` ignores both `hops` and `directCount`.

2. Bounded feasible-path storage
   - **Planned:** enforce `max_paths_per_edge_per_clusterhead=4`, `max_clusterheads_tracked_per_edge=3`.
   - **Current:** no path store exists.
   - **Deviation:** only a single `nodeId -> clusterheadId` map is stored.

3. Graph builder + Dijkstra shortest feasible route insertion
   - **Planned:** mandatory.
   - **Current:** not implemented.
   - **Deviation:** no graph representation, no shortest-path computation.

4. Loop-free routing tree entries per edge
   - **Planned:** mandatory.
   - **Current:** not implemented.
   - **Deviation:** no tree/path model exists to validate loop-freedom.

5. Recompute policy with debounce and topology-delta bypass
   - **Planned:** recompute no more often than every 5 cycles unless topology delta >10%.
   - **Current:** not implemented.
   - **Deviation:** no recompute scheduler, debounce, or topology-delta tracking.

6. Deliverables expected from Chunk 6
   - **Planned:** assignment tables + bounded path store + loop checks + Dijkstra integration.
   - **Current:** assignment map only.
   - **Deviation:** major deliverables are still missing.

## Prerequisites (Embedded, Mandatory)

These requirements are mandatory for an embedded-target implementation of Chunk 6 (or standard industry practice for safety/reliability-sensitive networking firmware).

1. Memory determinism and hard bounds
   - Use fixed upper bounds for all node/path structures (`MAX_NODES`, `MAX_CLUSTERHEADS_PER_EDGE`, `MAX_PATHS_PER_EDGE_PER_CLUSTERHEAD`, `MAX_PATH_HOPS`).
   - No unbounded container growth at runtime.
   - No heap allocation in steady-state operation; preallocate pools/buffers at init.
   - Enforce and test overflow behavior when bounds are reached.

2. Deterministic behavior and tie-break reproducibility
   - Implement exact deterministic ranking rule: shortest path, then highest direct-count, then lowest clusterhead ID.
   - Ensure stable deterministic iteration/order so identical inputs produce identical assignments.
   - Define one canonical integer-domain comparison path (avoid floating tie-break behavior).

3. Bounded runtime/WCET behavior
   - Define worst-case execution time (WCET) budgets for assignment update, route recompute, and query operations.
   - Use algorithms/data structures with predictable worst-case latency under configured max scale.
   - Avoid recursion in routing/path logic.

4. Path safety and loop prevention
   - Validate every inserted path for loop-freedom.
   - Reject malformed paths (duplicate node in path, invalid hop count, out-of-range node IDs).
   - Maintain parent/next-hop state with explicit invariants that can be asserted in debug builds.

5. Recompute control for CPU/power stability
   - Recompute must be event-driven and debounce-limited (>=5 cycles between recomputes unless topology delta >10%).
   - Track topology-change counters and recompute reasons.
   - Cap recompute work per cycle to preserve real-time scheduling headroom.

6. API and state contract strictness
   - `SetClusterhead` parameters must either be fully used (`hops`, `directCount`) or removed from the API.
   - Provide explicit remove/invalidate APIs for stale entries.
   - Define invalid-ID and self-assignment policy and enforce it.

7. Concurrency and ISR/task safety
   - If accessed across tasks/ISRs, define single-writer ownership or lock strategy.
   - Guarantee atomic visibility for reads during updates.
   - Prohibit blocking locks in time-critical contexts.

8. Fault handling and observability
   - Add mandatory counters: bound-hit, loop-reject, invalid-input, recompute-trigger, recompute-skipped-debounce.
   - Expose deterministic diagnostics for postmortem/root-cause analysis.
   - Fail safely on invariant violation (drop update + increment error counter, no undefined behavior).

9. Verification requirements
   - Unit tests are required for: ranking/tie-break, bounds enforcement, Dijkstra correctness, loop-freedom, debounce policy.
   - Add stress tests at target scale bounds with worst-case topology churn.
   - Add static analysis/lint gate aligned with embedded C++ standards (for example MISRA/CERT-C++ profile used by your organization).
   - Verify deterministic replay for fixed seeds and identical input timelines.

## Potential Issues and Risks

1. **Ignored parameters in `SetClusterhead`**
   - `hops` and `directCount` are accepted but not used.
   - Risk: callers may assume these influence selection/priority when they do not.

2. **Unused header include (`<vector>`)**
   - Adds avoidable compile dependency noise.

3. **Silent overwrite semantics**
   - Reassigning a `nodeId` replaces previous clusterhead with no warning or trace.
   - Risk: hard to debug churn without instrumentation.

4. **No validation constraints**
   - No checks for invalid/self assignments (for example `nodeId == clusterheadId`) or reserved IDs.

5. **No removal API for a single node**
   - Only full `Clear()` exists.
   - Risk: callers needing selective invalidation must reassign or rebuild state externally.

6. **No observability hooks**
   - No trace sources, counters, or attributes exposed via `TypeId`.
   - Makes runtime introspection harder in larger experiments.

## Suggested Improvements (If Needed)

- Use or remove `hops`/`directCount` to align interface with behavior.
- Remove unused `<vector>` include.
- Add optional validation/assertion policy for assignments.
- Add `RemoveClusterhead(nodeId)` and optional `HasClusterhead(nodeId)`.
- Add trace sources or logging for assignment changes.
