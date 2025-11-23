# Phase 3 Engine Integration TODO

This document captures the detailed tasks required to evolve the Phase‑2 event engine (`engine-core/ble_discovery_engine.c`) into the full Phase‑3 state machine described in `discovery_protocol.txt`. Tasks are grouped so they can be implemented in independent commits. For each item, update the relevant docs (`TODO.md`, `future_report.md`, `merge_report.md`, etc.) as we progress.



Original prompt:


can you heavily and carefully augment my entire engine so that not only forwarding is included, but also states, different phases of discovery, and all the things you mentioned are included for a full phase 3 implementation? make sure the engine lines up perfectly with how the process is described in discovery_protocol.txt, and use the C-engine_TODO.md as reference. for any conflicts between the TODO and protocol, prioritize the discovery_protocol.txt and update C-engine_TODO.md accordingly. for any unfinishable or incomplete helpers that can't translate into a full, completed engine, augment the C-engineTODO to reflect those as things need to be finished. make sure, when you implement, that everything is correct and factors in all possible factors. prioritize the actual C core engine/state machine.

---

## Current Status (2025-XX-XX)

| Area | Status | Notes |
|------|--------|-------|
| A. Phase scheduling / RSSI window | ✅ Implemented (`ble_discovery_engine.c`: phase state machine wraps `ble_broadcast_timing`, noisy window is the only time RSSI samples are recorded, neighbor adds gated to discovery sub-phase). |
| B. Post-cycle evaluation & metrics | ✅ Implemented (cycle completion now snapshots metrics via `metrics_cb`, prunes neighbors, and automatically transitions between DISCOVERY/EDGE/CANDIDATE with the 6→3→1 requirement). |
| C. Election announcement generation | ✅ Implemented (engine constructs `ble_election_packet_t`, seeds ΣΠ history, tracks three rounds, and emits one announcement per cycle while candidate). |
| D. Election reception & propagation | ✅ Implemented (full election packets preserved in queues/wrappers, forwarding enforces PDSF capacity, conflict detection demotes weaker candidates, and renouncement floods now reuse `ble_election_packet_t` with the `is_renouncement` flag). |
| E. Documentation updates | ✅ Files updated to reflect the completed engine work and renouncement feature. |

## Remaining Work

1. **Regression Tests**
   - Add NS-3/C automation that exercises Phase‑3 behaviors (three-round announcements, renouncement floods, capacity cutoff, demotion path).

2. **Phase 4 Planning**
   - With Phase 3 complete, prepare the Phase‑4 roadmap (cluster formation, renouncement analytics, etc.).
