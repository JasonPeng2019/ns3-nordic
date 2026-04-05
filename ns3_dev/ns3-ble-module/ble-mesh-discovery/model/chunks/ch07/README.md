# ch07 Overview: `BleMeshMetricsCollector`

This README documents only the files inside `model/chunks/ch07`.

## Files in This Chunk

- `ble-mesh-metrics-collector.h`: Declares `ns3::BleMeshMetricsCollector`, its public API, and internal state.
- `ble-mesh-metrics-collector.cc`: Implements type registration and all methods declared in the header.

## High-Level Purpose

`BleMeshMetricsCollector` is currently a lightweight ns-3 `Object` wrapper that stores one configuration value (`m_outputPrefix`).

Despite the name, this chunk does **not** yet implement metrics collection logic, file writing, aggregation, or trace hookups.

## Alignment With `split.md` Chunk 7 Plan

`split.md` (Section "Chunk 7: Scenario + Instrumentation for Scale") marks this chunk as a scaffold stage and expects:

- Scenario presets/runners for `20`, `150`, and `1000+` node profiles (static + mobility).
- `BleMeshMetricsCollector` CSV and JSON exports.
- Metric computation for convergence, E2E latency, PDR, control overhead, cluster-size distribution, and invariant counters.
- Synthetic data-plane traffic model (default `1 packet/sec` from each edge after convergence).

Current `ch07` implementation is **only partially aligned**:

- Fits: `BleMeshMetricsCollector` class scaffold exists and is ns-3 type-registered.
- Deviates: no scenario helper/presets in this folder.
- Deviates: no metrics are computed or tracked.
- Deviates: no CSV/JSON serialization or artifact schema.
- Deviates: no synthetic traffic hooks for E2E/PDR measurement.
- Deviates: no reproducibility/reporting logic tied to `D0/D1` expectations.

## Dependency Map

### File-Level Includes

- `ble-mesh-metrics-collector.h`
- depends on `ns3/object.h` for `ns3::Object` and `TypeId` integration.
- depends on `<string>` for `std::string`.

- `ble-mesh-metrics-collector.cc`
- depends on local header `ble-mesh-metrics-collector.h`.
- indirectly depends on ns-3 type system APIs used by `GetTypeId()` (`TypeId`, `SetParent`, `SetGroupName`, `AddConstructor`) and `NS_OBJECT_ENSURE_REGISTERED`.

### Symbol/Runtime Dependencies

- `BleMeshMetricsCollector` inherits from `ns3::Object`.
- `NS_OBJECT_ENSURE_REGISTERED (BleMeshMetricsCollector)` ties the class into ns-3 runtime type registration.
- `TypeId ("ns3::BleMeshMetricsCollector")` defines how ns-3 introspection identifies this class.

## API and Function-by-Function Breakdown

### `ble-mesh-metrics-collector.h`

- `static TypeId GetTypeId (void);`
- Purpose: ns-3 type metadata declaration for object factory/introspection.
- Dependencies: `TypeId`, `Object` base class type chain.

- `BleMeshMetricsCollector ();`
- Purpose: construct object with default output prefix.
- Dependencies: `std::string` initialization.

- `~BleMeshMetricsCollector () override;`
- Purpose: virtual cleanup path via `ns3::Object` interface.
- Dependencies: `Object` virtual destructor chain.

- `void SetOutputPrefix (const std::string &prefix);`
- Purpose: update internal prefix value.
- Dependencies: `std::string` copy assignment.

- `std::string GetOutputPrefix () const;`
- Purpose: return current prefix value.
- Dependencies: `std::string` copy return.

- `std::string m_outputPrefix;` (private)
- Role: sole internal state in this chunk.

### `ble-mesh-metrics-collector.cc`

- `NS_OBJECT_ENSURE_REGISTERED (BleMeshMetricsCollector);`
- Purpose: ensures registration object code is linked and type is available at runtime.

- `TypeId BleMeshMetricsCollector::GetTypeId (void)`
- Creates a function-local static `TypeId` once.
- Declares type name as `"ns3::BleMeshMetricsCollector"`.
- Sets parent class to `Object`.
- Assigns ns-3 group name `"BleMeshDiscovery"`.
- Registers default constructor via `AddConstructor<BleMeshMetricsCollector>()`.

- `BleMeshMetricsCollector::BleMeshMetricsCollector () : m_outputPrefix ("ble_mesh_metrics") {}`
- Initializes default prefix to `"ble_mesh_metrics"`.

- `BleMeshMetricsCollector::~BleMeshMetricsCollector () = default;`
- No custom destruction behavior.

- `void BleMeshMetricsCollector::SetOutputPrefix (const std::string &prefix)`
- Replaces `m_outputPrefix` with caller-provided string.

- `std::string BleMeshMetricsCollector::GetOutputPrefix () const`
- Returns current prefix by value.

## Current Behavior Summary

- The class is constructible through ns-3 type system.
- Default output prefix is `ble_mesh_metrics`.
- Prefix can be overwritten and queried.
- No side effects occur when setting prefix.
- No files are opened or written.
- No metrics are gathered or emitted.

## Highlights

- Correct baseline ns-3 object integration (`SetParent<Object>`, constructor registration, runtime registration macro).
- Small, stable API surface that is easy to extend.
- Deterministic default configuration for prefix.

## Potential Issues / Gaps

- Name/implementation mismatch: class is called a metrics collector but currently stores only a string.
- No ns-3 `Attribute` exposure for `m_outputPrefix`; cannot configure via standard attribute/config paths.
- `SetOutputPrefix` accepts any string with no validation (empty value, path separators, invalid filename chars).
- `GetOutputPrefix` returns by value; repeated calls copy the string (small cost now, but avoidable).
- No trace sources, callbacks, counters, or aggregation state.
- No logging (`NS_LOG_*`) for observability.
- No tests in this chunk verifying defaults, setter/getter behavior, or type registration expectations.

## Prerequisites and Strict Embedded Design Requirements

The following are non-optional requirements for using this chunk as part of an embedded-target BLE mesh system implementation.

- Functional completeness:
- `BleMeshMetricsCollector` must implement Chunk 7 metric outputs: convergence (`cycles`, `seconds`), E2E latency, PDR, control overhead, cluster-size distribution, and invariant counters.
- Synthetic post-convergence data-plane traffic generation (or equivalent measurement hooks) must exist for E2E/PDR.

- Determinism and reproducibility:
- Static-profile runs must satisfy deterministic replay (`D0`) for fixed seed and timeline.
- Metrics/export ordering must be deterministic (stable key order, stable record ordering, explicit schema version).

- Bounded resources (embedded mandatory):
- Memory use must be bounded and pre-budgeted; no unbounded growth containers in long-running paths.
- Per-cycle/per-packet collector work must have bounded runtime; avoid blocking file I/O in timing-critical paths.
- Collector failure must not break discovery/election/cluster formation control flow (fail-open behavior).

- Storage and export safety:
- CSV/JSON artifacts must be versioned and parseable; schema changes require explicit version bump/migration note.
- Export path/prefix inputs must be validated/sanitized (empty string, illegal characters, traversal patterns).
- Write policy must minimize flash wear (batched flush/interval policy) and tolerate storage unavailability without crashing protocol logic.

- Concurrency and integration discipline:
- If called from multiple execution contexts, collector state updates must be synchronized (single-writer model or explicit locking).
- Snapshot/export operations must avoid data races and partial-write corruption.

- Verification requirements:
- Unit tests must cover default config, setters/getters, metric formulas, export serialization, and validation failures.
- Integration tests must verify parseable CSV+JSON output on `20`, `150`, and `1000+` profiles.
- Regression gates must include invariant checks and zero fatal invariant breaks in large-scale (`1000+`) runs.

- Industry-standard quality controls:
- Enforce fixed coding standard/lint profile for embedded-target code paths (for example MISRA/CERT-aligned subsets where project policy requires it).
- Track CPU, RAM, and storage budgets as CI-checked limits for each profile tier (`20`, `150`, `1000+`).

## Extension Points (if this chunk is expanded later)

- Add explicit metrics state (per-node counters, discovery latency, packet statistics).
- Add flush/export methods and output backend abstraction.
- Promote `m_outputPrefix` to an ns-3 attribute for runtime configurability.
- Add validation and sanitization rules for prefix values.
- Add unit/integration tests around object lifecycle and output behavior.

## Quick Reference

- Class: `ns3::BleMeshMetricsCollector`
- Namespace: `ns3`
- Base class: `ns3::Object`
- Group name: `BleMeshDiscovery`
- Default prefix: `ble_mesh_metrics`
- Public mutator/accessor: `SetOutputPrefix`, `GetOutputPrefix`
