# Phase 4: Clusterhead Election - Announcement & Conflict Resolution

## Overview

Phase 4 is the **election phase** of the BLE Mesh Discovery Protocol. After Phase 3 determines which nodes are clusterhead candidates, Phase 4 handles:

1. **Announcement**: Candidates broadcast their qualifications to the network
2. **Conflict Resolution**: Competing candidates are compared and winners determined
3. **Convergence**: The network settles on a stable set of clusterheads

## Where Phase 4 Fits in the Protocol

```
Phase 1-2: Foundation & Core Discovery
    ↓
    Nodes broadcast discovery messages
    Learn about neighbors (RSSI, GPS, hop count)
    ↓
Phase 3: Candidacy Determination (COMPLETED)
    ↓
    Nodes calculate crowding factor, connection:noise ratio
    Nodes decide if they should become clusterhead candidates
    ↓
═══════════════════════════════════════════════════════════
Phase 4: Announcement & Conflict Resolution (THIS PHASE)
═══════════════════════════════════════════════════════════
    ↓
    Candidates announce themselves to the network
    Competing candidates resolve conflicts
    Winners become clusterheads, losers become edge nodes
    ↓
Phase 5: Cluster Formation
    ↓
    Edge nodes align with clusterheads
    Routing trees are built
```

---

## Task Breakdown

### Task 19: Design Election Announcement Message Structure

#### Why am I doing this step?

Candidates need a standardized message format to broadcast their qualifications to the network. This message must contain all information other nodes need to:
1. Decide if this candidate is "better" than other candidates
2. Track how many devices the announcement has reached (PDSF)
3. Avoid duplicate processing (Hash)

#### Where in the protocol flow is this logic used?

- When a node transitions from `DISCOVERY` → `CLUSTERHEAD_CANDIDATE` state
- The candidate creates an election announcement and broadcasts it
- Other nodes receive, evaluate, and potentially forward the announcement
- Used throughout the 3-round flooding process (Task 23)

#### Critical Test Cases

| Test | What to Validate |
|------|-----------------|
| Field preservation | Serialize → Deserialize round-trip preserves all election fields (Class ID, PDSF, Score, Hash) |
| Message type identification | Can distinguish election announcements from regular discovery messages |
| PDSF update on forward | PDSF field correctly updated when message is forwarded |
| Size efficiency | Election message size is reasonable for BLE constraints |
| Backward compatibility | Regular discovery message handling unaffected |

---

### Task 20: Implement PDSF Calculation Function

#### Why am I doing this step?

PDSF (Predicted Devices So Far) estimates how many unique devices the election announcement has reached. This is critical because:
1. **Cluster capacity limiting**: Stop forwarding when PDSF ≥ 150 (max cluster size)
2. **Coverage estimation**: Know when the announcement has reached enough devices
3. **Efficiency**: Prevent infinite flooding by capping reach

#### Formula

```
t(x) = Σᵢ Πᵢ(xᵢ)
```

Where xᵢ = direct connections at hop i

**Example**: If hop 1 node has 10 connections, hop 2 has 8, hop 3 has 5:
- After hop 1: PDSF = 10
- After hop 2: PDSF = 10 + (10 × 8) = 90
- After hop 3: PDSF = 90 + (10 × 8 × 5) = 490

#### Where in the protocol flow is this logic used?

When a node **forwards** an election announcement, it:
1. Adds its direct connection count to the calculation
2. Updates the PDSF field in the message
3. Checks if PDSF ≥ 150 → if so, DON'T forward (Task 24)

#### Critical Test Cases

| Test | What to Validate |
|------|-----------------|
| Single hop calculation | PDSF after 1 hop = direct_connections[0] |
| Multi-hop accumulation | PDSF correctly accumulates Σᵢ Πᵢ formula |
| Zero connections edge case | Node with 0 connections doesn't break calculation |
| Overflow protection | Large connection counts don't cause integer overflow |
| Exclusion of previous devices | Previously counted devices not double-counted |

---

### Task 21: Implement Clusterhead Score Calculation

#### Why am I doing this step?

When multiple candidates compete for the same region, the network needs an objective way to rank them. The score quantifies "how good of a clusterhead would this node be?" based on:
- **Direct connections**: More connections = better coverage
- **Connection:noise ratio**: High connections with low crowding = stable candidate
- **Geographic distribution**: Well-spread neighbors = covers more area
- **Forwarding success**: Reliable message delivery = good network position

#### Where in the protocol flow is this logic used?

- Calculated when node decides to become a candidate (end of Phase 3)
- Included in election announcement message
- Used by other nodes to compare competing clusterheads (Task 25)
- Higher score wins in conflict resolution

#### Critical Test Cases

| Test | What to Validate |
|------|-----------------|
| Score ordering | Node with more direct connections scores higher |
| Weight verification | Each metric contributes correctly to final score |
| Edge vs candidate | Edge nodes score significantly lower than candidates |
| Identical inputs | Same inputs produce identical scores (deterministic) |
| Boundary values | Score handles 0 neighbors, 100% crowding, etc. |
| Score normalization | Scores are comparable across different network conditions |

---

### Task 22: Implement FDMA/TDMA Hash Function Generation

#### Why am I doing this step?

Once clusterheads are elected, they need dedicated time/frequency slots to communicate with their cluster members without collision. The hash function `h(ID)` generates a unique slot assignment based on the clusterhead's ID, ensuring:
1. **No collisions**: Different clusterheads get different slots
2. **Deterministic**: Same ID always produces same hash (other nodes can calculate it)
3. **Uniform distribution**: Slots are evenly distributed

#### Where in the protocol flow is this logic used?

- Hash is calculated when candidate becomes clusterhead
- Included in election announcement (so cluster members know the slot)
- Used in **Phase 5+** for scheduled communication (not fully implemented yet)
- Edge nodes use received hash to know when to communicate with their clusterhead

#### Critical Test Cases

| Test | What to Validate |
|------|-----------------|
| Determinism | Same node ID always produces same hash |
| Uniqueness | Different node IDs produce different hashes (low collision rate) |
| Distribution | Hashes are uniformly distributed across slot space |
| Edge cases | Hash works for ID=0, ID=MAX_UINT32 |
| Slot mapping | Hash can be mapped to valid time/frequency slot indices |

---

### Task 23: Implement 3-Round Election Announcement Flooding

#### Why am I doing this step?

A single broadcast might not reach all nodes due to:
- Collisions during transmission
- Nodes in listen mode during broadcast
- Packet loss due to interference

**3 rounds ensure high probability of coverage** - even if round 1 misses some nodes, rounds 2 and 3 provide redundancy. This is the **core mechanism** that propagates election announcements through the network.

#### Where in the protocol flow is this logic used?

- Triggered when a node becomes `CLUSTERHEAD_CANDIDATE`
- Runs 3 announcement cycles with stochastic timing (from Task 14)
- Other nodes receive, evaluate, and re-broadcast
- After 3 rounds, election should be complete

#### Critical Test Cases

| Test | What to Validate |
|------|-----------------|
| Round counting | Exactly 3 rounds occur, not more or fewer |
| Coverage improvement | More nodes receive announcement with each round |
| Timing distribution | Broadcasts are stochastically distributed (not synchronized) |
| Message deduplication | Same announcement not processed multiple times |
| Retransmission logic | Failed transmissions are retried within each round |
| Convergence | After 3 rounds, all reachable nodes have received announcement |

---

### Task 24: Implement PDSF-Based Cluster Capacity Limiting

#### Why am I doing this step?

The protocol specifies a **maximum cluster size of 150 devices**. Without this limit:
- One strong clusterhead could absorb the entire network
- Clusters would be too large for efficient intra-cluster communication
- FDMA/TDMA slots would be insufficient

**PDSF tracking** allows nodes to estimate reach and **stop forwarding** when the cluster is "full."

#### Where in the protocol flow is this logic used?

Every time a node considers forwarding an election announcement:
1. Check current PDSF in message
2. If PDSF ≥ 150 → **DO NOT FORWARD**
3. If PDSF < 150 → Update PDSF, forward normally

This creates a natural "boundary" around each clusterhead's reach.

#### Critical Test Cases

| Test | What to Validate |
|------|-----------------|
| Below threshold | PDSF=100 → message IS forwarded |
| At threshold | PDSF=150 → message NOT forwarded |
| Above threshold | PDSF=200 → message NOT forwarded |
| PDSF update before check | PDSF updated with current node's connections before comparison |
| Multiple clusterheads | Each clusterhead's announcements limited independently |
| Boundary precision | Cluster forms with ~150 members, not significantly more or less |

---

### Task 25: Implement Conflict Resolution Between Clusterheads

#### Why am I doing this step?

In a real network, multiple nodes may independently decide they're good clusterhead candidates and start announcing. When a node receives announcements from **multiple clusterheads**, it needs rules to decide which one "wins":

**Rule: Higher direct connection count wins**

This ensures the clusterhead with the best network position (most connections) serves the cluster.

#### Where in the protocol flow is this logic used?

When a node receives an election announcement from a clusterhead:
1. Compare with any previously received clusterhead announcements
2. If new clusterhead has MORE direct connections → new one wins
3. If current clusterhead has MORE → ignore new announcement
4. Update internal state to track winning clusterhead

#### Critical Test Cases

| Test | What to Validate |
|------|-----------------|
| Higher wins | Clusterhead with 50 connections beats one with 30 |
| Lower loses | Lower-connection clusterhead's announcement is ignored |
| State update | Node correctly switches allegiance when better clusterhead found |
| Propagation | Winning clusterhead's announcement continues to propagate |
| Losing announcement stopped | Losing clusterhead's announcement stops being forwarded |
| Multi-candidate scenario | 3+ competing clusterheads resolved correctly |

---

### Task 26: Implement Tie-Breaking Using Device ID

#### Why am I doing this step?

What happens when two clusterheads have **exactly the same** direct connection count? Without a tie-breaker, the network could oscillate forever.

**Rule: Lower device ID takes precedence**

This is deterministic and requires no additional communication.

#### Where in the protocol flow is this logic used?

During conflict resolution (Task 25), if connection counts are equal:
1. Compare device IDs
2. Lower ID wins

Applied consistently by all nodes (deterministic outcome).

#### Critical Test Cases

| Test | What to Validate |
|------|-----------------|
| Lower ID wins | Same connections, ID=100 beats ID=200 |
| Higher ID loses | Same connections, ID=500 loses to ID=100 |
| Tie-break only when equal | If connections differ, ID is NOT compared |
| Determinism | All nodes reach same conclusion given same inputs |
| Edge case IDs | ID=0, ID=1, ID=MAX work correctly |

---

### Task 27: Implement Candidacy Renouncement Mechanism

#### Why am I doing this step?

When a candidate realizes it has **lost** the conflict resolution:
1. It must stop announcing itself
2. It must inform nodes that previously heard its announcement
3. It transitions from `CLUSTERHEAD_CANDIDATE` → `EDGE` or `CLUSTER_MEMBER`

Without renouncement, losing candidates would keep announcing, causing confusion.

#### Where in the protocol flow is this logic used?

When a `CLUSTERHEAD_CANDIDATE` receives an announcement from a better clusterhead:
1. Recognize defeat (higher connection count or lower ID on tie)
2. Transition state to `EDGE`
3. In subsequent cycles, broadcast renouncement message
4. Stop forwarding own election announcements

Other nodes that heard the losing candidate update their records.

#### Critical Test Cases

| Test | What to Validate |
|------|-----------------|
| State transition | Candidate correctly transitions to EDGE after defeat |
| Renouncement broadcast | Renouncement message sent in subsequent cycle |
| Stop self-announcement | Losing candidate stops sending own election announcements |
| Renouncement propagation | Renouncement reaches nodes that heard original announcement |
| Final state stability | After renouncement, node doesn't flip-flop back to candidate |
| Timing | Renouncement happens promptly (within 1-2 cycles of defeat detection) |

---

## Phase 4 Protocol Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                     PHASE 4 PROTOCOL FLOW                        │
└─────────────────────────────────────────────────────────────────┘

1. CANDIDATE ANNOUNCES (Tasks 19, 21, 22)
   ┌──────────────────────────────────────┐
   │ Create Election Announcement:        │
   │   - Class ID                         │
   │   - Score (Task 21)                  │
   │   - Hash h(ID) (Task 22)             │
   │   - Initial PDSF = direct_connections│
   └──────────────────────────────────────┘
                    │
                    ▼
2. 3-ROUND FLOODING (Task 23)
   ┌──────────────────────────────────────┐
   │ Round 1 ──► Round 2 ──► Round 3      │
   │                                       │
   │ Each round:                           │
   │   - Stochastic broadcast timing      │
   │   - Nodes receive & evaluate         │
   │   - Forward if PDSF < 150 (Task 24)  │
   │   - Update PDSF on forward (Task 20) │
   └──────────────────────────────────────┘
                    │
                    ▼
3. CONFLICT RESOLUTION (Tasks 25, 26)
   ┌──────────────────────────────────────┐
   │ Node receives multiple announcements:│
   │                                       │
   │ Compare direct_connections:           │
   │   Higher wins ──► Track as clusterhead│
   │   Lower loses ──► Ignore announcement │
   │                                       │
   │ If equal connections:                 │
   │   Lower ID wins (tie-break)          │
   └──────────────────────────────────────┘
                    │
                    ▼
4. RENOUNCEMENT (Task 27)
   ┌──────────────────────────────────────┐
   │ Losing candidate:                     │
   │   - Detects better clusterhead        │
   │   - Transitions to EDGE state         │
   │   - Broadcasts renouncement           │
   │   - Stops own announcements           │
   │                                       │
   │ Winning candidate:                    │
   │   - Becomes CLUSTERHEAD               │
   │   - Continues announcements           │
   └──────────────────────────────────────┘
                    │
                    ▼
         ┌─────────────────┐
         │ PHASE 4 COMPLETE │
         │                  │
         │ Network has:     │
         │ - Stable CHs     │
         │ - Edge nodes     │
         │ - Ready for P5   │
         └─────────────────┘
```

---

## Cross-Task Dependencies

```
Task 19 (Message Structure)
    ↓
    Used by ALL other Phase 4 tasks
    ↓
Task 20 (PDSF) ←──────────────────────→ Task 24 (Capacity Limiting)
    │                                          │
    │  PDSF calculated here                    │  PDSF checked here
    │                                          │
    └──────────────┬───────────────────────────┘
                   ↓
            Task 23 (3-Round Flooding)
                   │
                   │  Announcements flood through network
                   ↓
            Task 25 (Conflict Resolution)
                   │
                   │  If connection counts equal
                   ↓
            Task 26 (Tie-Breaking)
                   │
                   │  Loser detected
                   ↓
            Task 27 (Renouncement)
```

---

## Existing Implementation (Partial)

Some Phase 4 functionality already has **partial implementation** from earlier tasks:

| Component | Location | Status |
|-----------|----------|--------|
| Election packet structure | `ble_discovery_packet.h` (Task 3) |  Exists |
| PDSF calculation | `ble_election_calculate_pdsf()` (Task 3) |  Exists |
| Score calculation | `ble_election_calculate_candidacy_score()` (Task 17) |  Exists |
| Hash generation | `ble_election_generate_hash()` (Task 3) |  Exists |

### What's Missing

1. **Election announcement transmission logic** (how/when to send)
2. **3-round flooding protocol** (timing, retransmission)
3. **Conflict resolution state machine** (comparing competing clusterheads)
4. **Renouncement mechanism** (state transition + broadcast)
5. **PDSF-based capacity enforcement** during forwarding

---

## Implementation Order Recommendation

1. **Task 19** - Message structure (foundation for all others)
2. **Task 20** - PDSF calculation (needed for capacity limiting)
3. **Task 21** - Score calculation (needed for conflict resolution)
4. **Task 22** - Hash generation (can be done in parallel)
5. **Task 23** - 3-round flooding (core mechanism)
6. **Task 24** - Capacity limiting (uses PDSF from Task 20)
7. **Task 25** - Conflict resolution (uses scores from Task 21)
8. **Task 26** - Tie-breaking (extension of Task 25)
9. **Task 27** - Renouncement (final convergence mechanism)

---

## Related Files

- **Protocol Core**: `model/protocol-core/ble_discovery_packet.{h,c}`
- **Election Logic**: `model/protocol-core/ble_election.{h,c}`
- **Node State Machine**: `model/protocol-core/ble_mesh_node.{h,c}`
- **Broadcast Timing**: `model/protocol-core/ble_broadcast_timing.{h,c}`
- **NS-3 Wrappers**: `model/ble-*-wrapper.{h,cc}`

---

## References

- Main TODO document: `/ns3-nordic/TODO.md`
- Protocol specification: "Clusterhead & BLE Mesh discovery process" by jason.peng (November 2025)
- Phase 3 implementation: Tasks 12-18 in TODO.md

---

*Last Updated: 2025-11-22*
