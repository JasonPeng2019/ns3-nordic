# BLE + LoRA Integration TODO

Goal: extend the current BLE-only discovery/election + hash/slotting work to include LoRA discovery, clusterhead election, and hybrid communication per the Compass Project Technical Document. Deliver a unified simulation that models both radios, their mode cadence, and the interaction between BLE clusterheads and LoRA clusterheads.

## Principles from the PDFs
- Discovery: BLE on a single channel; LoRA supports up to 56 channels with channel voting/migration based on crowding/RSSI.
- Clusterhead election: 3-round BLE CH election with hash h(ID); LoRA CH election based on connectivity/crowding.
- Hash distribution: BLE CH distributes hash; edges derive listen slots from (cluster_hash + edge_id); three iterations per messaging window.
- Mode cadence: Mode1 (1.5s) BLE CH↔Edge + LoRA CH↔CH; Mode2 (1.5s) BLE CH↔CH + LoRA CH↔Edge/CH↔Edge; pipeline repeats.
- CH listen ratio: ~75% listen / 25% TX during messaging phases.

## Workstream 1: LoRA Protocol Core
- Define pure-C LoRA packet structs (discovery, CH declaration, search, data) and serialization.
- Implement LoRA discovery logic: channel listen, CAD/RSSI crowding assessment, channel voting/migration, crowding factor calculation, TTL/PSF, GPS.
- Implement LoRA CH election: candidate criteria, CH declaration, conflict resolution (higher direct count, lower ID tie-breaker), cluster formation.
- Implement LoRA hash/slotting: stochastic slot assignment for LoRA Edge↔CH and CH↔CH; reuse hash mix functions where possible; derive listen slots per PDF.
- Add LoRA node state machine: channel home/k, crowding factor, CH/edge roles, neighbor table, stats.
- Unit tests: serialization, channel migration decisions, crowding/voting, CH election tie-breakers, hash→slot mapping.

## Workstream 2: Engine Integration (C core)
- Extend engine to own two radio contexts: BLE and LoRA, each with its own discovery phases, timing, and queues.
- Add Mode1/Mode2 scheduler (1.5s each) that toggles which radio runs which role: Mode1 BLE CH↔Edge, LoRA CH↔CH; Mode2 BLE CH↔CH, LoRA CH↔Edge/CH↔Edge.
- Implement 75% listen / 25% TX weighting for clusterheads during messaging windows.
- Gate TX/RX by slots for both radios; three iterations per messaging window for probabilistic delivery.
- Support LoRA channel migration and home channel tracking during discovery/rediscovery.
- Metrics: track per-radio slots_tx/rx/collision/empty, CH counts, channel crowding, mode transitions.

## Workstream 3: NS-3 Wrapper & Attributes
- Add LoRA attributes: channel count, CAD/RSSI thresholds, crowding vote thresholds, TDMA slots, frame durations, hash/slot enable flag.
- Add Mode1/Mode2 attributes (duration, iteration count) and CH listen ratio knobs.
- Expose separate trace sources for BLE and LoRA SlotOutcome, election events, and channel migration events.
- Ensure wrapper wiring passes mode cadence and per-radio configs into the C core.

## Workstream 4: Hybrid Simulation Program
- Create a new sim (e.g., `phase4-hybrid-sim.cc`) that instantiates nodes with both BLE and LoRA engines.
- Discovery: BLE on single channel; LoRA on voted channels. Channel model: distance-based RSSI for both; LoRA range > BLE.
- Mode scheduler: drive Mode1/Mode2, activate appropriate radio roles per window; log SEND/RECV/SLOT events with radio tag, channel/slot/frame/iteration, collisions.
- Support command-line knobs: BLE/LoRA node counts, tdmaSlots/fdmaChannels per radio, Mode durations, collision toggles, channel vote thresholds.
- Visualization: extend visualizer to plot BLE vs LoRA events, channel migrations, and slot heatmaps.

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
