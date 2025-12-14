# Hash & Timeslot Integration TODO

This plan stitches the PDF requirements (hash-based FDMA/TDMA slotting after cluster formation) into the existing Phase 3 NS-3 simulation. It assumes the current codebase state in `ns3_dev/ns3-ble-module/ble-mesh-discovery` (Phase 3 engine and sims working, hash present in packets but unused for scheduling) and must stay consistent with **both** protocol PDFs:
- **Clusterhead BLE Mesh discovery process**: election floods, PDSF-based capacity, hash h(ID) for FDMA/TDMA, hash used to reach clusterhead, three election rounds.
- **Compass Project Technical Document**: post-discovery communication uses clusterhead-distributed hash; edges derive **listening** time slots from the CH hash + their device ID; messaging phases iterate three times per communication window; Mode1/Mode2 pipeline (1.5s each) with CH listen heavy (~75%) vs TX (~25%); hash/slots reused for CH↔edge data exchange.

**Scope for this effort:** implement hash→FDMA/TDMA slot mapping and the TDMA data-phase mechanics. Only add a lightweight skeleton for Mode1/Mode2 timing (placeholders/config hooks); do **not** build full Mode1/Mode2 behavior in this pass.

## Copy-First Workflow (to ease merging later)
- Requirement: do **all edits in copied files, not the originals**, so the untouched files remain easy to diff/merge. Only when stable, explicitly promote the copies (rename/mv) into place in a single commit/step.
- Suggested copy commands (run inside `ns3_dev/ns3-ble-module/ble-mesh-discovery`):
  - `cp model/protocol-core/ble_discovery_packet.c model/protocol-core/ble_discovery_packet.c.hashwip`
  - `cp model/protocol-core/ble_discovery_packet.h model/protocol-core/ble_discovery_packet.h.hashwip`
  - `cp model/protocol-core/ble_mesh_node.{h,c} model/protocol-core/ble_mesh_node.{h,c}.hashwip`
  - `cp model/engine-core/ble_discovery_engine.{h,c} model/engine-core/ble_discovery_engine.{h,c}.hashwip`
  - `cp model/ble-discovery-engine-wrapper.{h,cc} model/ble-discovery-engine-wrapper.{h,cc}.hashwip`
  - `cp engine_sim/phase3-discovery-sim.cc engine_sim/phase3-hash-timeslot-sim.cc`
  - `cp engine_sim/tools/phase3_visualizer.py engine_sim/tools/phase3_visualizer_hash.py`
  - Copy any test/doc you touch (`*.hashwip` suffix) and update `.gitignore` to hide them until promotion.
- Build/CI should temporarily point to the `*.hashwip` sources (adjust `wscript`/`CMakeLists` in copies only) or use a feature branch. When satisfied, replace originals with the `hashwip` versions in one controlled rename step.

## Current Status Snapshot
- Hash: `ble_election_generate_hash()` (FNV-1a) exists; hash is stored in `ble_election_packet_t`, initialized in `ble_mesh_node`, and emitted in Phase 3 traces, but **no slot mapping/use** exists.
- Discovery timing: `ble_broadcast_timing` covers noisy + neighbor slots (Tasks 12/14), not FDMA/TDMA data slots.
- Engine: Phase 3 state machine/election flooding/renouncement complete; no post-election slot scheduler or cluster-member TX logic.
- Simulation: `phase3-discovery-engine-sim` stops at election/alignment; CSV includes hash column only for observability.
- Docs/TODO: Task 22 (FDMA/TDMA hash + slot mapping) still unchecked; Phase 5+ (cluster formation/data plane) not implemented.
## Integration Plan

### 1) Finalize Hash → Slot/Frequency Spec (PDF alignment)
- Decide slot space: number of TDMA slots per frame and FDMA channels (e.g., start with 8–16 slots and 1–3 channels; bucket_count = slots × channels).
- Define deterministic mapping: `bucket = hash % (slots * channels)`; `channel = bucket / slots`; `slot_index = bucket % slots`.
- Collision policy: if clusterheads > bucket_count, shared buckets collide; keep simple collision model now and note potential secondary offset using LP/PDSF entropy for future refinement.
- Parameterize so sims can sweep slot counts and see collision rates.
- Respect PDF semantics: Clusterhead generates/distributes hash; edges derive **listening slots** from CH hash + edge ID; each messaging phase runs **three iterations** of the slot schedule for probabilistic delivery (Mode1/Mode2 remain placeholders).
- Status: IMPLEMENTED in `*.hashwip` core helpers: added `ble_hash_map_to_slot`, `ble_hash_combine_cluster_edge`, and `ble_hash_map_edge_slot` (edge = CH hash + edge ID) with test stub `ble-hash-slot-c-test.c.hashwip`; ready to use in higher layers.

### 2) Core Protocol Updates (C, portable; C++ wrapper for NS-3 integration)
- Status: DONE in `*.hashwip` copies (`ble_discovery_packet` + `ble_mesh_node`), plus a minimal C test stub `ble-hash-slot-c-test.c.hashwip`.
- In copies `ble_discovery_packet.*.hashwip`:
  - Add pure functions `ble_hash_map_to_slot(hash, slots, channels, &slot_idx, &channel_idx)` and `ble_hash_next_frame_start(now_ms, frame_ms, slot_idx, slots)`; keep serialization unchanged.
  - Keep `ble_election_generate_hash()` but allow injection of slot params (struct in config) for deterministic tests.
- In copies `ble_mesh_node.*.hashwip`:
  - Store derived slot/channel for self (clusterhead) and for selected clusterhead; include `frame_len_ms`, `next_slot_time_ms`, `slot_collision_count`.
  - Add helpers to set/clear slot state when role changes; include reset on renouncement/edge transition.
  - Track whether a node is in **listener-slot** role (edge) vs **talk-slot** role (clusterhead), per Compass doc (edges compute listen slots from CH hash).
- C++ wrappers should remain thin shims around the C helpers for NS-3 (no duplicate logic).
- Add C-unit tests (new `test/ble-hash-slot-c-test.c` copy):
  - Determinism and distribution of hash→slot mapping.
  - Edge/clusterhead derive identical slot/channel from the same hash.
  - Boundary IDs (0, max uint32) map into valid buckets.
  - Frame/slot arithmetic rollover (slot index wraps; next frame).

### 3) Engine State Machine Wiring (C core; C++ wrapper integration)
- Status: COMPLETE in `ble_discovery_engine.*.hashwip` (TDMA/FDMA defaults/placeholders; self-slot assignment on init/entering candidate; cluster slot assignment on adopting a clusterhead; cluster slot cleared on renouncement/clear; mode placeholders stored; slot counters/refresh helper; data-phase start/end/iteration hooks; slot event callback + collision gating in `ble_engine_gate_and_record_slot`; TX/forward/RX paths gated by slot with collision counts; autonomous empty-slot sampling during data phase; slot counters surfaced via metrics; slot events surfaced to wrapper trace; data phase aligned to Mode1 cadence; wrapper attributes for data/collision knobs).
  - Recent: Added slot event struct/callback, data-phase start/end/iteration hooks, and wrapper trace `SlotOutcome`. TDMA scheduler still placeholder (no automatic slot-driven TX/RX in engine yet); collision handling is callback-driven only.
- In copies `ble_discovery_engine.*.hashwip`:
  - On entering `CLUSTERHEAD_CANDIDATE`, compute/store `(channel, slot)` and expose via metrics/log hook.
  - On edge alignment, compute `(channel, slot)` from received hash and store frame config.
  - Add TDMA scheduler: track `data_phase_start_ms`, `frame_len_ms`, **three iterations per messaging window** (Compass doc), and schedule per-slot callbacks; gate TX only at own slot and channel.
  - Collision modeling hook: if multiple edges target same `(channel, slot, frame)` into same clusterhead, set collision flag and drop delivery.
  - Add **skeleton only** for Mode1/Mode2 toggling (config fields, stub state, default 1.5s values) without implementing full pipeline logic; TDMA scheduler can treat modes as optional gating flags but not enforce behaviors beyond placeholders.
  - Reset slot state on demotion/renouncement and when re-running discovery.
  - Track metrics: `slots_tx`, `slots_rx`, `slots_collision`, `slots_empty`.
- Keep core scheduling/metrics in C; expose via wrapper callbacks/trace only.

### 4) NS-3 Wrapper & Attributes (C core logic; C++ glue only)
- Status: COMPLETE in `ble-discovery-engine-wrapper.*.hashwip` (attributes for TDMA/FDMA + mode placeholders plumbed through config; SlotOutcome trace wired; collision/data-phase toggles exposed; metrics now include slot counters; data phase aligned to Mode1 duration with gating/trace hook-up).
- In copies `ble-discovery-engine-wrapper.*.hashwip`:
  - Add attributes: `FdmaChannels`, `TdmaSlots`, `TdmaFrameDuration`, `DataStartDelay`, `EnableDataPhase`, `EnableCollisionModel`.
  - Pass new params into C config; expose new trace source `SlotOutcome` (fields: node_id, role, frame_idx, channel, slot, outcome).
  - Add attributes for Mode1/Mode2 durations (default 1.5s each) and per-mode iteration count (default 3 slot iterations) as **placeholders only**; do not implement full mode sequencing in this pass.
  - Ensure wrapper can be built without touching originals; switch build to copies when testing.

### 5) Simulation Program Upgrade (Phase 3 → Hash/Timeslot; C++ sim over C core)
- Status: COMPLETE in `phase3-hash-timeslot-sim.cc` and visualizer.
- `phase3-hash-timeslot-sim.cc`:
  - Slot fields included in trace header; SEND/RECV rows emit blank slot fields, SLOT_EVENT rows emit slot/channel/frame/iteration with success bit.
  - CLI knobs for FDMA/TDMA/frame and collision toggle; nodes apply TDMA/FDMA config.
  - SimpleVirtualChannel gates delivery by channel bucket; wscript builds the new sim target.
  - SlotOutcome trace consumed and logged to CSV.
- `phase3_visualizer_hash.py`:
  - Updated COLUMNS to include slot fields; loader parses numeric slot columns; ready to visualize slot events (heatmap rendering can be added as needed).

### 6) Acceptance Tests & Regression (C core + C++ integration)
- Status: COMPLETE (with runnable commands documented).
- C tests: hash/slot determinism, collision mapping, slot arithmetic (frame rollover) via `ble-hash-slot-c-test.c.hashwip` (build/run: `clang -std=c99 -Wall -Werror -I model/protocol-core -include model/protocol-core/ble_discovery_packet.h.hashwip -x c test/ble-hash-slot-c-test.c.hashwip -x c model/protocol-core/ble_discovery_packet.c.hashwip -o /tmp/ble-hash-slot-c-test && /tmp/ble-hash-slot-c-test`).
- C++/NS-3 validation: use the wrapper-integrated sim `phase3-hash-timeslot-sim`:
  - Small topology sanity: `./waf --run "phase3-hash-timeslot-sim --nodes=3 --fdmaChannels=1 --tdmaSlots=4 --simulateCollisions=0"` and inspect CSV for SEND/RECV only (no SLOT collisions).
  - Collision case: `./waf --run "phase3-hash-timeslot-sim --nodes=5 --fdmaChannels=1 --tdmaSlots=2 --simulateCollisions=1"` and verify SLOT_EVENT rows with `slot_success=0` and collision counts in metrics trace.
  - Multi-channel case: `./waf --run "phase3-hash-timeslot-sim --nodes=5 --fdmaChannels=2 --tdmaSlots=4 --simulateCollisions=1"`; expect collisions to drop when channels differ.
  - Parameter sweep sanity: vary `--tdmaSlots/--fdmaChannels` and confirm slot indices in SLOT_EVENT rows change accordingly.
- Mode sanity: Mode1/Mode2 attributes carried through; data phase ends after Mode1 duration; Mode2 toggle unused (placeholder).
- Simulation smoke: `./waf --run "phase3-hash-timeslot-sim --nodes=20 --tdmaSlots=8 --fdmaChannels=2 --simulateCollisions=1 --trace=phase3_trace.csv"`; visualize via `engine_sim/tools/phase3_visualizer_hash.py --trace phase3_trace.csv` (slot columns parsed).

### 7) Documentation & TODO Updates
- Status: COMPLETE.
- `docs/PHASE4-OVERVIEW.md` updated: hash→slot mapping, collision policy, and TDMA/FDMA parameters documented.
- `doc/packet-format.md` and `README.md` updated with slot-derivation summary and sim knobs.
- `TODO.md` reflects completed hash/slot work and remaining Phase 5 items.
- Mode1/Mode2 placeholders and three-iteration messaging noted; CH-distributed hash semantics and FDMA/TDMA role documented per PDFs.
- `merge_report.md` records the copy→promote workflow and which `*.hashwip` files hold authoritative changes.

### 8) Mainline Integration & Scheduling (no more hashwip-only)
- Port the hash/timeslot logic from `*.hashwip` into the primary sources (`ble_discovery_packet.*`, `ble_mesh_node.*`, `ble_discovery_engine.*`, wrapper, sim) so the default build uses slotting without duplicate files.
- Status: In progress/mainlined. Core engine now gates data-plane TX/RX via `ble_engine_gate_and_record_slot`, emits SlotOutcome events, and enforces three iterations inside a Mode1/Mode2 cadence (data phase restarts after Mode2). Gating only applies to post-discovery/election traffic; data phase starts after CH election/alignment. Sim keeps discovery single-channel and traces channel/slot/frame/iteration for data traffic; collisions keyed per receiver+channel+slot+frame. Hash-slot C test promoted into mainline and built via waf. Remaining: clean up hashwip artifacts and update merge_report.md when copies are retired.

## Exit Criteria
- Hash in packets deterministically maps to `(channel, slot)` using configurable counts; edges and clusterheads agree without extra signaling.
- Post-discovery TDMA/FDMA stage runs in NS-3, producing traces that show successes and collisions, with three slot iterations per messaging window; Mode1/Mode2 support is limited to configuration placeholders (no full alternation behavior yet).
- New tests cover hash determinism, slot derivation, and basic slot-level delivery/collision behavior.
- Backups removed/ignored and primary files kept merge-friendly. 

## New Follow-ups (to close the remaining gaps)
- Promote the hash/timeslot code into the primary sources, delete/retire `*.hashwip`, and ensure waf builds the mainline files.
- Ensure edge listening slots derive from `cluster_hash + edge_id` (`ble_hash_map_edge_slot`) and both roles track channel+slot and frame timing in-node metrics.
- Make `EnableDataPhase`/`EnableCollisionModel` effective end-to-end; default data phase on in sims so slot callbacks and collisions actually occur.
- Enforce TDMA gating for TX/RX with SlotOutcome emissions and three-slot iterations per messaging window; mode cadence bounds the data phase (1.5s Mode1 skeleton).
- Keep discovery single-channel; apply TDMA/FDMA only to data-phase traffic and populate slot/iteration fields in SEND/RECV/SLOT_EVENT traces.
- Add hash-slot C test and hash/timeslot sim runs into the CI/test matrix to exercise mapping/collision behavior.
