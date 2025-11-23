# NS-3 BLE Mesh Discovery – Phases 3 & 4 Design Notes

Reference spec: `Clusterhead & BLE Mesh discovery process` (jason.peng, Nov 2025).

## Phase 3: Discovery/Election Foundations
- **Noisy broadcast window (PDF §3.2)**: 2s RSSI sampling only, then frozen crowding snapshot. Implemented in C core (`ble_election.c`) with window start/end; C++ wrapper auto-starts at node start and stops after window.
- **Crowding & forwarding metrics (PDF §2)**: Picky forwarding (crowding factor), GPS proximity, TTL priority implemented in `ble_forwarding_logic.c` and integrated in `BleMeshNode::ForwardQueuedMessage`.
- **Connectivity metrics**: Direct neighbors, connection:noise ratio, geographic distribution (variance of LHGPS), forwarding success tracked in election state; candidacy score now weighted across all four.
- **Candidacy gating (PDF §3)**: Thresholds on direct neighbors, conn:noise, geo spread, forwarding success; dynamic requirement relaxation via `last_candidate_heard_cycle` (6→3→1 cycles).
- **Timing**: 4-slot discovery cycle, noisy vs neighbor broadcast schedules with stochastic slot selection and crowding-adaptive TX budget (`ble_broadcast_timing.c`).
- **Serialization**: Discovery/election headers wrap the C packet core; TypeId, size, serialize/deserialize validated by unit tests.

## Phase 4: Announcements, Capacity, Conflict Resolution
- **Election payload (PDF §4)**: Announcements carry class ID, sender ID, direct neighbor count, PDSF + Last Π history, score, hash flagging clusterhead messages.
- **FDMA/TDMA hash → slot (Task 22)**: FNV-1a hash per node ID plus deterministic slot assignment helper; slot stored on candidacy/clusterhead adoption for future channel/time use.
- **3-round flooding (Task 23)**: Candidates send three announcement rounds; receipt tracking via `m_heardAnnouncements`. Direct-neighbor count is included for pre-empting weaker candidates.
- **Capacity limiting (Task 24, PDF §4)**: Announcements with PDSF ≥ 150 are dropped (not forwarded) and not adopted as clusterheads; forwarding path enforces the same.
- **Conflict resolution & tie-breaks (Task 25/26, PDF §5)**: Higher direct-neighbor count wins; ties resolved by lower device ID. Applied on receiving announcements and when a candidate hears a stronger one.
- **Renouncement (Task 27, PDF §6)**: Candidates demote to EDGE when hearing stronger announcements and broadcast a 3-round renouncement (direct=0, score=0). Clusterhead selections are cleared on renouncement reception.
- **Path/PDSF tracking**: Hop-by-hop direct counts retained; PDSF updated per hop with duplicate exclusion to mirror “stop at capacity” behavior.

## Architecture
- **C core (portable)**: Protocol logic in `src/ble-mesh-discovery/model/protocol-core/` (election, packet, node state, broadcast timing, forwarding).
- **C++ wrappers (NS-3)**: Thin wrappers in `src/ble-mesh-discovery/model/` bridge to NS-3 types, logging, and attributes.

## Testing (Phase 1–4 coverage)
- Unit suites: `ble-broadcast-timing`, `ble-discovery-cycle`, `ble-discovery-header`, `ble-forwarding-logic`, `ble-mesh-node`, `ble-message-queue`, plus legacy `ble1`, `ble2`. All passing via `./test.py -s …` after latest changes.

## Open Items (beyond PDF scope)
- No full cluster membership manager (joins/leaves) or clusterhead-to-clusterhead routing/data phase; PDF does not specify detailed membership state machine.

