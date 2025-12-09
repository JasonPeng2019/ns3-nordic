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
- Decide slot space: number of TDMA slots per frame and FDMA channels (e.g., 16 TDMA slots × 3 channels = 48 possible buckets).
- Define deterministic mapping: `bucket = hash % (slots * channels)`; `channel = bucket / slots`; `slot_index = bucket % slots`.
- Collision policy: document what happens when >bucket_count clusterheads exist (e.g., simple collision model, or secondary offset using LP/PDSF entropy).
- Parameterize so sims can sweep slot counts and see collision rates.
- Respect PDF semantics: Clusterhead generates and distributes hash; edges derive **their listening slots** from CH hash + edge ID; each messaging phase runs **three iterations** of the slot schedule for probabilistic delivery.

### 2) Core Protocol Updates (C, portable; C++ wrapper for NS-3 integration)
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
- In copies `ble-discovery-engine-wrapper.*.hashwip`:
  - Add attributes: `FdmaChannels`, `TdmaSlots`, `TdmaFrameDuration`, `DataStartDelay`, `EnableDataPhase`, `EnableCollisionModel`.
  - Pass new params into C config; expose new trace source `SlotOutcome` (fields: node_id, role, frame_idx, channel, slot, outcome).
  - Add attributes for Mode1/Mode2 durations (default 1.5s each) and per-mode iteration count (default 3 slot iterations) as **placeholders only**; do not implement full mode sequencing in this pass.
  - Ensure wrapper can be built without touching originals; switch build to copies when testing.

### 5) Simulation Program Upgrade (Phase 3 → Hash/Timeslot; C++ sim over C core)
- In copied sim `phase3-hash-timeslot-sim.cc`:
  - Add post-discovery wait for election completion, then start TDMA frames; include **Mode1/Mode2 config hooks only** (no full alternation logic), with **three slot iterations per messaging phase**.
  - Reuse `SimpleVirtualChannel` with channel awareness: deliveries only if sender/receiver share channel; collisions when simultaneous same `(channel, slot)`.
  - Extend CSV header to: `... fdma_channel,tdma_slot,frame_idx,slot_event,slot_success`.
  - Add CLI knobs: `--fdmaChannels`, `--tdmaSlots`, `--frameMs`, `--dataDurationMs`, `--slotSendSize`, `--simulateCollisions=true|false`, `--dataStartDelayMs`.
  - Inject synthetic collision scenarios (e.g., force two edges to same bucket) for testing.
- In copied visualizer `phase3_visualizer_hash.py`:
  - Render per-frame slot heatmap, collision markers, per-channel separation, and counts of successes vs collisions.

### 6) Acceptance Tests & Regression (C core + C++ integration)
- C tests: hash/slot determinism, collision mapping, slot arithmetic (frame rollover).
- C++/NS-3 tests:
  - Small topology (3 nodes): ensure edges TX only in their slots and clusterhead receives when unique.
  - Collision case: two edges hashed to same bucket → collision counter increments, no delivery.
  - Multi-channel case: edges on different channels but same slot both succeed.
  - Parameter sweep sanity: changing `tdmaSlots` or `fdmaChannels` changes derived slot indices in traces.
- Mode placeholder sanity: verify Mode1/Mode2 attributes are accepted and passed through but not functionally altering TDMA behavior yet.
- Simulation smoke tests: run new sim with `--nodes=20 --tdmaSlots=8 --fdmaChannels=2` and assert >0 successful slot deliveries and recorded collisions when forced.

### 7) Documentation & TODO Updates
- Update `docs/PHASE4-OVERVIEW.md` Task 22 with the implemented mapping rules, collision policy, and new parameters.
- Add a short “How slots are derived” section to `doc/packet-format.md` (hash interpretation) and `README.md` (simulation knobs).
- Refresh `TODO.md` to reflect completed hash/slot tasks and any remaining Phase 5 items.
- Document Mode1/Mode2 timing placeholders (1.5s each) and three-iteration messaging window for future work, while clarifying that this pass only implements hash/slot mechanics; CH-distributed hash semantics (edges compute listen slots) per Compass PDF, and the hash h(ID) FDMA/TDMA role per Clusterhead PDF.
- Document copy→promote workflow in `merge_report.md` so others know which files are authoritative during review.

## Exit Criteria
- Hash in packets deterministically maps to `(channel, slot)` using configurable counts; edges and clusterheads agree without extra signaling.
- Post-discovery TDMA/FDMA stage runs in NS-3, producing traces that show successes and collisions, with three slot iterations per messaging window; Mode1/Mode2 support is limited to configuration placeholders (no full alternation behavior yet).
- New tests cover hash determinism, slot derivation, and basic slot-level delivery/collision behavior.
- Backups removed/ignored and primary files kept merge-friendly. 
