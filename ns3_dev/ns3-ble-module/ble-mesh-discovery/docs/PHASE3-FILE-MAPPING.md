# Phase 3: Clusterhead Election - Discovery Phase
## File Mapping Reference

This document maps each Phase 3 task (Tasks 12-18) to its corresponding implementation files, wrapper functions, and test files.

---

## Overview

Phase 3 implements the **Clusterhead Election - Discovery Phase** of the BLE Mesh Discovery Protocol. The implementation follows a **Pure C Core + C++ Wrapper** architecture:

- **C Core Files** (`model/protocol-core/`) - Portable to embedded systems, no NS-3 dependencies
- **C++ Wrapper Files** (`model/`) - NS-3 integration layer
- **C Test Files** (`test/*-c-test.c`) - Standalone C tests
- **C++ Test Files** (`test/*-test.cc`) - NS-3 integration tests

---

## Task 12: Implement Noisy Broadcast Phase

**Description**: Implement "noisy broadcast" message transmission with stochastic randomized listening time slot selection and RSSI measurement during listening phase.

### Implementation Files

| File Type | Path |
|-----------|------|
| **C Header** | `model/protocol-core/ble_broadcast_timing.h` |
| **C Source** | `model/protocol-core/ble_broadcast_timing.c` |
| **C++ Wrapper Header** | `model/ble-broadcast-timing.h` |
| **C++ Wrapper Source** | `model/ble-broadcast-timing.cc` |

### Key Functions

| Function | Description |
|----------|-------------|
| `ble_broadcast_timing_init()` | Initialize timing state with schedule type |
| `ble_broadcast_timing_advance_slot()` | Advance slot with stochastic selection |
| `ble_broadcast_timing_should_broadcast()` | Check if current slot is for broadcasting |
| `ble_broadcast_timing_should_listen()` | Check if current slot is for listening |

### Test Files

| Test Type | Path |
|-----------|------|
| **Standalone C Tests** | `test/ble-broadcast-timing-c-test.c` |
| **NS-3 Integration Tests** | `test/ble-broadcast-timing-test.cc` |

### Test Suite
```bash
./test.py -s ble-broadcast-timing
```

---

## Task 13: Implement RSSI-Based Crowding Factor Calculation

**Description**: Define crowding factor function f(RSSI), implement RSSI aggregation from multiple samples, and calculate noise level from measurements.

### Implementation Files

| File Type | Path |
|-----------|------|
| **C Header** | `model/protocol-core/ble_election.h` |
| **C Source** | `model/protocol-core/ble_election.c` |
| **C++ Wrapper Header** | `model/ble-election.h` |
| **C++ Wrapper Source** | `model/ble-election.cc` |

### Key Functions

| Function | Description |
|----------|-------------|
| `ble_election_add_rssi_sample()` | Stores RSSI samples (circular buffer, 100 samples) |
| `ble_election_calculate_crowding()` | RSSI to crowding factor conversion (0.0-1.0) |

### Algorithm
- Formula: `(mean_rssi - RSSI_MIN) / (RSSI_MAX - RSSI_MIN)`
- RSSI range: -90 dBm (not crowded) to -40 dBm (very crowded)
- Returns bounded value 0.0-1.0

### Test Files

| Test Type | Path |
|-----------|------|
| **Standalone C Tests** | `test/ble-mesh-node-c-test.c` (includes crowding tests) |
| **NS-3 Integration Tests** | `test/ble-mesh-node-test.cc` (ElectionTestCase) |

### Test Suite
```bash
./test.py -s ble-mesh-node
```

---

## Task 14: Implement Stochastic Broadcast Timing

**Description**: Implement random time slot selection for clusterhead broadcasts with collision avoidance through randomization.

### Implementation Files

| File Type | Path |
|-----------|------|
| **C Header** | `model/protocol-core/ble_broadcast_timing.h` |
| **C Source** | `model/protocol-core/ble_broadcast_timing.c` |
| **C++ Wrapper Header** | `model/ble-broadcast-timing.h` |
| **C++ Wrapper Source** | `model/ble-broadcast-timing.cc` |

**Note**: Tasks 12 and 14 share the same implementation files as they address complementary aspects of broadcast timing.

### Key Functions

| Function | Description |
|----------|-------------|
| `ble_broadcast_timing_set_seed()` | Set random seed for stochastic timing |
| `ble_broadcast_timing_advance_slot()` | Random slot selection with listen ratio |
| `ble_broadcast_timing_record_success()` | Track successful broadcasts |
| `ble_broadcast_timing_record_failure()` | Retry logic (returns true if should retry) |
| `ble_broadcast_timing_get_success_rate()` | Calculate success rate |
| `ble_broadcast_timing_get_actual_listen_ratio()` | Verify distribution |

### Algorithm
- Uses Linear Congruential Generator (LCG) for randomization
- Each slot: `random_value < listen_ratio` → listen, else → broadcast
- Default: 80% listen, 20% broadcast
- Max 3 retries for failed broadcasts

### Test Files

| Test Type | Path |
|-----------|------|
| **Standalone C Tests** | `test/ble-broadcast-timing-c-test.c` |
| **NS-3 Integration Tests** | `test/ble-broadcast-timing-test.cc` |

### Test Suite
```bash
./test.py -s ble-broadcast-timing
```

---

## Task 15: Implement Direct Connection Counting

**Description**: Add direct connection counter to node state, track successfully parsed messages with strong signal, and store list of 1-hop neighbors.

### Implementation Files

| File Type | Path |
|-----------|------|
| **C Header** | `model/protocol-core/ble_election.h` |
| **C Source** | `model/protocol-core/ble_election.c` |
| **C++ Wrapper Header** | `model/ble-election.h` |
| **C++ Wrapper Source** | `model/ble-election.cc` |

**Also uses:**
| File Type | Path |
|-----------|------|
| **C Header** | `model/protocol-core/ble_mesh_node.h` |
| **C Source** | `model/protocol-core/ble_mesh_node.c` |
| **C++ Wrapper Header** | `model/ble-mesh-node-wrapper.h` |
| **C++ Wrapper Source** | `model/ble-mesh-node-wrapper.cc` |

### Key Functions

| Function | Description |
|----------|-------------|
| `ble_election_update_neighbor()` | Updates neighbor info; neighbors heard during the direct-discovery window are flagged `is_direct = true` |
| `ble_election_count_direct_connections()` | Counts neighbors where `is_direct == true` |

### Algorithm
- `is_direct = true` for any neighbor heard during the stochastic direct-discovery phase (0-hop broadcast); no RSSI threshold required.
- Only successfully decoded broadcasts counted

### Test Files

| Test Type | Path |
|-----------|------|
| **Standalone C Tests** | `test/ble-mesh-node-c-test.c` |
| **NS-3 Integration Tests** | `test/ble-mesh-node-test.cc` |

### Test Suite
```bash
./test.py -s ble-mesh-node
```

---

## Task 16: Implement Connectivity Metric Tracking

**Description**: Track number of direct neighbors, unique paths from forwarded messages, and implement geographic distribution calculation using LHGPS.

### Implementation Files

| File Type | Path |
|-----------|------|
| **C Header** | `model/protocol-core/ble_election.h` |
| **C Source** | `model/protocol-core/ble_election.c` |
| **C++ Wrapper Header** | `model/ble-election.h` |
| **C++ Wrapper Source** | `model/ble-election.cc` |

### Key Functions

| Function | Description |
|----------|-------------|
| `ble_election_update_metrics()` | Updates all metrics before candidacy check |

### Metrics Tracked (in `ble_connectivity_metrics_t`)

| Metric | Description |
|--------|-------------|
| `direct_connections` | 1-hop neighbors count |
| `total_neighbors` | All unique neighbors discovered |
| `crowding_factor` | Local crowding (0.0-1.0) |
| `connection_noise_ratio` | direct / (1 + crowding) |
| `geographic_distribution` | Spatial distribution score (0.0-1.0) |
| `messages_forwarded` | Successfully forwarded count |
| `messages_received` | Total received count |
| `forwarding_success_rate` | Ratio of forwarded/received |

**Notes:** `geographic_distribution` is captured for analytics and logging but is not presently used to gate candidacy decisions; the direct neighbor and connection:noise metrics drive the discovery behavior.

### Test Files

| Test Type | Path |
|-----------|------|
| **Standalone C Tests** | `test/ble-mesh-node-c-test.c` |
| **NS-3 Integration Tests** | `test/ble-mesh-node-test.cc` |

### Test Suite
```bash
./test.py -s ble-mesh-node
```

---

## Task 17: Implement Clusterhead Candidacy Determination

**Description**: Calculate direct connection count : noise heard ratio, implement candidacy threshold calculation, and create candidacy decision logic combining all criteria.

### Implementation Files

| File Type | Path |
|-----------|------|
| **C Header** | `model/protocol-core/ble_election.h` |
| **C Source** | `model/protocol-core/ble_election.c` |
| **C++ Wrapper Header** | `model/ble-election.h` |
| **C++ Wrapper Source** | `model/ble-election.cc` |

### Key Functions

| Function | Description |
|----------|-------------|
| `ble_election_should_become_candidate()` | Complete candidacy logic |
| `ble_election_calculate_candidacy_score()` | Protocol scoring formula (direct connections + connection:noise ratio) |
| `ble_election_set_thresholds()` | Configure candidacy thresholds |

### Candidacy Thresholds (configurable)

| Threshold | Default Value |
|-----------|---------------|
| `min_neighbors_for_candidacy` | 10 |
| `min_connection_noise_ratio` | 5.0 |

### Decision Logic
1. Check `direct_connections >= min_neighbors`
2. Check `connection_noise_ratio >= min_ratio`
3. If both pass → set `is_candidate = true`, calculate score

### Candidacy Scoring (per protocol)
Score = `direct_connections + (direct_connections / BLE_DISCOVERY_MAX_CLUSTER_SIZE) / (noise + 1)`
- Emphasizes high connectivity while penalizing noisy environments
- No weighting knobs; mirrors discovery_protocol.txt

### Test Files

| Test Type | Path |
|-----------|------|
| **Standalone C Tests** | `test/ble-mesh-node-c-test.c` |
| **NS-3 Integration Tests** | `test/ble-mesh-node-test.cc` |

### Test Suite
```bash
./test.py -s ble-mesh-node
```

---

## Task 18: Implement Geographic Distribution Analysis

**Description**: Calculate spatial distribution of neighbors using LHGPS, implement clustering detection algorithm, and handle GPS-unavailable scenarios.

### Implementation Files

| File Type | Path |
|-----------|------|
| **C Header** | `model/protocol-core/ble_election.h` |
| **C Source** | `model/protocol-core/ble_election.c` |
| **C++ Wrapper Header** | `model/ble-election.h` |
| **C++ Wrapper Source** | `model/ble-election.cc` |

### Key Functions

| Function | Description |
|----------|-------------|
| `ble_election_calculate_geographic_distribution()` | Variance-based clustering detection |

### Algorithm
1. Calculate centroid (mean x, y, z) of all neighbor GPS locations
2. Calculate variance of distances from centroid
3. Compute standard deviation: `sqrt(variance)`
4. Normalize: `std_dev / 100.0` (capped at 1.0)
5. Return: 0.0 (clustered) to 1.0 (well-distributed)

### GPS Unavailable Handling
- Returns 0.0 if < 2 neighbors have valid GPS
- Metric stored for analytics; candidacy logic currently ignores geographic distribution regardless of GPS availability
- Gracefully handles zero-location (0, 0, 0) entries

### Test Files

| Test Type | Path |
|-----------|------|
| **Standalone C Tests** | `test/ble-mesh-node-c-test.c` |
| **NS-3 Integration Tests** | `test/ble-mesh-node-test.cc` |

### Test Suite
```bash
./test.py -s ble-mesh-node
```

---

## Summary Table

| Task | Description | C Core Files | C++ Wrapper Files | Test Files |
|------|-------------|--------------|-------------------|------------|
| **12** | Noisy Broadcast Phase | `ble_broadcast_timing.{h,c}` | `ble-broadcast-timing.{h,cc}` | `ble-broadcast-timing-c-test.c`, `ble-broadcast-timing-test.cc` |
| **13** | RSSI Crowding Factor | `ble_election.{h,c}` | `ble-election.{h,cc}` | `ble-mesh-node-c-test.c`, `ble-mesh-node-test.cc` |
| **14** | Stochastic Broadcast Timing | `ble_broadcast_timing.{h,c}` | `ble-broadcast-timing.{h,cc}` | `ble-broadcast-timing-c-test.c`, `ble-broadcast-timing-test.cc` |
| **15** | Direct Connection Counting | `ble_election.{h,c}`, `ble_mesh_node.{h,c}` | `ble-election.{h,cc}`, `ble-mesh-node-wrapper.{h,cc}` | `ble-mesh-node-c-test.c`, `ble-mesh-node-test.cc` |
| **16** | Connectivity Metrics | `ble_election.{h,c}` | `ble-election.{h,cc}` | `ble-mesh-node-c-test.c`, `ble-mesh-node-test.cc` |
| **17** | Candidacy Determination | `ble_election.{h,c}` | `ble-election.{h,cc}` | `ble-mesh-node-c-test.c`, `ble-mesh-node-test.cc` |
| **18** | Geographic Distribution | `ble_election.{h,c}` | `ble-election.{h,cc}` | `ble-mesh-node-c-test.c`, `ble-mesh-node-test.cc` |

---

## File Locations (Full Paths)

### C Core Files (Protocol Logic)
```
model/protocol-core/
├── ble_broadcast_timing.h    # Tasks 12, 14
├── ble_broadcast_timing.c    # Tasks 12, 14
├── ble_election.h            # Tasks 13, 15, 16, 17, 18
├── ble_election.c            # Tasks 13, 15, 16, 17, 18
├── ble_mesh_node.h           # Task 15 (neighbor storage)
└── ble_mesh_node.c           # Task 15 (neighbor storage)
```

### C++ Wrapper Files (NS-3 Integration)
```
model/
├── ble-broadcast-timing.h    # Tasks 12, 14 wrapper
├── ble-broadcast-timing.cc   # Tasks 12, 14 wrapper
├── ble-election.h            # Tasks 13, 15, 16, 17, 18 wrapper
├── ble-election.cc           # Tasks 13, 15, 16, 17, 18 wrapper
├── ble-mesh-node-wrapper.h   # Task 15 wrapper
└── ble-mesh-node-wrapper.cc  # Task 15 wrapper
```

### Test Files
```
test/
├── ble-broadcast-timing-c-test.c   # Standalone C tests (Tasks 12, 14)
├── ble-broadcast-timing-test.cc    # NS-3 tests (Tasks 12, 14)
├── ble-mesh-node-c-test.c          # Standalone C tests (Tasks 13, 15, 16, 17, 18)
└── ble-mesh-node-test.cc           # NS-3 tests (Tasks 13, 15, 16, 17, 18)
```

---

## Running All Phase 3 Tests

```bash
# Run all Phase 3 related test suites
./test.py -s ble-broadcast-timing   # Tasks 12, 14
./test.py -s ble-mesh-node          # Tasks 13, 15, 16, 17, 18

# Run standalone C tests (for embedded validation)
gcc -o test_broadcast test/ble-broadcast-timing-c-test.c \
    model/protocol-core/ble_broadcast_timing.c -lm
./test_broadcast

gcc -o test_node test/ble-mesh-node-c-test.c \
    model/protocol-core/ble_mesh_node.c \
    model/protocol-core/ble_election.c \
    model/protocol-core/ble_discovery_packet.c -lm
./test_node
```

---

## Architecture Notes

### Why C Core + C++ Wrapper?

1. **Portability** - C core can compile for ARM Cortex-M, ESP32, nRF52, etc.
2. **Testability** - Test C core without NS-3 dependency
3. **Maintainability** - Protocol logic separate from simulation
4. **Reusability** - Same code for simulation and real hardware
5. **Performance** - C is faster and produces smaller binaries

### Wrapper Pattern Example

```cpp
// C++ Wrapper delegates to C core
bool BleElectionWrapper::ShouldBecomeCandidate() {
    return ble_election_should_become_candidate(&m_state);
}
```

---

*Last Updated: 2025-11-22*
