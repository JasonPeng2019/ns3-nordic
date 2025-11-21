# NS-3 BLE Mesh Discovery Protocol Implementation - Task List

This document outlines all tasks required to implement the BLE Mesh Discovery Protocol with Clusterhead Election in NS-3, based on the protocol specification by jason.peng (November 2025).

## Protocol Overview

The protocol enables BLE mesh networks to scale from traditional 100-150 node capacity to thousands or tens of thousands of nodes using:
- Discovery messages with TTL, Path So Far (PSF), and GPS location
- 3-metric message forwarding (Picky Forwarding, GPS Proximity, TTL)
- RSSI-based clusterhead election with 3-round announcement
- Cluster capacity limiting (150 devices per cluster)
- Multi-path routing with Dijkstra's shortest path

Target performance: End-to-end latency in seconds, supporting thousands of nodes.

---

## Phase 1: Foundation & Setup

### Task 1: Research NS-3 BLE/Bluetooth Capabilities ✅ COMPLETED
- [x] Review NS-3 documentation for existing BLE/Bluetooth modules
- [x] Evaluate if existing modules support required features
- [x] Determine if custom PHY/MAC layer implementation is needed
- [x] Document gaps and requirements for custom implementation
- [x] Review NS-3 examples for similar wireless protocols (WiFi mesh, LoRaWAN)


**Research Summary (Completed 2025-11-19):**

#### Current State of BLE Support in NS-3:
- **NO official BLE module** exists in main NS-3 distribution
- NS-3 includes LR-WPAN (IEEE 802.15.4) for ZigBee, but this is NOT BLE
- **NO BLE mesh implementations** exist for NS-3

#### Available Third-Party BLE Modules:
1. **ns3-ble-module by Stijn** (GitLab: https://gitlab.com/Stijng/ns3-ble-module) ⭐ **SELECTED**
   - KU Leuven master's thesis (2018)
   - More recent than alternatives (2018 vs 2015)
   - Includes: BlePhy, BleMac, Spectrum Channel integration
   - Basic data transmission support
   - **Chosen as starting point for this project**

2. **BLEATS by zzma** (GitHub: https://github.com/zzma/bleats)
   - More complete implementation (includes BleNetDevice, BleErrorModel)
   - NS-3 version 3.24 (2015) - older
   - Has example programs
   - May be used as reference for additional features

**Compatibility Issues:**
- All existing modules use wscript (pre-CMake)
- Must convert to CMakeLists.txt for NS-3 3.36+
- Requires API updates and testing

#### What Must Be Built From Scratch:

**Layer 1 - Physical (PHY):** ⚠️ Partially Available
- Use existing BlePhy from Stijn's module as foundation
- Enhance with BLE 5.0+ features
- Already integrated with NS-3 Spectrum framework
- Reference BLEATS for additional PHY features if needed

**Layer 2 - MAC:** ⚠️ Partially Available
- Use existing BleMac from Stijn's module
- Extend for mesh advertising mechanisms
- Add channel hopping and collision avoidance
- Reference BLEATS BleNetDevice for device integration patterns

**Layer 3 - BLE Mesh Network Layer:** ❌ MUST BUILD COMPLETELY
- Advertising Bearer (PB-ADV)
- Network layer with managed flooding
- TTL control and message relay
- Message cache for duplicate detection
- Segmentation/reassembly
- Foundation models

**Layer 4 - Custom Discovery Protocol:** ❌ YOUR INNOVATION
- Discovery protocol from PDF specification
- Clusterhead election algorithm
- Routing beyond standard flooding
- Scalability features for 1000+ nodes

#### NS-3 Modules to Leverage:

**Primary References:**
1. **WiFi Mesh (802.11s) Module** (`/src/mesh/`)
   - HWMP routing protocol
   - Peer management and mesh topology
   - **Best reference for mesh implementation**

2. **LR-WPAN Module** (`/src/lrwpan/`)
   - Low-power PHY/MAC patterns
   - CSMA/CA mechanism

3. **Routing Protocols** (`/src/aodv/`, `/src/olsr/`)
   - Route discovery and maintenance
   - Routing table management

4. **LoRaWAN Module** (GitHub: https://github.com/signetlabdei/lorawan)
   - IoT protocol architecture
   - Helper class design patterns

5. **Spectrum, Energy, Mobility, Propagation Modules**
   - Supporting infrastructure

#### Scalability for 1000+ Nodes:

**Challenges:**
- Standard BLE mesh uses flooding → broadcast storm at 1000 nodes
- Practical limit: ~100 devices before severe congestion
- NS-3 can handle 1000 nodes but needs optimization

**Required Solutions:**
1. **Hierarchical clustering** (10-50 nodes per cluster)
2. **Selective relay nodes** (only 5% forward messages)
3. **Optimized TTL management**
4. **Message caching and deduplication**
5. **Zone-based organization**

**NS-3 Optimization:**
- Use distributed simulation with MPI for 1000+ nodes
- Range-based packet reception (not global broadcast)
- Nix-vector routing to avoid Dijkstra on full graph
- Memory-efficient data structures

#### Key Research Papers:
- "Enhancing WSN with BLE Mesh and ACO Algorithm" (MDPI) - **1000 node implementation**
- "Bluetooth Mesh Analysis, Issues, and Challenges" (IEEE)
- "Modified LEACH for WSN" - Clustering algorithms

#### Recommended Implementation Approach:

**Phase 1:** Start with Stijn's ns3-ble-module
- Clone from GitLab: https://gitlab.com/Stijng/ns3-ble-module
- Port to NS-3 3.43+ with CMake
- Validate basic BLE functionality
- Fix API compatibility issues
- Reference BLEATS for additional features if needed

**Phase 2:** Build BLE Mesh Network Layer
- Implement Bearer, Network, Transport layers
- Use WiFi mesh module structure as reference
- Start with basic flooding

**Phase 3:** Implement Custom Discovery Protocol
- Follow PDF specification
- Add clusterhead election (LEACH-based)
- Implement routing algorithms

**Phase 4:** Optimize for Scale
- Test incrementally: 10 → 100 → 500 → 1000 nodes
- Add hierarchical clustering
- Implement selective relay
- Use distributed simulation if needed

**Estimated Timeline:** 4-6 months for complete implementation

**Feasibility:** ✅ FEASIBLE but CHALLENGING
- Foundation exists (BLE PHY/MAC)
- Excellent reference implementations available
- Significant development effort required
- Complete network stack must be custom-built

### Task 2: Set Up NS-3 Development Environment ✅ COMPLETED
- [x] Install NS-3 (latest stable version or development branch)
- [x] Create new module: `ns3/src/ble-mesh-discovery/`
- [x] Set up module directory structure (model/, helper/, examples/, test/)
- [x] Configure wscript build file for new module
- [x] Verify build system works with empty module

**Setup Summary (Completed 2025-11-20):**

#### NS-3 Installation:
- Cloned NS-3 development repository
- Checked out commit `25e0d01d28` (June 2021) - compatible with BLE module
- Uses **waf** build system (not CMake)
- Configured with `--enable-tests --enable-examples`

#### BLE Module Integration:
- Linked Stijn's ns3-ble-module to `ns-3-dev/src/ble/`
- Fixed compiler warnings for Apple Clang 12:
  - Added `__attribute__((unused))` to unused private fields
  - Fixed assignment-in-condition warning
- Module now compiles successfully with `-Werror`

#### BLE Mesh Discovery Module Created:
- Location: `ns-3-dev/src/ble-mesh-discovery/`
- Directory structure:
  ```
  ble-mesh-discovery/
  ├── wscript              # Build configuration
  ├── README.md            # Project documentation
  ├── model/               # Core protocol implementation (empty, ready for Task 3+)
  ├── helper/              # Helper classes (empty)
  ├── examples/            # Example programs (wscript ready)
  │   └── wscript
  ├── test/                # Unit tests (empty)
  └── doc/                 # Documentation (empty)
  ```

#### Git Configuration:
- Created branch: `ble-mesh-dev`
- Changed remote to: https://github.com/buh07/ns-3.git
- Committed module scaffold
- BLE module fixes committed to ns3-ble-module repo

#### Build System Status:
- NS-3 configures successfully
- Empty module ready for implementation
- Dependencies declared: core, network, spectrum, mobility, energy, propagation, ble

**Ready for Task 3**: Design core BLE discovery message packet structure

### Task 3: Design Core BLE Discovery Message Packet Structure ✅ COMPLETED
- [x] Define discovery message fields (ID, TTL, PSF, LHGPS)
- [x] Design packet format and field sizes
- [x] Plan for extensibility (election announcement fields)
- [x] Document packet structure specification

**Implementation Summary (Completed 2025-11-20):**

#### Architecture: Pure C Core + C++ Wrapper

**Design Decision:** Separated protocol logic (C) from NS-3 integration (C++)
- **Pure C Protocol Core** - Portable to embedded systems, no dependencies
- **C++ Wrapper** - Thin integration layer for NS-3

#### Files Created:

**Pure C Core (Portable):**
- `model/protocol-core/ble_discovery_packet.h` (300+ lines)
  - C structs: `ble_discovery_packet_t`, `ble_election_packet_t`
  - Function API for all operations
  - No C++ or NS-3 dependencies
- `model/protocol-core/ble_discovery_packet.c` (360+ lines)
  - Complete implementation
  - Serialization/deserialization (network byte order)
  - Election calculations (PDSF, score, hash)

**C++ NS-3 Wrapper:**
- `model/ble-discovery-header-wrapper.h` (246 lines)
  - Inherits from `ns3::Header`
  - Wraps C structures internally
- `model/ble-discovery-header-wrapper.cc` (317 lines)
  - Delegates all logic to C core
  - Converts NS-3 types ↔ C types

**Tests & Examples:**
- `test/ble-discovery-header-test.cc` (updated to use wrapper)
- `examples/ble-discovery-header-example.cc`

**Documentation:**
- `C-CORE-ARCHITECTURE.md` - Complete architecture guide

#### Features Implemented:

**Discovery Fields:**
- ✅ Message Type (DISCOVERY / ELECTION)
- ✅ Sender ID (32-bit)
- ✅ TTL with decrement
- ✅ Path So Far (variable length, max 50 nodes)
- ✅ GPS Location (optional, saves 24 bytes when unavailable)

**Election Fields:**
- ✅ Class ID (clusterhead identifier)
- ✅ PDSF (Predicted Devices So Far) with calculation
- ✅ Score calculation (connection:noise ratio + distribution)
- ✅ Hash generation (FNV-1a for FDMA/TDMA)

**Key Functions (C Core):**
```c
// Initialization
void ble_discovery_packet_init(ble_discovery_packet_t *packet);

// Operations
bool ble_discovery_decrement_ttl(ble_discovery_packet_t *packet);
bool ble_discovery_add_to_path(ble_discovery_packet_t *packet, uint32_t node_id);
bool ble_discovery_is_in_path(const ble_discovery_packet_t *packet, uint32_t node_id);

// Serialization
uint32_t ble_discovery_serialize(const ble_discovery_packet_t *packet,
                                   uint8_t *buffer, uint32_t buffer_size);
uint32_t ble_discovery_deserialize(ble_discovery_packet_t *packet,
                                     const uint8_t *buffer, uint32_t buffer_size);

// Election calculations
uint32_t ble_election_calculate_pdsf(const uint32_t *direct_counts, uint16_t hop_count);
double ble_election_calculate_score(uint32_t direct_connections,
                                      double noise_level,
                                      double geographic_distribution);
uint32_t ble_election_generate_hash(uint32_t node_id);
```

#### Architecture Benefits:

1. **Portability** - C core can compile for ARM Cortex-M, ESP32, nRF52, etc.
2. **Testability** - Test C core without NS-3 dependency
3. **Maintainability** - Protocol logic separate from simulation
4. **Reusability** - Same code for simulation and real hardware
5. **Performance** - C is faster and produces smaller binaries

#### Packet Sizes:

**Discovery Message:**
- Minimum: 9 bytes (no path, no GPS)
- Typical: 45 bytes (3-hop path + GPS)
- Maximum: ~400 bytes (50-hop path + GPS)

**Election Announcement:**
- Minimum: 27 bytes (no path, no GPS)
- Typical: 63 bytes (3-hop path + GPS)
- Additional: 18 bytes for election fields

#### Usage Examples:

**NS-3 (C++ Wrapper):**
```cpp
BleDiscoveryHeaderWrapper header;
header.SetSenderId(42);
header.AddToPath(42);
Ptr<Packet> packet = Create<Packet>();
packet->AddHeader(header);
```

**Embedded (Pure C):**
```c
ble_discovery_packet_t packet;
ble_discovery_packet_init(&packet);
packet.sender_id = 42;
uint8_t buffer[256];
ble_discovery_serialize(&packet, buffer, sizeof(buffer));
```

#### Cleanup Performed:

- ❌ Removed `model/ble-discovery-header.{h,cc}` (pure C++ version)
- ✅ Single source of truth: C core only
- ✅ No code duplication
- ✅ Clean architecture maintained

#### Build & Test Status:

**Compilation:** ✅ **SUCCESSFUL**
```bash
./waf build
# Output: 'build' finished successfully (1m51s)
# Module: ble-mesh-discovery (no Python)
```

**Unit Tests:** ✅ **ALL PASSING**
```bash
./test.py -s ble-discovery-header
# Result: 1 of 1 tests passed
```

**Example Execution:** ✅ **WORKING**
```bash
./build/src/ble-mesh-discovery/examples/ns3-dev-ble-discovery-header-example-debug
# Successfully demonstrates:
# - Discovery message: 45 bytes serialized
# - Election announcement: 55 bytes serialized
# - TTL operations and loop detection working correctly
```

**Issues Fixed During Build:**
1. Include path: Changed to `ns3/ble_discovery_packet.h` for installed headers
2. Test file: Updated `GetPathSoFar()` to `GetPath()`
3. Example file: Updated to use `BleDiscoveryHeaderWrapper` and `SetAsElectionMessage()`
4. `SetAsElectionMessage()`: Fixed to preserve packet state while setting message type correctly

**Task 3 Status: ✅ COMPLETE AND VERIFIED**

#### Build Status:
- wscript updated with C and C++ files
- Ready to compile: `./waf build`
- Total code: 1,316 lines across 6 files

**Ready for Task 4**: Implement BleDiscoveryHeader serialization unit tests

### Task 4: Implement Discovery Message Header Class
- [ ] Create `BleDiscoveryHeader` class inheriting from `ns3::Header`
- [ ] Implement field getters/setters (ID, TTL, PSF, LHGPS)
- [ ] Implement `Serialize()` and `Deserialize()` methods
- [ ] Implement `GetSerializedSize()` and `GetTypeId()`
- [ ] Add support for GPS unavailable flag handling
- [ ] Write unit tests for serialization/deserialization

### Task 5: Create BLE Node Model with State Machine
- [ ] Design node states: DISCOVERY, EDGE, CLUSTERHEAD_CANDIDATE, CLUSTERHEAD
- [ ] Create `BleMeshNode` class with state management
- [ ] Implement state transition logic
- [ ] Add neighbor tracking data structures
- [ ] Implement node ID generation/assignment
- [ ] Add debugging/logging for state transitions

### Task 6: Implement GPS Location Caching Mechanism
- [ ] Add GPS coordinate storage to node model
- [ ] Implement GPS update mechanism (integration hook for mobility models)
- [ ] Add "GPS unavailable" state handling
- [ ] Implement LHGPS field population in discovery messages
- [ ] Add configuration option for GPS-enabled vs GPS-denied environments
- [ ] Test with static and mobile node scenarios

---

## Phase 2: Core Discovery Protocol

### Task 7: Design and Implement 4-Slot Discovery Cycle
- [ ] Define discovery cycle timing structure (4 message slots)
- [ ] Implement slot timing mechanism (1 slot for own message, 3 for forwarding)
- [ ] Create discovery cycle scheduler
- [ ] Implement slot allocation algorithm
- [ ] Add synchronization mechanism for network-wide cycles
- [ ] Test cycle timing accuracy and consistency

### Task 8: Implement Message Forwarding Queue
- [ ] Create message queue data structure for received discovery messages
- [ ] Implement queue management (add, remove, prioritize)
- [ ] Add message deduplication logic (track seen messages)
- [ ] Implement PSF loop detection (message not forwarded if node already in PSF)
- [ ] Add queue size limits and overflow handling
- [ ] Test queue behavior under high message load

### Task 9: Implement Picky Forwarding Algorithm
- [ ] Define crowding factor calculation function
- [ ] Implement percentage-based message filtering using crowding factor
- [ ] Add configurable crowding threshold parameters
- [ ] Test forwarding probability vs crowding factor relationship
- [ ] Validate congestion prevention effectiveness
- [ ] Add logging for forwarding decisions

### Task 10: Implement GPS Proximity Filtering
- [ ] Implement distance calculation between GPS coordinates
- [ ] Define proximity threshold (configure based on network density)
- [ ] Add LHGPS comparison logic in forwarding decision
- [ ] Skip proximity check when GPS unavailable
- [ ] Test with different proximity threshold values
- [ ] Validate spatial message distribution

### Task 11: Implement TTL-Based Message Prioritization
- [ ] Implement TTL decrement on message forwarding
- [ ] Create priority queue sorted by TTL
- [ ] Implement top-3 selection algorithm after other filters applied
- [ ] Add TTL expiration handling (TTL=0 messages not forwarded)
- [ ] Test message propagation distance vs initial TTL
- [ ] Validate network coverage with different TTL values

---

## Phase 3: Clusterhead Election - Discovery Phase

### Task 12: Implement Noisy Broadcast Phase
- [ ] Implement "noisy broadcast" message transmission
- [ ] Create base-level noise message format
- [ ] Implement stochastic randomized listening time slot selection
- [ ] Add RSSI measurement during listening phase
- [ ] Store RSSI samples for crowding factor calculation
- [ ] Test broadcast/listen timing coordination

### Task 13: Implement RSSI-Based Crowding Factor Calculation
- [ ] Define crowding factor function f(RSSI)
- [ ] Implement RSSI aggregation from multiple samples
- [ ] Add noise level calculation from RSSI measurements
- [ ] Account for collision signal contribution to RSSI
- [ ] Add configurable parameters for f(RSSI) function
- [ ] Validate crowding factor accuracy in different densities

### Task 14: Implement Stochastic Broadcast Timing
- [ ] Implement random time slot selection for clusterhead broadcasts
- [ ] Create majority-listening, minority-broadcasting schedule
- [ ] Add collision avoidance through randomization
- [ ] Implement broadcast attempt retry logic
- [ ] Test broadcast success rate vs network density
- [ ] Validate timing distribution randomness

### Task 15: Implement Direct Connection Counting
- [ ] Add direct connection counter to node state
- [ ] Implement neighbor detection during broadcast phase
- [ ] Track successfully parsed messages (strong signal)
- [ ] Store list of direct neighbors (1-hop neighbors)
- [ ] Handle collision impact on connection count
- [ ] Test connection count accuracy vs actual topology

### Task 16: Implement Connectivity Metric Tracking
- [ ] Track number of direct neighbors (1-hop)
- [ ] Track unique paths from forwarded messages (PSF analysis)
- [ ] Implement geographic distribution calculation using LHGPS
- [ ] Create connectivity score aggregation
- [ ] Add metrics update during discovery phase
- [ ] Log connectivity metrics for analysis

### Task 17: Implement Clusterhead Candidacy Determination
- [ ] Calculate direct connection count : noise heard ratio
- [ ] Implement candidacy threshold calculation (dynamic based on crowding)
- [ ] Add geographic distribution check (neighbors not all clustered)
- [ ] Implement successful forwarding requirement check
- [ ] Create candidacy decision logic combining all criteria
- [ ] Test candidacy election in various network topologies

### Task 18: Implement Geographic Distribution Analysis
- [ ] Calculate spatial distribution of neighbors using LHGPS
- [ ] Implement clustering detection algorithm (e.g., variance-based)
- [ ] Define "well-distributed" threshold
- [ ] Handle GPS-unavailable scenarios (skip this check)
- [ ] Test with clustered vs distributed topologies
- [ ] Validate clusterhead placement quality

---

## Phase 4: Clusterhead Election - Announcement & Conflict Resolution

### Task 19: Design Election Announcement Message Structure
- [ ] Extend discovery message format with election fields:
  - Class ID: Clusterhead class identifier
  - ID: Message sender ID
  - PDSF: Predicted Devices So Far
  - PSF: Path So Far
  - Score: Clusterhead candidacy score
  - Hash h(ID): FDMA/TDMA hash function
- [ ] Create `BleElectionHeader` class
- [ ] Implement serialization/deserialization
- [ ] Add clusterhead flag to identify election messages

### Task 20: Implement PDSF Calculation Function
- [ ] Implement t(x) = Σᵢ Πᵢ(xᵢ) formula
- [ ] Track direct connection count at each hop (xᵢ)
- [ ] Exclude previously reached devices from calculation
- [ ] Update PDSF as message propagates through network
- [ ] Test PDSF accuracy vs actual devices reached
- [ ] Validate mathematical correctness of calculation

### Task 21: Implement Clusterhead Score Calculation
- [ ] Define score formula based on candidacy metrics:
  - Direct connection count
  - Connection:noise ratio
  - Geographic distribution quality
  - Successful forwarding rate
- [ ] Implement score calculation function
- [ ] Add configurable weights for different metrics
- [ ] Test score correlation with node quality
- [ ] Validate score-based ranking

### Task 22: Implement FDMA/TDMA Hash Function Generation
- [ ] Design hash function h(ID) for cluster communication
- [ ] Ensure hash uniqueness for each clusterhead
- [ ] Implement time/frequency slot assignment from hash
- [ ] Add hash to election announcement messages
- [ ] Plan for future FDMA/TDMA implementation
- [ ] Document hash function specification

### Task 23: Implement 3-Round Election Announcement Flooding
- [ ] Implement election announcement transmission
- [ ] Create 3-round announcement schedule
- [ ] Implement message retransmission/flooding logic
- [ ] Add announcement receipt tracking
- [ ] Ensure stochastic probability of reaching all devices
- [ ] Test announcement coverage across 3 rounds

### Task 24: Implement PDSF-Based Cluster Capacity Limiting
- [ ] Check PDSF against cluster capacity threshold (150 devices)
- [ ] Stop retransmitting announcements when capacity reached
- [ ] Prevent new devices from joining full clusters
- [ ] Implement capacity check before forwarding election announcements
- [ ] Test cluster size distribution
- [ ] Validate 150-device cluster limit enforcement

### Task 25: Implement Conflict Resolution Between Clusterheads
- [ ] Compare direct connection counts of competing clusterheads
- [ ] Implement higher-connection-count wins logic
- [ ] Handle overlapping cluster coverage
- [ ] Update node state when conflict detected
- [ ] Test with multiple simultaneous clusterhead candidates
- [ ] Validate conflict resolution convergence

### Task 26: Implement Tie-Breaking Using Device ID
- [ ] Detect equal connection count ties
- [ ] Compare device IDs (lower ID takes precedence)
- [ ] Implement deterministic tie-breaking
- [ ] Test tie-breaking with multiple nodes
- [ ] Validate consistent tie resolution

### Task 27: Implement Candidacy Renouncement Mechanism
- [ ] Detect when higher-ranked clusterhead announcement received
- [ ] Implement candidacy withdrawal logic
- [ ] Broadcast renouncement in subsequent cycles
- [ ] Update node state from CLUSTERHEAD_CANDIDATE to EDGE
- [ ] Test renouncement propagation
- [ ] Validate final clusterhead stability

---

## Phase 5: Cluster Formation & Path Management

### Task 28: Implement Edge Node Cluster Alignment
- [ ] Track received election announcements from all clusterheads
- [ ] Compare path lengths to each clusterhead
- [ ] Compare direct count scores when path lengths equal
- [ ] Select primary clusterhead based on criteria
- [ ] Store clusterhead ID and path information
- [ ] Test alignment with multiple nearby clusterheads

### Task 29: Implement Multi-Path Memorization
- [ ] Store all discovered paths to each clusterhead
- [ ] Maintain path list with PSF sequences
- [ ] Track path quality metrics (length, score)
- [ ] Implement path redundancy for fault tolerance
- [ ] Test with complex multi-path topologies
- [ ] Validate path diversity

### Task 30: Implement Dijkstra's Algorithm for Shortest Path
- [ ] Implement Dijkstra's shortest path algorithm
- [ ] Use hop count as edge weight
- [ ] Compute shortest path to assigned clusterhead
- [ ] Add computed path to feasible paths list
- [ ] Handle multiple interconnected paths
- [ ] Test shortest path accuracy vs brute force

### Task 31: Implement Routing Tree Construction
- [ ] Build routing tree from edge node to clusterhead
- [ ] Use memorized paths and shortest path
- [ ] Define primary and backup paths
- [ ] Implement next-hop determination for routing
- [ ] Test tree correctness and loop-freedom
- [ ] Validate routing efficiency

---

## Phase 6: Network Simulation Infrastructure

### Task 32: Create Network Topology Generator
- [ ] Implement random node placement (uniform distribution)
- [ ] Implement grid-based node placement
- [ ] Implement clustered node placement (simulate buildings/areas)
- [ ] Support configurable network sizes (100-10,000 nodes)
- [ ] Add configurable node density parameters
- [ ] Generate topology configuration files

### Task 33: Implement Mobility Model Integration
- [ ] Integrate NS-3 mobility models (ConstantPosition, RandomWalk, etc.)
- [ ] Implement GPS coordinate update callbacks
- [ ] Handle position changes during simulation
- [ ] Test with static and mobile scenarios
- [ ] Validate LHGPS updates during movement
- [ ] Analyze protocol behavior with mobility

### Task 34: Configure BLE PHY Layer Parameters
- [ ] Set BLE transmission power (realistic values)
- [ ] Configure transmission range (10-100m typical for BLE)
- [ ] Set data rate (1-2 Mbps for BLE)
- [ ] Configure propagation loss model (log-distance or realistic)
- [ ] Set receiver sensitivity and noise floor
- [ ] Validate PHY parameters match BLE specification

### Task 35: Implement Single-Channel Communication Model
- [ ] Configure all nodes to use single discovery channel
- [ ] Implement channel access mechanism
- [ ] Add channel busy/idle detection
- [ ] Test channel utilization vs network size
- [ ] Validate single-channel behavior
- [ ] Monitor channel congestion

### Task 36: Implement Collision Detection and Modeling
- [ ] Implement packet collision detection on shared channel
- [ ] Model RSSI impact from collisions
- [ ] Handle partial packet reception (corrupted messages)
- [ ] Impact collision on connection count and noise metrics
- [ ] Test collision rate vs network density
- [ ] Validate collision impact on protocol performance

### Task 37: Create Logging and Tracing Infrastructure
- [ ] Implement detailed logging for discovery events
- [ ] Log clusterhead election process
- [ ] Log cluster formation and assignments
- [ ] Add packet-level tracing (sent/received/forwarded)
- [ ] Create configurable log levels (debug, info, warning)
- [ ] Output logs to files for post-processing

---

## Phase 7: Testing & Validation

### Task 38: Implement Metrics Collection
- [ ] Track end-to-end latency (source to destination)
- [ ] Measure convergence time (until all nodes assigned to clusters)
- [ ] Count number of clusters formed
- [ ] Measure cluster size distribution
- [ ] Track message overhead (total messages sent)
- [ ] Calculate packet delivery ratio

### Task 39: Create Visualization Output
- [ ] Export cluster topology (nodes, clusterheads, edges)
- [ ] Generate visualization files (GraphML, DOT, JSON)
- [ ] Color-code nodes by cluster assignment
- [ ] Show clusterhead-to-clusterhead links
- [ ] Visualize routing trees
- [ ] Create animation of discovery/election process

### Task 40: Develop Unit Tests for Discovery Message Handling
- [ ] Test message serialization/deserialization
- [ ] Test TTL decrement and expiration
- [ ] Test PSF loop detection
- [ ] Test message deduplication
- [ ] Test forwarding queue prioritization
- [ ] Validate all three forwarding metrics

### Task 41: Develop Unit Tests for Clusterhead Election
- [ ] Test connectivity metric calculation
- [ ] Test candidacy determination logic
- [ ] Test PDSF calculation accuracy
- [ ] Test conflict resolution scenarios
- [ ] Test tie-breaking
- [ ] Test renouncement mechanism

### Task 42: Create Small-Scale Validation Scenario (10-20 Nodes)
- [ ] Set up simple topology with 10-20 nodes
- [ ] Run discovery and election process
- [ ] Verify single clusterhead emerges
- [ ] Validate all nodes assigned to cluster
- [ ] Check routing paths correctness
- [ ] Analyze logs for protocol correctness

### Task 43: Create Medium-Scale Test Scenario (150 Nodes)
- [ ] Set up topology with ~150 nodes (single cluster limit)
- [ ] Test cluster formation at capacity boundary
- [ ] Validate PDSF limiting prevents over-capacity
- [ ] Measure convergence time
- [ ] Check clusterhead election quality
- [ ] Analyze network coverage

### Task 44: Create Large-Scale Test Scenario (1000+ Nodes)
- [ ] Set up topology with 1000-10,000 nodes
- [ ] Run full protocol simulation
- [ ] Validate multiple cluster formation
- [ ] Measure end-to-end latency (must be in seconds)
- [ ] Check cluster distribution and size
- [ ] Analyze scalability bottlenecks

### Task 45: Test and Tune Crowding Factor Parameters
- [ ] Run simulations with varying node densities
- [ ] Tune f(RSSI) function parameters
- [ ] Adjust forwarding percentage thresholds
- [ ] Optimize for congestion vs coverage tradeoff
- [ ] Test in sparse, medium, and dense deployments
- [ ] Document optimal parameter ranges

### Task 46: Test and Tune GPS Proximity Threshold
- [ ] Run simulations with different proximity thresholds
- [ ] Measure impact on message propagation distance
- [ ] Balance redundancy vs unnecessary forwarding
- [ ] Test with GPS-available and GPS-denied scenarios
- [ ] Optimize threshold for different deployment areas
- [ ] Document recommended threshold values

### Task 47: Test and Tune Clusterhead Candidacy Threshold
- [ ] Vary dynamic threshold based on crowding factor
- [ ] Test candidacy criteria in different topologies
- [ ] Optimize for well-distributed clusterhead placement
- [ ] Balance cluster size vs number of clusters
- [ ] Validate threshold adaptivity to network density
- [ ] Document threshold formula and parameters

### Task 48: Validate End-to-End Latency Requirements
- [ ] Measure latency from edge to edge (through clusterheads)
- [ ] Ensure latency in seconds range (not minutes)
- [ ] Test with different network sizes
- [ ] Identify latency bottlenecks
- [ ] Optimize protocol timing if needed
- [ ] Document latency vs network size relationship

### Task 49: Analyze Network Convergence Time
- [ ] Measure time until all nodes assigned to clusters
- [ ] Analyze convergence vs network size
- [ ] Test impact of 3-round election on convergence
- [ ] Identify convergence bottlenecks
- [ ] Compare with theoretical expectations
- [ ] Document convergence characteristics

### Task 50: Test Robustness with Node Failures
- [ ] Simulate node failures during discovery
- [ ] Simulate clusterhead failures
- [ ] Test network recovery and re-election
- [ ] Validate multi-path redundancy effectiveness
- [ ] Test with dynamic node join/leave
- [ ] Document fault tolerance capabilities

---

## Phase 8: Optimization & Documentation

### Task 51: Benchmark Simulation Performance
- [ ] Measure simulation runtime vs network size
- [ ] Profile CPU and memory usage
- [ ] Identify performance bottlenecks
- [ ] Test maximum simulatable network size
- [ ] Compare with NS-3 best practices
- [ ] Document performance characteristics

### Task 52: Optimize for Large-Scale Scenarios
- [ ] Optimize data structures (use efficient containers)
- [ ] Reduce logging overhead in large simulations
- [ ] Implement event batching where applicable
- [ ] Optimize path computation algorithms
- [ ] Consider parallel execution if supported
- [ ] Re-benchmark after optimizations

### Task 53: Document Protocol Implementation and Usage
- [ ] Write comprehensive module documentation
- [ ] Document all classes and methods (Doxygen)
- [ ] Create user guide for running simulations
- [ ] Document configuration parameters and defaults
- [ ] Provide example simulation scripts
- [ ] Write design document explaining implementation choices
- [ ] Create troubleshooting guide
- [ ] Document known limitations and future work

---

## Additional Future Work (Not in Scope)

These tasks are mentioned in the protocol document but noted as "not implemented in scope":

- [ ] Implement FDMA/TDMA slotting for edge-to-clusterhead communication
- [ ] Implement clusterhead-to-clusterhead path discovery and establishment
- [ ] Implement full end-to-end routing through clusterhead network
- [ ] Implement data transmission phase (post-discovery)
- [ ] Add network maintenance and periodic re-election
- [ ] Implement node mobility handling and cluster handoff

---

## Key Protocol Parameters to Configure

| Parameter | Description | Suggested Range |
|-----------|-------------|-----------------|
| Discovery Cycle Duration | Time for 4-slot discovery cycle | 100-500ms |
| TTL Initial Value | Starting TTL for discovery messages | 5-10 hops |
| Crowding Factor Threshold | Percentage of messages to forward | 20-80% |
| GPS Proximity Threshold | Minimum distance for forwarding | 10-50m |
| Cluster Capacity | Maximum devices per cluster | 150 (fixed) |
| Candidacy Connection Threshold | Minimum direct connections for candidacy | 10-30 |
| Election Rounds | Number of announcement rounds | 3 (fixed) |
| BLE Transmission Range | Radio range | 30-100m |
| Network Size | Number of nodes | 100-10,000 |

---

## Success Criteria

- [ ] Protocol successfully scales to 1000+ nodes
- [ ] End-to-end latency remains in seconds range
- [ ] Cluster sizes respect 150-device limit
- [ ] Clusterhead election converges reliably
- [ ] Network achieves full coverage (all nodes assigned)
- [ ] Simulation runs efficiently for target network sizes
- [ ] Protocol behavior matches specification document

---

## References

- **Protocol Specification**: "Clusterhead & BLE Mesh discovery process" by jason.peng (November 2025)
- **NS-3 Documentation**: https://www.nsnam.org/documentation/
- **BLE Specification**: Bluetooth Core Specification v5.x
- **NS-3 Tutorials**: https://www.nsnam.org/tutorials/

---

**Last Updated**: 2025-11-19
**Status**: Ready to begin implementation
