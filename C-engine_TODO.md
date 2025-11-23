# BLE Discovery Event Engine TODO
#note: arrange all concurrency, transmission, etc. into memory so that the next tick can actually instantiate 1,000 of these devices and know which successfully communicated, what settings, etc.

Portable C event loop driving the discovery cycle, message handling, and node state logic. Build it incrementally alongside the phases outlined in `TODO.md`.

## Phase 2 Scope (Helpers should already exist)

1. **Define Engine Context Struct**
   - Aggregate node state, discovery cycle, message queue, forwarding logic, timers.
   - Include callback pointers for platform services (radio TX/RX, RNG, logging).

2. **Public API & Configuration**
   - `ble_engine_init(ctx, config)` to zero state, wire callbacks, load defaults.
   - Setters for runtime parameters (slot duration, GPS mode, proximity threshold).

3. **Slot Scheduler Integration**
   - Embed `ble_discovery_cycle_t` inside the engine.
   - Register internal handlers for slot 0 (own transmit) and slots 1–3 (forwarding).

4. **Engine Tick Entry Point**
   - Implement `ble_engine_tick(ctx, current_time_ms)`:
     - Execute the current slot’s handler.
     - Process RX/TX queues, timers, and pending state transitions.
     - Advance to the next slot and prepare follow-up work.

5. **Platform Abstraction Layer (Phase 2 minimum)**
   - Define function pointers for radio send/receive, RNG, logging, time source.
   - Allow NS-3 wrapper or embedded firmware to supply implementations at init.

## Phase 3–4 Extensions (Tasks 12–27 as helpers land)

6. **Message Handling Glue**
   - Integrate noisy broadcast results, queue operations, forwarding decisions, TTL/PDSF updates in one place.
   - Ensure dedupe, proximity checks, and capacity limits are enforced before TX.

7. **Node State Machine Hooks**
   - Invoke `ble_mesh_node_*` transitions based on metrics gathered each cycle (edge vs. candidate states).
   - Update candidacy scores, neighbor stats, GPS cache, and noise level inputs within the engine.
   - Track when other clusterhead candidates were last heard and apply the dynamic (6→3→1) candidacy threshold logic; trigger election-announcement preparation when becoming candidate.

8. **Election Flooding & Capacity Controls**
   - Handle multi-round election announcements, PDSF thresholds, renouncement logic once those helpers are implemented.

## Phase 5+ (Routing & Advanced Features)

9. **Routing / Path Management**
   - Integrate multi-path tracking, Dijkstra shortest-path helpers, and routing tree construction when those modules are ready.

## Cross-Cutting Work (Phases 6–8 as infrastructure matures)

10. **Full Platform Abstraction**
    - Expand callback surface for mobility updates, logging/tracing, topology generators, etc.

11. **NS-3 Wrapper**
    - Build `BleEventEngineWrapper` that owns the C context and schedules `Tick()` via `Simulator::Schedule`.
    - Map NS-3 services (SendPacket, GetPosition, RNG streams) into the core callbacks.

12. **Testing**
    - Pure C unit tests simulating multiple ticks/cycles to verify callback ordering and state updates.
    - NS-3 integration tests ensuring the wrapper drives the engine deterministically.

13. **Documentation & Examples**
    - Document the engine API (init, tick, callback contracts) in the C-core docs.
    - Add an NS-3 example program demonstrating the event engine in action.
