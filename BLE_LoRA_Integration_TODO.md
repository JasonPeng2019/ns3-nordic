# BLE + LoRA Integration TODO

Goal: extend the current BLE-only discovery/election + hash/slotting work to include LoRA discovery, clusterhead election, and hybrid communication per the Compass Project Technical Document. Deliver a unified simulation that models both radios, their mode cadence, and the interaction between BLE clusterheads and LoRA clusterheads.

## Principles from the PDFs
- Discovery: BLE on a single channel; LoRA supports up to 56 channels with channel voting/migration based on crowding/RSSI.
- Clusterhead election: 3-round BLE CH election with hash h(ID); LoRA CH election based on connectivity/crowding.
- Hash distribution: BLE CH distributes hash; edges derive listen slots from (cluster_hash + edge_id); three iterations per messaging window.
- Mode cadence: Mode1 (1.5s) BLE CH↔Edge + LoRA CH↔CH; Mode2 (1.5s) BLE CH↔CH + LoRA CH↔Edge/CH↔Edge; pipeline repeats.
- CH listen ratio: ~75% listen / 25% TX during messaging phases.

## Workstream 1: LoRA Protocol Core
- Status: scaffolding expanded further. Packets/nodes include channel/home/target, crowding, CH flag; engine emits discovery packets each tick, ignores off-channel traffic, bumps crowding on strong RSSI, and adopts lowest-ID CH seen. Simple serialization/deserialization added for discovery packets. Still missing real discovery/channel voting, CH election, hash/slotting.
- Define pure-C LoRA packet structs (discovery, CH declaration, search, data) and serialization. (Partial: discovery serialize/deserialize done; search/data still TODO)
- Implement LoRA discovery logic: channel listen, CAD/RSSI crowding assessment, channel voting/migration, crowding factor calculation, TTL/PSF, GPS. (TODO)
- Implement LoRA CH election: candidate criteria, CH declaration, conflict resolution (higher direct count, lower ID tie-breaker), cluster formation. (TODO)
- Implement LoRA hash/slotting: stochastic slot assignment for LoRA Edge↔CH and CH↔CH; reuse hash mix functions where possible; derive listen slots per PDF. (TODO)
- Add LoRA node state machine: channel home/k, crowding factor, CH/edge roles, neighbor table, stats. (TODO: expand beyond current minimal fields)
- Unit tests: serialization, channel migration decisions, crowding/voting, CH election tie-breakers, hash→slot mapping. (TODO)

## Workstream 2: Engine Integration (C core)
- Extend engine to own two radio contexts: BLE and LoRA, each with its own discovery phases, timing, and queues.
- Add Mode1/Mode2 scheduler (1.5s each) that toggles which radio runs which role: Mode1 BLE CH↔Edge, LoRA CH↔CH; Mode2 BLE CH↔CH, LoRA CH↔Edge/CH↔Edge.
- Implement 75% listen / 25% TX weighting for clusterheads during messaging windows.
- Gate TX/RX by slots for both radios; three iterations per messaging window for probabilistic delivery.
- Support LoRA channel migration and home channel tracking during discovery/rediscovery.
- Metrics: track per-radio slots_tx/rx/collision/empty, CH counts, channel crowding, mode transitions.

## Workstream 3: NS-3 Wrapper & Attributes
- Status: placeholder `LoraEngineWrapper` added (NodeId, SlotDuration, InitialChannel, MaxChannels, TX callback). No attributes for slots/voting yet; no traces.
- Add LoRA attributes: channel count, CAD/RSSI thresholds, crowding vote thresholds, TDMA slots, frame durations, hash/slot enable flag. (TODO)
- Add Mode1/Mode2 attributes (duration, iteration count) and CH listen ratio knobs. (TODO)
- Expose separate trace sources for BLE and LoRA SlotOutcome, election events, and channel migration events. (TODO)
- Ensure wrapper wiring passes mode cadence and per-radio configs into the C core. (TODO)

## Workstream 4: Hybrid Simulation Program
- Status: basic hybrid driver added (`phase4-hybrid-sim.cc`) wiring BLE and LoRA wrappers over simple channels; LoRA still placeholder behavior.
- TODO: evolve hybrid sim to enforce Mode1/Mode2 roles, slot gating, channel migration, and logging/tracing per PDF.

## Workstream 5: Rediscovery & Failure Handling
- Implement 30-minute (simulated) rediscovery hooks: BLE rediscovery in Mode1, LoRA rediscovery in Mode2.
- Local failure handling: CH failure detection, temporary failover, queued messages; backup routing via cached PSF/paths.
- Tests/sims to validate rediscovery triggers and failover behavior.

## Workstream 6: Documentation & TODO Updates
- Document BLE+LoRA packet formats, mode cadence, slot derivation, channel migration policy.
- Update TODO.md and PHASE docs with hybrid milestones, test commands, and acceptance criteria.
- Update merge_report.md when hybrid integration is merged into mainline.

## Milestone Demos
- **Milestone 1 (BLE+LoRA discovery):** Both radios discover/elect CHs independently; hashes/slots derived; Mode cadence placeholders active.
- **Milestone 2 (Hybrid Mode1/Mode2):** Mode scheduler toggles radio roles; slot-gated delivery with three iterations; CH listen ratio enforced; traces show radio-tagged SlotOutcome events.
- **Milestone 3 (Channel migration & rediscovery):** LoRA channel voting/migration works; rediscovery hooks run at scheduled intervals.
