# BLE Discovery Engine - Implementation Analysis

## Summary

The `ble_discovery_engine.c/h` implements a **Phase 2** foundation that correctly orchestrates the helper modules (discovery cycle, message queue, forwarding logic, node state). However, it is **missing critical state machine logic and election mechanisms** required by the protocol specification.

---

## ✅ What IS Implemented (Phase 2 Foundation)

### 1. Engine Context Structure
- ✅ Aggregates all core components: node, cycle, queue, forwarding
- ✅ Stores configuration (node ID, TTL, proximity threshold, callbacks)
- ✅ Tracks crowding factor, noise level, last tick time

### 2. Public API & Configuration
- ✅ `ble_engine_init()` - Initializes all subsystems, wires callbacks
- ✅ `ble_engine_config_init()` - Sets sensible defaults
- ✅ Setters for runtime parameters (GPS, noise, crowding factor)
- ✅ `ble_engine_reset()` - Clears state for fresh start

### 3. Slot Scheduler Integration
- ✅ Embeds `ble_discovery_cycle_t` structure
- ✅ Registers slot handlers for all 4 slots (0=own, 1-3=forward)
- ✅ Wires cycle completion callback to advance node cycle counter

### 4. Engine Tick Entry Point
- ✅ `ble_engine_tick()` executes current slot and advances
- ✅ Auto-starts cycle on first tick if not running
- ✅ Properly dispatches to slot 0 (transmit own) or slots 1-3 (forward)

### 5. Platform Abstraction Layer
- ✅ Callback for packet transmission (`ble_engine_send_callback`)
- ✅ Callback for logging (`ble_engine_log_callback`)
- ✅ User context pointer passed to all callbacks
- ✅ No NS-3 dependencies in core engine

### 6. Message Handling (Partial)
- ✅ `ble_engine_receive_packet()` adds neighbors and enqueues messages
- ✅ Queue deduplication, loop detection, TTL handling
- ✅ Forwarding decision uses 3-metric algorithm (crowding, proximity, TTL)
- ✅ GPS handling (set/clear, attach to transmitted packets)
- ✅ Statistics tracking (sent/received/forwarded/dropped)

---

## ❌ What IS MISSING (Critical Protocol Features)

### 1. **Node State Machine Transitions** ⚠️ CRITICAL
The engine **NEVER transitions the node state**. It should invoke state changes based on gathered metrics:

**Missing Logic:**
```c
// After N discovery cycles, should check:
if (ble_mesh_node_should_become_edge(&engine->node)) {
    ble_mesh_node_set_state(&engine->node, BLE_NODE_STATE_EDGE);
}
else if (ble_mesh_node_should_become_candidate(&engine->node)) {
    ble_mesh_node_set_state(&engine->node, BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE);
    // Trigger election announcement generation
}
```

**Where it should happen:**
- In `ble_engine_cycle_complete()` callback - check metrics after each cycle
- Call existing helper functions: `ble_mesh_node_should_become_edge()`, `ble_mesh_node_should_become_candidate()`

**Impact:** Nodes remain in `BLE_NODE_STATE_INIT` or `BLE_NODE_STATE_DISCOVERY` forever. No clusterhead election occurs.

---

### 2. **Election Message Handling** ⚠️ CRITICAL
The engine treats all packets as discovery messages. It should:

**Missing in `ble_engine_receive_packet()`:**
- Check if `packet->message_type == BLE_DISCOVERY_MSG_TYPE_ELECTION`
- If election message:
  - Compare PDSF with local node
  - Compare score/direct connections for conflict resolution
  - Update node state (renounce candidacy if hearing better candidate)
  - Reset dynamic threshold counter when hearing another candidate

**Missing in `ble_engine_transmit_own_message()`:**
- No generation of election announcement messages when node is `CLUSTERHEAD_CANDIDATE`
- Should populate election fields: `class_id`, `pdsf`, `score`, `hash`
- Should use `ble_election_calculate_pdsf()`, `ble_election_calculate_score()`, `ble_election_generate_hash()`

**Impact:** No clusterhead election announcements are ever sent. Network never forms clusters.

---

### 3. **3-Round Election Announcement Flooding** ⚠️ CRITICAL
Protocol specifies clusterheads announce 3 times for redundancy.

**Missing:**
- No tracking of "which round" the node is on (1st, 2nd, or 3rd announcement)
- No logic to retransmit election announcements in subsequent cycles
- No mechanism to count announcement rounds

**Implementation needed:**
- Add `uint8_t election_round` to node state (0-2)
- When transitioning to `CLUSTERHEAD_CANDIDATE`, set `election_round = 0`
- On each cycle completion, increment round and retransmit if `round < 3`

**Impact:** Election announcements only sent once, reducing probability of reaching all nodes.

---

### 4. **Dynamic Candidacy Threshold (6→3→1)** ⚠️ HIGH PRIORITY
Protocol specifies threshold relaxes from 6² → 3² → 1² if no candidates heard.

**Current State:**
- `ble_mesh_node_mark_candidate_heard()` is called (✅)
- `ble_mesh_node.c` tracks `cycles_since_last_candidate_heard` (✅)
- Threshold calculation in `ble_mesh_node_should_become_candidate()` exists (✅)

**Missing in Engine:**
- Engine never calls `ble_mesh_node_should_become_candidate()` to check threshold
- No periodic re-evaluation of candidacy as threshold relaxes

**Fix:**
- In `ble_engine_cycle_complete()`, periodically call `ble_mesh_node_should_become_candidate()`
- Transition to candidate when threshold is met

---

### 5. **PDSF-Based Cluster Capacity Limiting** ⚠️ HIGH PRIORITY
Protocol: Stop forwarding election announcements when PDSF ≥ 150.

**Missing in `ble_engine_forward_next_message()`:**
```c
if (packet_to_process.message_type == BLE_DISCOVERY_MSG_TYPE_ELECTION) {
    if (packet_to_process.election.pdsf >= BLE_MESH_CLUSTER_CAPACITY) {
        // Do NOT forward - cluster is full
        ble_mesh_node_inc_dropped(&engine->node);
        return;
    }
}
```

**Impact:** Clusters can exceed 150 nodes, violating protocol constraints.

---

### 6. **Noisy Broadcast Phase** (Phase 3 - Not Yet Required)
Protocol specifies initial noisy broadcast phase for crowding factor measurement.

**Status:** Not implemented (expected for Phase 3)
- No "noisy broadcast" message generation
- No stochastic listening slot selection
- No RSSI sampling during dedicated broadcast phase

**Current Workaround:** Engine has `ble_engine_set_crowding_factor()` and `ble_engine_set_noise_level()` setters, expecting external system (NS-3 wrapper) to measure and provide these values.

**Note:** This is acceptable for Phase 2, but Phase 3 should integrate this into the engine.

---

### 7. **Direct Connection Counting Phase** (Phase 3 - Not Yet Required)
Protocol specifies stochastic broadcast phase to count direct neighbors.

**Status:** Not implemented (expected for Phase 3)
- No dedicated "clusterhead broadcast" phase
- No majority-listening, minority-broadcasting schedule

**Current Workaround:** Engine tracks neighbors via `ble_mesh_node_add_neighbor()` during normal discovery. This provides an estimate of direct connections but doesn't match the protocol's dedicated counting phase.

**Note:** Acceptable for Phase 2.

---

### 8. **Cluster Formation and Path Management** (Phase 5 - Future)
Not expected in Phase 2. These are documented in TODO.md as Phase 5 tasks:
- Edge node cluster alignment
- Multi-path memorization
- Dijkstra's algorithm integration
- Routing tree construction

**Status:** Correctly deferred to future phases.

---

## 🔧 Recommended Fixes (Priority Order)

### Priority 1: State Machine Integration (Required for Phase 2)
**File:** `ble_discovery_engine.c`
**Function:** `ble_engine_cycle_complete()`

Add state transition logic:
```c
static void
ble_engine_cycle_complete(uint32_t cycle_count, void *user_context)
{
    ble_engine_t *engine = (ble_engine_t *)user_context;
    if (!engine) {
        return;
    }

    ble_mesh_node_advance_cycle(&engine->node);

    // NEW: Check if node should transition states
    if (engine->node.state == BLE_NODE_STATE_DISCOVERY) {
        if (ble_mesh_node_should_become_edge(&engine->node)) {
            ble_mesh_node_set_state(&engine->node, BLE_NODE_STATE_EDGE);
            ble_engine_log(engine, "INFO", "Transitioned to EDGE state");
        }
        else if (ble_mesh_node_should_become_candidate(&engine->node)) {
            ble_mesh_node_set_state(&engine->node, BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE);
            ble_engine_log(engine, "INFO", "Transitioned to CLUSTERHEAD_CANDIDATE state");
            // Mark that election announcements should start
            engine->node.election_round = 0;  // NEW FIELD NEEDED
        }
    }
}
```

**Additional Changes Needed:**
- Add `uint8_t election_round` to `ble_mesh_node_t` structure
- Add `uint8_t election_rounds_remaining` to track 3-round announcement

---

### Priority 2: Election Message Generation
**File:** `ble_discovery_engine.c`
**Function:** `ble_engine_transmit_own_message()`

Modify to generate election messages when candidate:
```c
static void
ble_engine_transmit_own_message(ble_engine_t *engine)
{
    ble_discovery_packet_init(&engine->tx_buffer);
    engine->tx_buffer.sender_id = engine->node.node_id;
    engine->tx_buffer.ttl = engine->config.initial_ttl;
    ble_discovery_add_to_path(&engine->tx_buffer, engine->node.node_id);

    if (engine->node.gps_available) {
        ble_discovery_set_gps(&engine->tx_buffer,
                              engine->node.gps_location.x,
                              engine->node.gps_location.y,
                              engine->node.gps_location.z);
    }

    // NEW: If clusterhead candidate, send election announcement
    if (engine->node.state == BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE ||
        engine->node.state == BLE_NODE_STATE_CLUSTERHEAD) {

        engine->tx_buffer.message_type = BLE_DISCOVERY_MSG_TYPE_ELECTION;

        // Populate election fields
        engine->tx_buffer.election.class_id = engine->node.node_id; // Simple assignment
        engine->tx_buffer.election.pdsf = ble_election_calculate_pdsf(0,
                                            ble_mesh_node_count_direct_neighbors(&engine->node));
        engine->tx_buffer.election.score = ble_mesh_node_calculate_candidacy_score(&engine->node);
        engine->tx_buffer.election.hash = ble_election_generate_hash(engine->node.node_id);
    }

    ble_mesh_node_inc_sent(&engine->node);
    if (engine->config.send_cb) {
        engine->config.send_cb(&engine->tx_buffer, engine->config.user_context);
    }
}
```

---

### Priority 3: Election Message Reception & Conflict Resolution
**File:** `ble_discovery_engine.c`
**Function:** `ble_engine_receive_packet()`

Add election message handling:
```c
bool
ble_engine_receive_packet(ble_engine_t *engine,
                          const ble_discovery_packet_t *packet,
                          int8_t rssi,
                          uint32_t now_ms)
{
    if (!engine || !packet) {
        return false;
    }

    ble_mesh_node_add_neighbor(&engine->node,
                               packet->sender_id,
                               rssi,
                               1);

    // NEW: Handle election messages
    if (packet->message_type == BLE_DISCOVERY_MSG_TYPE_ELECTION) {
        ble_engine_mark_candidate_heard(engine);  // Reset threshold

        // If we're also a candidate, compare scores
        if (engine->node.state == BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE) {
            double our_score = ble_mesh_node_calculate_candidacy_score(&engine->node);
            uint32_t our_neighbors = ble_mesh_node_count_direct_neighbors(&engine->node);

            // Conflict resolution: higher direct connections wins
            // Tie break: lower node ID wins
            if (packet->election.pdsf > our_neighbors ||
                (packet->election.pdsf == our_neighbors &&
                 packet->sender_id < engine->node.node_id)) {
                // Renounce candidacy
                ble_mesh_node_set_state(&engine->node, BLE_NODE_STATE_EDGE);
                ble_engine_log(engine, "INFO", "Renounced candidacy - heard better candidate");
            }
        }
    }

    bool enqueued = ble_queue_enqueue(&engine->forward_queue,
                                      packet,
                                      engine->node.node_id,
                                      now_ms);
    if (!enqueued) {
        ble_engine_log(engine, "DEBUG", "Queue rejected incoming packet");
    } else {
        ble_mesh_node_inc_received(&engine->node);
    }

    return enqueued;
}
```

---

### Priority 4: PDSF Capacity Limiting
**File:** `ble_discovery_engine.c`
**Function:** `ble_engine_forward_next_message()`

Add cluster capacity check:
```c
static void
ble_engine_forward_next_message(ble_engine_t *engine)
{
    ble_discovery_packet_t peek_packet;
    if (!ble_queue_peek(&engine->forward_queue, &peek_packet)) {
        return;
    }

    // NEW: Check cluster capacity for election messages
    if (peek_packet.message_type == BLE_DISCOVERY_MSG_TYPE_ELECTION) {
        if (peek_packet.election.pdsf >= BLE_MESH_CLUSTER_CAPACITY) {
            // Cluster full - drop message
            ble_discovery_packet_t dropped;
            ble_queue_dequeue(&engine->forward_queue, &dropped);
            ble_mesh_node_inc_dropped(&engine->node);
            ble_engine_log(engine, "DEBUG", "Dropped election message - cluster full");
            return;
        }
    }

    // ... rest of existing forwarding logic ...
}
```

**Additional:** Need to update PDSF as message is forwarded:
```c
// After adding self to path, update PDSF
if (packet_to_process.message_type == BLE_DISCOVERY_MSG_TYPE_ELECTION) {
    uint32_t direct = ble_mesh_node_count_direct_neighbors(&engine->node);
    packet_to_process.election.pdsf = ble_election_calculate_pdsf(
        packet_to_process.election.pdsf,
        direct
    );
}
```

---

### Priority 5: 3-Round Election Announcement
**Changes Needed:**
1. Add to `ble_mesh_node_t`:
   ```c
   uint8_t election_round;  // 0, 1, 2 for three rounds
   ```

2. In `ble_engine_cycle_complete()`:
   ```c
   if (engine->node.state == BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE) {
       if (engine->node.election_round < 3) {
           engine->node.election_round++;
           // Election announcement will be sent on next slot 0
       } else {
           // After 3 rounds, transition to CLUSTERHEAD
           ble_mesh_node_set_state(&engine->node, BLE_NODE_STATE_CLUSTERHEAD);
       }
   }
   ```

---

## 📊 Compliance Summary

| Protocol Feature | C-engine TODO Phase | Implemented? | Notes |
|------------------|---------------------|--------------|-------|
| **Phase 2 Foundation** |
| Engine context struct | Phase 2.1 | ✅ Complete | All subsystems integrated |
| Public API & config | Phase 2.2 | ✅ Complete | Init, reset, setters working |
| Slot scheduler integration | Phase 2.3 | ✅ Complete | 4-slot cycle with callbacks |
| Engine tick entry point | Phase 2.4 | ✅ Complete | Executes and advances slots |
| Platform abstraction | Phase 2.5 | ✅ Complete | Callbacks for TX, logging |
| **Phase 3-4 Extensions** |
| Message handling glue | Phase 3.6 | ⚠️ Partial | Queue/forwarding work, missing election handling |
| Node state machine hooks | Phase 3.7 | ❌ Missing | No state transitions based on metrics |
| Election flooding | Phase 3.8 | ❌ Missing | No election message generation or 3-round logic |
| Dynamic threshold | Phase 3.7 | ⚠️ Partial | Tracking exists, not used in engine |
| PDSF capacity limiting | Phase 3.8 | ❌ Missing | No cluster size enforcement |
| **Phase 3 Protocol Details** |
| Noisy broadcast phase | Phase 3 | ❌ Deferred | External measurement for now |
| Direct connection count | Phase 3 | ⚠️ Partial | Passive tracking, no dedicated phase |
| **Phase 5 Features** |
| Cluster formation | Phase 5 | ⏸️ Future | Correctly deferred |
| Multi-path routing | Phase 5 | ⏸️ Future | Correctly deferred |

---

## ✅ Conclusion

The engine provides a **solid Phase 2 foundation** that correctly orchestrates all helper modules. However, it is **incomplete for Phase 3-4** because it lacks:

1. **State machine integration** - nodes never transition from DISCOVERY to EDGE/CANDIDATE
2. **Election message handling** - no generation, reception, or conflict resolution
3. **3-round announcement flooding** - no multi-round election logic
4. **PDSF capacity limiting** - clusters can exceed 150 nodes

The missing features prevent the protocol from performing clusterhead election and cluster formation. The engine can run discovery cycles and forward messages, but **never forms clusters** because the state machine logic is not connected.

**Recommendation:** Implement Priority 1-4 fixes to complete Phase 3-4 requirements. The architecture is sound; the logic just needs to be wired together.