# BLE Discovery Event Engine TODO
#note: arrange all concurrency, transmission, etc. into memory so that the next tick can actually instantiate 1,000 of these devices and know which successfully communicated, what settings, etc.

Portable C event loop driving the discovery cycle, message handling, and node state logic. Build it incrementally alongside the phases outlined in `TODO.md`.

## Phase 2 Scope (Completed)
- Context struct, public API, slot scheduler, tick entry point, and basic forwarding logic exist today. Remaining work focuses on Phase 3/4 features below.

## Phase 3 Integration Tasks

1. **Phase Scheduler & RSSI Window** ✅
   - `ble_discovery_engine.c` now orchestrates noisy/neighbor sub-phases through `ble_broadcast_timing`, gates RSSI sampling to the noisy window, and limits neighbor adds to the direct-discovery window. Stale neighbors are pruned each cycle via `ble_election_clean_old_neighbors()`.

2. **Cycle-End State Transitions** ✅
   - `ble_engine_cycle_complete()` advances statistics, snapshots metrics, and runs the 6→3→1 requirement before transitioning into EDGE/CANDIDATE states.

3. **Election Announcement Generation** ✅
   - Candidate transitions schedule three announcement rounds, build `ble_election_packet_t` (class ID, direct connections, score, hash, ΣΠ history), and emit one packet per discovery cycle until the rounds complete or another candidate wins.

4. **Election Reception & Conflict Resolution** ✅
   - `ble_engine_receive_packet()` detects election packets, skips neighbor tracking, bumps the candidate-heard cycle, and demotes the node to EDGE if a higher-ranked candidate (direct count tie-broken by sender ID) is heard.

5. **Forwarding Queue Enhancements** ✅
   - The queue stores full `ble_election_packet_t` entries; election forwarding refreshes PDSF/Last Π and enforces the cluster capacity limit before re-broadcasting.

6. **Metrics Snapshot & Logging** ✅
   - Metrics are updated once per cycle and optionally surfaced through the new `metrics_cb` hook; `ble_mesh_node_update_statistics()` keeps forwarding stats in sync.

7. **Renouncement Broadcasts & Documentation** ✅
   - The engine now floods three renouncement announcements (reusing `ble_election_packet_t` with `is_renouncement=true`) whenever it yields, and all Phase‑3 docs have been brought up to date.
