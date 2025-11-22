# Code Review - Inspected Functions

This document tracks functions that have been reviewed and approved for the BLE mesh discovery protocol implementation.

---

## `ble_broadcast_timing.{h,c}` - Stochastic Broadcast Timing

### ✅ `ble_broadcast_timing_init(state, type, num_slots, slot_duration_ms, listen_ratio)`

**Location**: [ble_broadcast_timing.c:92-146](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_broadcast_timing.c#L92-L146)

**Purpose**: Initialize a BLE broadcast timing state structure for managing stochastic broadcast schedules.

**Design Choices**:
- **Pure C implementation**: No dependencies on C++ or NS-3, portable to embedded systems
- **Auto-profile defaults**:
  - `BLE_BROADCAST_AUTO_SLOTS` (0) triggers schedule-specific defaults
  - `BLE_BROADCAST_AUTO_RATIO` (-1.0) uses phase-appropriate listen ratios
  - Noisy schedule: 10 slots, 10% listen ratio
  - Stochastic schedule: 200 slots, 90% listen ratio
- **Parameter validation**: Clamps listen_ratio (0.0-1.0), num_slots (≤256)
- **Zero initialization**: Uses `memset` to ensure clean state
- **Dual schedule support**:
  - `BLE_BROADCAST_SCHEDULE_NOISY` - Task 12: Randomized collision avoidance
  - `BLE_BROADCAST_SCHEDULE_STOCHASTIC` - Task 14: Neighbor discovery with crowding adaptation
- **Crowding-aware**: For stochastic schedules, applies neighbor profile that dynamically calculates 3-15 broadcast slots based on crowding factor
- **Statistics tracking**: Initializes counters for broadcast/listen slots, success/failure rates
- **Retry logic**: Configures max retries (default: 3)

**Status**: ✅ **PASSED** - Design is sound for embedded BLE mesh applications

---

### ✅ `ble_broadcast_timing_set_seed(state, seed)`

**Location**: [ble_broadcast_timing.c:148-153](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_broadcast_timing.c#L148-L153)

**Purpose**: Configure the Random Number Generator (RNG) seed for stochastic slot selection.

**Design Choices**:
- **Simple setter**: Minimal function that updates the seed field in the state structure
- **Per-instance randomization**: Each `ble_broadcast_timing_t` maintains its own seed, avoiding global state
- **Deterministic testing**: Allows reproducible test scenarios by setting explicit seeds
- **Collision avoidance**: Different nodes should use different seeds to desynchronize broadcast schedules
- **Lightweight RNG**: Uses Linear Congruential Generator (LCG) with constants `LCG_A=1664525U`, `LCG_C=1013904223U`
- **Default seed**: Initialized to 12345 in `ble_broadcast_timing_init`, but can be overridden
- **Null-safe**: Checks for valid state pointer before updating

**Use Cases**:
- **Testing**: All unit tests set explicit seeds (12345, 42, 999, 777, 111, 222) for reproducibility
- **Multi-node simulations**: Each node gets unique seed to prevent synchronized collisions
- **NS-3 integration**: Allows simulator to control randomness while maintaining reproducibility

**Status**: ✅ **PASSED** - Appropriate design for embedded and simulation contexts

---

### ✅ `ble_broadcast_timing_advance_slot(state)`

**Location**: [ble_broadcast_timing.c:168-221](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_broadcast_timing.c#L168-L221)

**Purpose**: Core state machine function that advances to the next time slot and stochastically decides whether that slot should be used for broadcasting or listening.

**Design Choices**:
- **Dual-mode operation**:
  - **Noisy broadcast (Task 12)**: Simple probabilistic decision using listen_ratio, no broadcast constraints
  - **Stochastic timing (Task 14)**: Enforces broadcast budget (`max_broadcast_slots`), implements majority-listening pattern
- **Slot advancement**: Increments slot index with modulo wraparound, resets broadcast counter at cycle boundary
- **Randomization for collision avoidance**: Uses LCG to generate random value (0.0-1.0) compared against listen_ratio
- **Broadcast budget enforcement**: In stochastic mode, caps broadcasts per cycle at 3-15 slots based on crowding factor
- **Forced listening**: Once quota exhausted, remaining slots automatically become listen slots
- **Automatic statistics**: Updates counters (`total_broadcast_slots`, `total_listen_slots`, `broadcast_attempts`) on each call
- **Dual interface**: Returns boolean for immediate use, also updates `is_broadcast_slot` flag for later queries
- **Cycle reset semantics**: When `current_slot` wraps to 0, `broadcasts_this_cycle` resets to enable new cycle

**Algorithm**:
1. Advance slot index: `current_slot = (current_slot + 1) % num_slots`
2. Generate random value: `0.0 ≤ random_value < 1.0`
3. Apply schedule logic:
   - **Noisy**: `random_value < listen_ratio` → listen, else broadcast
   - **Stochastic**: Check budget first, then `random_value < listen_ratio` → listen, else broadcast

**Key Insight**: This is the heart of collision avoidance - different seeds produce different random sequences, preventing synchronized transmissions between nodes.

**Status**: ✅ **PASSED** - Robust design balancing simplicity, efficiency, and collision avoidance

---

### ✅ `ble_broadcast_timing_should_broadcast(state)`

**Location**: [ble_broadcast_timing.c:223-227](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_broadcast_timing.c#L223-L227)

**Purpose**: Query function to check if the current time slot is designated for broadcasting (vs. listening).

**Design Choices**:
- **Read-only query**: Uses `const` parameter to guarantee no state modification
- **Cached decision**: Returns pre-computed `is_broadcast_slot` flag set by `advance_slot()`
- **NULL safety**: Returns `false` on NULL pointer (safe default - passive listening mode)
- **No recomputation**: Single source of truth, consistent results across multiple queries within same slot
- **Compiler-friendly**: Simple 1-line return likely to be inlined
- **Conservative default**: Defaults to NOT broadcasting when state is invalid (prevents interference)

**Companion Function**: `ble_broadcast_timing_should_listen(state)` provides inverse check, defaults to `true` on NULL (also passive mode).

**Usage Patterns**:
- **Pattern 1**: Check after advancement - `advance_slot()` then `should_broadcast()`
- **Pattern 2**: Use `advance_slot()` return value directly (more concise)
- **Pattern 3**: Multiple checks within same slot without re-advancing

**Status**: ✅ **PASSED** - Exemplifies embedded best practices: simple, fast, safe, predictable

---

### ✅ `ble_broadcast_timing_should_listen(state)`

**Location**: [ble_broadcast_timing.c:229-233](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_broadcast_timing.c#L229-L233)

**Purpose**: Query function to check if the current time slot is designated for listening (vs. broadcasting).

**Design Choices**:
- **Inverse of `should_broadcast()`**: Returns `!is_broadcast_slot`
- **Read-only query**: Uses `const` parameter
- **NULL safety**: Returns `true` on NULL pointer (safe default - passive listening mode)
- **Symmetric API**: Provides clear, readable alternative to `!should_broadcast()`
- **Same guarantees**: No recomputation, consistent results, compiler-friendly

**Key Difference from `should_broadcast()`**: NULL behavior defaults to `true` (listen) instead of `false`, maintaining consistent "passive mode on error" semantics.

**Status**: ✅ **PASSED** - Clean API design with symmetric, intuitive semantics

---

### ✅ `ble_broadcast_timing_record_success(state)`

**Location**: [ble_broadcast_timing.c:235-242](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_broadcast_timing.c#L235-L242)

**Purpose**: Record that a broadcast transmission was successful, updating statistics and resetting retry logic.

**Design Choices**:
- **Three state updates**:
  - `successful_broadcasts++`: Increments success counter for statistics
  - `message_sent = true`: Sets flag indicating message successfully transmitted
  - `retry_count = 0`: **Critical** - Resets retry counter since transmission succeeded
- **Per-message retry logic**: Retry counter is per-message, not cumulative; success makes previous failures irrelevant
- **No return value**: Fire-and-forget update; success is terminal state requiring no decision
- **Null safety**: Guards against invalid state pointer
- **Automatic reset**: Caller doesn't need to manually reset retry counter after success

**Key Insight**: Success resets retry state because each message's retry attempts are independent.

**Status**: ✅ **PASSED** - Simple, effective state management for success path

---

### ✅ `ble_broadcast_timing_record_failure(state)`

**Location**: [ble_broadcast_timing.c:244-259](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_broadcast_timing.c#L244-L259)

**Purpose**: Record broadcast failure, increment retry counter, and decide whether to retry or give up.

**Design Choices**:
- **Boolean return value** - Key design decision:
  - Returns `true` if caller should retry transmission
  - Returns `false` if max retries exhausted (give up)
  - Encapsulates retry decision logic in one place
- **Retry budget enforcement**: Default `max_retries = 3` allows up to 3 retry attempts (4 total attempts)
- **Auto-reset after exhaustion**: When max retries reached, automatically resets `retry_count = 0` for next message
- **Statistics tracking**: `failed_broadcasts++` counts **every** failure, including each retry attempt
- **Null safety**: Returns `false` (don't retry) on invalid state
- **State machine behavior**:
  - Attempt 1 fails → `retry_count = 1`, return `true` (retry)
  - Attempt 2 fails → `retry_count = 2`, return `true` (retry)
  - Attempt 3 fails → `retry_count = 0` (reset), return `false` (give up)

**Usage Pattern**:
```c
if (!transmit_succeeded) {
    if (ble_broadcast_timing_record_failure(&state)) {
        retry_transmission();  // Try again
    } else {
        log_permanent_failure();  // Give up
    }
}
```

**Key Insight**: Function encapsulates entire retry decision logic, providing automatic state management.

**Status**: ✅ **PASSED** - Robust retry mechanism with clear decision API

---

### ✅ `ble_broadcast_timing_reset_retry(state)`

**Location**: [ble_broadcast_timing.c:261-267](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_broadcast_timing.c#L261-L267)

**Purpose**: Manually reset retry logic state to prepare for a new message transmission.

**Design Choices**:
- **Two-field reset**:
  - `retry_count = 0`: Clears accumulated retry attempts
  - `message_sent = false`: Clears success flag
- **Manual control**: Application explicitly signals transition to new message
- **Separation of concerns**:
  - Statistics (`successful_broadcasts`, `failed_broadcasts`) remain unchanged - accumulate lifetime
  - Retry logic (per-message) is reset
- **Three reset mechanisms in the retry system**:
  - `record_success()` - Automatic reset on success
  - `record_failure()` at max - Automatic reset after exhaustion
  - `reset_retry()` - **Manual reset for new message**
- **Void return type**: Simple state reset, no decision needed
- **Null safety**: Guards against invalid state pointer

**Use Cases**:
- Starting transmission of a new message
- Manual intervention to abandon retry sequence
- State cleanup after timeout/external event
- Testing - reset between scenarios

**Key Insight**: Provides the **missing transition** between messages in a persistent state machine. Without this, no way to signal "I'm done with this message, starting a new one."

**Contrast with Automatic Resets**:
- `record_success()`: Resets `retry_count` only
- `record_failure()`: Resets `retry_count` only (when max reached)
- `reset_retry()`: Resets **both** `retry_count` and `message_sent` (fresh start)

**Status**: ✅ **PASSED** - Essential for multi-message protocols with persistent state

---

### ✅ `ble_broadcast_timing_get_success_rate(state)`

**Location**: [ble_broadcast_timing.c:269-277](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_broadcast_timing.c#L269-L277)

**Purpose**: Compute broadcast transmission success rate as ratio between 0.0 and 1.0.

**Design Choices**:
- **Formula**: `successful_broadcasts / (successful_broadcasts + failed_broadcasts)`
- **Return type**: `double` (0.0-1.0) - standard representation for rates/probabilities
- **Division by zero protection**: Returns 0.0 if no transmissions attempted yet
- **Explicit casting**: Both operands cast to `double` to ensure floating-point division
- **Const parameter**: Read-only query, no side effects
- **NULL safety**: Returns 0.0 on NULL pointer
- **Includes retries**: Each retry failure increments counter, so rate reflects per-attempt reliability

**Key Insight**: Success rate includes all retry attempts, providing visibility into transmission reliability rather than message delivery rate.

**Status**: ✅ **PASSED** - Well-designed metrics accessor with proper edge case handling

---

### ✅ `ble_broadcast_timing_get_current_slot(state)`

**Location**: [ble_broadcast_timing.c:279-283](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_broadcast_timing.c#L279-L283)

**Purpose**: Return current slot index (0 to num_slots-1) representing position in time-slotted schedule.

**Design Choices**:
- **Simple accessor**: 4-line getter function
- **Return type**: `uint32_t` (unsigned, matches field type)
- **Const parameter**: Read-only query
- **NULL safety**: Returns 0 (first slot) on NULL pointer
- **Encapsulation**: Hides internal structure, provides API stability
- **Zero overhead**: Likely inlined by compiler

**Value managed by**: `advance_slot()` increments with modulo wraparound

**Usage**: Debugging, synchronization checks, cycle detection, testing

**Status**: ✅ **PASSED** - Textbook example of well-designed accessor function

---

### ✅ `ble_broadcast_timing_get_actual_listen_ratio(state)`

**Location**: [ble_broadcast_timing.c:285-293](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_broadcast_timing.c#L285-L293)

**Purpose**: Compute observed listen ratio based on accumulated slot statistics.

**Design Choices**:
- **Formula**: `total_listen_slots / (total_listen_slots + total_broadcast_slots)`
- **Derived metric**: Computed on-demand rather than stored (no redundant state)
- **Validation tool**: Verifies stochastic algorithm produces expected distribution
- **Return type**: `double` (0.0-1.0)
- **Division by zero protection**: Returns 0.0 if no slots processed yet
- **Const parameter**: Read-only query
- **NULL safety**: Returns 0.0 on NULL pointer

**Key Insight**: "Actual" vs. "configured" - this returns observed behavior, distinct from configured `listen_ratio` parameter. Essential for validating randomization quality.

**Status**: ✅ **PASSED** - Essential observability for stochastic protocol validation

---

### ✅ `ble_broadcast_timing_rand(seed)` / `ble_broadcast_timing_rand_double(seed)`

**Locations**:
- `rand`: [ble_broadcast_timing.c:155-160](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_broadcast_timing.c#L155-L160)
- `rand_double`: [ble_broadcast_timing.c:162-166](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_broadcast_timing.c#L162-L166)

**Purpose**: Internal RNG helpers implementing Linear Congruential Generator (LCG) for stochastic slot selection.

**Design Choices**:
- **LCG Algorithm**: `next = (A × current + C) mod 2^32` with Numerical Recipes constants
  - `A = 1664525U` (multiplier)
  - `C = 1013904223U` (increment)
- **Why LCG**: Simplicity, determinism, no dependencies, minimal code size, O(1) performance, portability
- **Pointer parameter**: Seed updated in-place for automatic state persistence
- **Layered design**: `rand_double()` calls `rand()`, converts to [0.0, 1.0) range
- **NULL safety**: Returns 0 on NULL pointer
- **No stdlib dependency**: Custom implementation avoids `rand()` portability issues
- **Thread-safe pattern**: Each context maintains own seed (no global state)

**Trade-offs accepted**: Not cryptographically secure (not needed), lower quality than Mersenne Twister (sufficient for collision avoidance)

**Status**: ✅ **PASSED** - Excellent RNG choice for embedded BLE mesh. Simple, portable, deterministic, fit-for-purpose.

---

### ✅ `ble_broadcast_timing_set_crowding(state, crowding_factor)`

**Location**: [ble_broadcast_timing.c:294-306](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_broadcast_timing.c#L294-L306)

**Purpose**: Dynamically adjust neighbor-discovery transmission parameters based on measured network crowding.

**Design Choices**:
- **Crowding range**: 0.0 (sparse) to 1.0 (very crowded), clamped for safety
- **Schedule-specific**: Only affects stochastic mode (Task 14 neighbor discovery)
- **Adaptive TX slots**: Inverse relationship - `tx_slots = 3 + (1.0 - crowding) × 12`
  - High crowding (1.0) → 3 slots (minimal broadcasts)
  - Low crowding (0.0) → 15 slots (more broadcasts)
- **Automatic recalculation**: Calls `apply_neighbor_profile()` to update both `max_broadcast_slots` and `listen_ratio`
- **Cycle reset**: Sets `broadcasts_this_cycle = 0` for clean transition to new limits
- **Conservative bounds**: 3-15 slot range ensures minimum reliability and maximum efficiency
- **Immediate effect**: No gradual transition, responsive to network changes

**Key Insight**: Implements adaptive collision avoidance - more crowding → fewer broadcasts → less interference. This is the heart of Task 14's adaptive mechanism.

**Status**: ✅ **PASSED** - Excellent adaptive design with proper inverse scaling and state management

---

### ✅ `ble_broadcast_timing_get_max_broadcast_slots(state)`

**Location**: [ble_broadcast_timing.c:308-314](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_broadcast_timing.c#L308-L314)

**Purpose**: Return maximum broadcast slots allowed per cycle for diagnostics and testing.

**Design Choices**:
- **Simple getter**: 6-line accessor following minimal pattern
- **Return type**: `uint32_t` (unsigned, range: 3-256)
- **Const parameter**: Read-only query
- **NULL safety**: Returns 0 on NULL pointer (safe default)
- **Value ranges by mode**:
  - Noisy schedule: 256 (no enforcement)
  - Stochastic schedule: 3-15 (adaptive based on crowding)
- **Observability**: Exposes internal adaptive parameter for debugging/monitoring

**Usage**: Diagnostics, testing adaptive behavior, protocol logic decisions

**Status**: ✅ **PASSED** - Well-designed diagnostic accessor providing visibility into adaptive collision avoidance

---

---

## `ble_election.{h,c}` - Clusterhead Election and Neighbor Management

### ✅ `ble_election_init(state)`

**Location**: [ble_election.c:30-59](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_election.c#L30-L59)

**Purpose**: Initialize election state structure with conservative defaults suitable for sparse-to-moderate density networks.

**Design Choices**:
- **Zero-initialization with memset**: Fast, safe initialization of entire 150-neighbor database
- **Conservative candidacy defaults**:
  - Direct RSSI threshold: -70 dBm (typical strong signal boundary)
  - Min neighbors: 10 (prevents premature candidacy in sparse networks)
  - Min connection:noise ratio: 5.0 (ensures significant connectivity advantage)
  - Min geographic distribution: 0.3 (requires moderate spatial spread, not clustered)
- **Equal score weights**: Default 0.25 for all four factors (direct connections, cn_ratio, geo distribution, forwarding rate) - neutral baseline
- **Explicit critical field initialization**: Despite memset, key fields explicitly set for code clarity
- **No dynamic allocation**: Fixed-size arrays for embedded compatibility

**Key Insight**: The high default thresholds (10 neighbors, 5.0 ratio, 0.3 distribution) create a "prove it" system - nodes must demonstrate significant connectivity before attempting clusterhead candidacy.

**Status**: ✅ **PASSED** - Well-balanced defaults prevent premature or weak candidacy

---

### ✅ `ble_election_update_neighbor(state, node_id, location, rssi, current_time_ms)`

**Location**: [ble_election.c:61-106](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_election.c#L61-L106)

**Design Choices**:
- **Update-or-insert pattern**: Linear search first, append if not found
- **Message count tracking**: Increments on every update, measures neighbor reliability
- **Direct connection classification**:
  - RSSI ≥ threshold (default -70 dBm) → `is_direct = true` (1-hop neighbor)
  - RSSI < threshold → `is_direct = false` (multi-hop or weak signal)
- **GPS location optional**: Accepts `NULL` location pointer, only updates when provided
- **Timestamp always updated**: `last_seen_time_ms` refreshed on every message
- **Overflow protection**: Silently ignores new neighbors at capacity (150 max)
- **O(n) complexity**: Linear search acceptable for 150-neighbor max

**Key Insight**: Prioritizes **reliability over recency** - tracks how many times neighbor was heard, not just most recent contact. The `is_direct` flag enables sophisticated routing and election decisions.

**Status**: ✅ **PASSED** - Robust neighbor tracking with appropriate tradeoffs for embedded BLE mesh

---

### ✅ `ble_election_add_rssi_sample(state, rssi, current_time_ms)` ⚠️ **UPGRADED**

**Location**: [ble_election.c:78-113](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_election.c#L78-L113)

**Purpose**: Add RSSI sample to circular buffer with timestamp for temporal filtering and automatic stale sample eviction.

**Design Choices**:
- **Proper circular buffer**: Head/tail pointers with O(1) insertion (improved from O(n) shift-based)
- **Timestamp storage**: Each sample stored with `ble_rssi_sample_t` struct (rssi + timestamp_ms)
- **Dual eviction strategy**:
  1. **Count-based**: Max 100 samples (memory bounded)
  2. **Time-based**: Samples older than `rssi_max_age_ms` (default 10 seconds) automatically evicted
- **Modulo arithmetic**: `(index + 1) % 100` for wraparound
- **Temporal order assumption**: Newer samples assumed to have later timestamps (enables early-exit in eviction loop)
- **Memory footprint**: 500 bytes (100 × 5-byte struct) - increased from 100 bytes

**Algorithm**:
```c
1. Store sample at tail: rssi_samples[tail] = {rssi, timestamp}
2. Advance tail: tail = (tail + 1) % 100
3. If buffer full: advance head to evict oldest
4. Age-based eviction:
   while oldest_sample.age > max_age_ms:
       evict by advancing head
```

**Key Insight**: **Mobile-node optimized** - ensures crowding measurements reflect only recent network conditions (≤10 seconds). For nodes moving between sparse and dense areas, this eliminates the 50-100 sample lag of the old shift-based approach, providing ~1-2 second adaptation time.

**Breaking Change**: Function signature changed from `(state, rssi)` to `(state, rssi, current_time_ms)` - all callers must be updated.

**Performance**:
- Insertion: O(n) → O(1) ✅ **Improved**
- Eviction: O(1) + O(k) where k = stale samples (typically 0-5)
- Memory: 100 bytes → 500 bytes (+400 bytes)

**Status**: ✅ **PASSED** - Superior design for mobile deployments with explicit temporal control

**See Also**: [RSSI_CIRCULAR_BUFFER_UPGRADE.md](RSSI_CIRCULAR_BUFFER_UPGRADE.md) for detailed migration guide

---

### ✅ `ble_election_calculate_crowding(state)`

**Location**: [ble_election.c:122-154](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_election.c#L122-L154)

**Design Choices**:
- **RSSI normalization range**: -90 dBm (sparse) to -40 dBm (crowded)
  - Below -90 dBm → crowding = 0.0 (very sparse, edge of BLE sensitivity)
  - Above -40 dBm → crowding = 1.0 (very crowded, close proximity)
  - Linear interpolation: `crowding = (avg_rssi + 90.0) / 50.0`
- **Average-based**: Computes mean of all samples in circular buffer
- **Clamped output**: Always in [0.0, 1.0] range
- **Division-by-zero protection**: Returns 0.0 if no samples collected
- **Const parameter**: Read-only query
- **No caching**: Recalculated on-demand (acceptable given simple averaging)

**Algorithm**:
```c
avg_rssi = Σ rssi_samples[i] / rssi_sample_count
normalized = (avg_rssi + 90.0) / 50.0
crowding = clamp(normalized, 0.0, 1.0)
```

**Key Insight**: The -90 to -40 dBm range captures realistic BLE environments. Linear mapping assumes uniform distribution of crowding levels across RSSI range. Higher (less negative) RSSI indicates more neighbors nearby = more crowded.

**Status**: ✅ **PASSED** - Well-chosen normalization range and simple, effective algorithm

---

### ✅ `ble_election_count_direct_connections(state)`

**Location**: [ble_election.c:156-168](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_election.c#L156-L168)

**Design Choices**:
- **Simple iteration**: Linear scan of neighbor array
- **Boolean flag check**: Counts neighbors where `is_direct == true`
- **O(n) complexity**: Acceptable for max 150 neighbors
- **No caching**: Recalculated on-demand (derived metric pattern)
- **Const parameter**: Read-only query
- **NULL safety**: Returns 0 on NULL pointer

**Key Insight**: **Derived metric** rather than stored state. Recalculated whenever needed, ensuring it always reflects current neighbor database without complex invalidation logic.

**Status**: ✅ **PASSED** - Clean implementation of derived metric pattern

---

### ✅ `ble_election_calculate_geographic_distribution(state)`

**Location**: [ble_election.c:170-230](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_election.c#L170-L230)

**Design Choices**:
- **Centroid-based variance**: Measures spatial spread around geometric center
- **Two-pass algorithm**:
  1. Calculate centroid (mean latitude, mean longitude)
  2. Calculate variance of Euclidean distances from centroid
- **Normalization**:
  - Max expected variance: 1000.0 (empirically chosen for ~100m+ spread)
  - Output: `min(1.0, sqrt(variance) / sqrt(1000.0))`
  - Square root transform provides gradual scaling
- **Minimum neighbor requirement**: Returns 0.0 if < 2 neighbors (variance undefined)
- **GPS availability check**: Skips neighbors with invalid GPS (`has_gps == false`)
- **Euclidean distance**: Standard 2D distance formula on lat/lon coordinates

**Algorithm**:
```c
centroid = (Σ lat_i / n, Σ lon_i / n)
variance = Σ [sqrt((lat_i - centroid_lat)² + (lon_i - centroid_lon)²)]² / n
distribution = min(1.0, sqrt(variance / 1000.0))
```

**Key Insight**: **Penalizes clustering** - nodes whose neighbors are tightly grouped score low, while nodes with spatially distributed neighbors (ideal clusterhead position) score high. This implements Task 18's geographic centrality requirement.

**Status**: ✅ **PASSED** - Sophisticated spatial analysis with appropriate normalization

---

### ✅ `ble_election_update_metrics(state)`

**Location**: [ble_election.c:232-252](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_election.c#L232-L252)

**Design Choices**:
- **Refresh-all pattern**: Recalculates every metric in single function call
- **Connection:noise ratio formula**:
  ```c
  cn_ratio = direct_connections / (1.0 + crowding_factor)
  ```
  - Denominator offset by 1.0 prevents division by zero
  - Higher crowding → lower ratio (penalizes noisy environments)
  - Combines connectivity benefit with crowding penalty
- **Forwarding success rate**:
  ```c
  success_rate = messages_forwarded / messages_received
  ```
  - Returns 0.0 if no messages received (safe default)
- **All derived metrics**: No stored intermediate state, always fresh values
- **Calls helper functions**: `count_direct_connections()`, `calculate_crowding()`, `calculate_geographic_distribution()`
- **Null safety**: Returns early on NULL pointer

**Key Insight**: **Aggregation function** combining lower-level measurements into high-level connectivity metrics. The cn_ratio formula elegantly captures tradeoff: many connections are good, but only if environment isn't too noisy.

**Status**: ✅ **PASSED** - Clean aggregation layer with well-designed composite metrics

---

### ✅ `ble_election_set_score_weights(state, weights)`

**Location**: [ble_election.c:254-262](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_election.c#L254-L262)

**Design Choices**:
- **Optional parameter**: `NULL` weights resets to defaults (0.25 each)
- **Struct copy**: Copies entire `ble_score_weights_t` (4 doubles)
- **No validation**: Accepts any weights, allows experimentation
- **Default weights**: Equal (0.25) for all four factors
- **Tuning flexibility**: Enables scenario-specific optimization without recompilation

**Four Factors**:
1. `direct_connections` - Neighbor count weight
2. `connection_noise_ratio` - Connectivity vs crowding weight
3. `geographic_distribution` - Spatial spread weight
4. `forwarding_success_rate` - Reliability weight

**Key Insight**: Provides **runtime tuning** for different deployment scenarios (e.g., high-traffic networks might prioritize forwarding success, sparse networks might prioritize geographic distribution).

**Status**: ✅ **PASSED** - Flexible configuration mechanism enabling scenario-specific optimization

---

### ✅ `ble_election_calculate_candidacy_score(state)`

**Location**: [ble_election.c:264-281](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_election.c#L264-L281)

**Design Choices**:
- **Normalized inputs**: All metrics pre-normalized to [0.0, 1.0] range
- **Direct connections normalization**: Divides by 30.0 (expected max neighbors)
- **Connection:noise normalization**: Divides by 10.0 (expected max ratio)
- **Weighted sum formula**:
  ```c
  score = w₁·(direct/30) + w₂·(cn_ratio/10) + w₃·geo_dist + w₄·fwd_rate
  ```
- **Clamped output**: Final score clamped to [0.0, 1.0]
- **No threshold enforcement**: Pure scoring function, separate from candidacy decision

**Normalization Constants**:
- **30 connections**: Empirically tuned for typical BLE mesh density (150 max neighbors, ~20% direct)
- **10.0 cn_ratio**: Expected max ratio in realistic scenarios

**Key Insight**: Normalization constants are **empirically tuned** based on expected network densities. They ensure no single factor dominates regardless of absolute magnitudes.

**Status**: ✅ **PASSED** - Well-balanced multi-factor scoring with appropriate normalization

---

### ✅ `ble_election_should_become_candidate(state)`

**Location**: [ble_election.c:283-307](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_election.c#L283-L307)

**Design Choices**:
- **Four threshold gates** (AND logic):
  1. `direct_connections >= min_neighbors` (default 10)
  2. `cn_ratio >= min_cn_ratio` (default 5.0)
  3. `geo_distribution >= min_geo_dist` (default 0.3)
  4. `messages_forwarded > 0` (at least one successful forward)
- **Metrics refresh first**: Always calls `update_metrics()` before evaluation
- **Score calculation**: Computes and stores `candidacy_score` even if thresholds fail
- **State update**: Sets `state->is_candidate` boolean flag
- **Boolean return**: Returns candidacy decision for immediate use

**Decision Logic**: Score can be high, but candidacy still fails if **any** individual metric is deficient.

**Key Insight**: **Multi-gate filter** prevents "well-rounded weak candidates". Ensures all critical capabilities present, not just high average. This implements Task 17's comprehensive candidacy evaluation.

**Status**: ✅ **PASSED** - Robust multi-criteria decision logic with appropriate safety gates

---

### ✅ `ble_election_set_thresholds(state, min_neighbors, min_cn_ratio, min_geo_dist)`

**Location**: [ble_election.c:309-319](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_election.c#L309-L319)

**Design Choices**:
- **Three configurable thresholds**: Direct neighbors, cn_ratio, geographic distribution
- **No validation**: Accepts any values (enables extreme scenarios for testing)
- **Immediate effect**: Next `should_become_candidate()` call uses new thresholds
- **No state invalidation**: Doesn't force re-evaluation, waits for next check

**Use Cases**:
- **Adaptive behavior**: Adjust based on network phase (strict during discovery, relaxed during maintenance)
- **Density adaptation**: Lower thresholds in sparse networks, raise in dense networks
- **Testing**: Extreme values for edge case validation

**Key Insight**: Enables **runtime adaptivity** - thresholds can evolve as network conditions change.

**Status**: ✅ **PASSED** - Essential tuning mechanism for adaptive protocols

---

### ✅ `ble_election_get_neighbor(state, node_id)`

**Location**: [ble_election.c:321-333](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_election.c#L321-L333)

**Design Choices**:
- **Const correctness**: Returns `const ble_neighbor_info_t*` (read-only access)
- **Linear search**: O(n) lookup acceptable for 150-neighbor max
- **NULL return**: Indicates neighbor not found
- **No side effects**: Pure query function
- **NULL safety**: Returns NULL on invalid state

**Key Insight**: **Database accessor** with const-correctness preventing caller modification. Forces modifications through `update_neighbor()`, maintaining state integrity.

**Status**: ✅ **PASSED** - Clean accessor with appropriate const semantics

---

### ✅ `ble_election_clean_old_neighbors(state, current_time_ms, timeout_ms)`

**Location**: [ble_election.c:335-353](ns3_dev/ns3-ble-module/ble-mesh-discovery/model/protocol-core/ble_election.c#L335-L353)

**Design Choices**:
- **In-place compaction**: Overwrites stale entries, shifts remaining neighbors left
- **Backward iteration**: Prevents index issues when removing elements
- **Timeout-based expiration**: `current_time - last_seen > timeout`
- **Returns removal count**: Useful for diagnostics/logging
- **No sorting maintained**: Preserves insertion order
- **memmove for shifting**: Efficient block move of remaining neighbors

**Algorithm**:
```c
for i = neighbor_count-1 down to 0:
    if (current_time - neighbors[i].last_seen > timeout):
        memmove(&neighbors[i], &neighbors[i+1], ...)
        neighbor_count--
```

**Key Insight**: **Passive neighbor pruning** handles node failures and mobility. Backward iteration ensures correct behavior when removing multiple consecutive entries.

**Status**: ✅ **PASSED** - Efficient pruning algorithm with proper index management

---

## Section 3 Completion Status

### ✅ Section 3.1 - `ble_broadcast_timing.{h,c}` - Stochastic Broadcast Timing
**Status**: ✅ **COMPLETE** - All 14 functions reviewed and passed

**Functions Reviewed**:
1. `ble_broadcast_timing_init()`
2. `ble_broadcast_timing_set_seed()`
3. `ble_broadcast_timing_advance_slot()`
4. `ble_broadcast_timing_should_broadcast()`
5. `ble_broadcast_timing_should_listen()`
6. `ble_broadcast_timing_record_success()`
7. `ble_broadcast_timing_record_failure()`
8. `ble_broadcast_timing_reset_retry()`
9. `ble_broadcast_timing_get_success_rate()`
10. `ble_broadcast_timing_get_current_slot()`
11. `ble_broadcast_timing_get_actual_listen_ratio()`
12. `ble_broadcast_timing_rand()` + `ble_broadcast_timing_rand_double()`
13. `ble_broadcast_timing_set_crowding()`
14. `ble_broadcast_timing_get_max_broadcast_slots()`

---

### ✅ Section 3.2 - `ble_election.{h,c}` - Clusterhead Election
**Status**: ✅ **COMPLETE** - All 13 functions reviewed and passed (includes circular buffer upgrade)

**Functions Reviewed**:
1. `ble_election_init()`
2. `ble_election_update_neighbor()`
3. `ble_election_add_rssi_sample()` ⚠️ **UPGRADED** - Circular buffer with timestamp eviction
4. `ble_election_calculate_crowding()`
5. `ble_election_count_direct_connections()`
6. `ble_election_calculate_geographic_distribution()`
7. `ble_election_update_metrics()`
8. `ble_election_set_score_weights()`
9. `ble_election_calculate_candidacy_score()`
10. `ble_election_should_become_candidate()`
11. `ble_election_set_thresholds()`
12. `ble_election_get_neighbor()`
13. `ble_election_clean_old_neighbors()`

**Key Upgrade**: RSSI circular buffer upgraded to proper head/tail implementation with dual eviction strategy (count-based + time-based) for mobile node support. See [RSSI_CIRCULAR_BUFFER_UPGRADE.md](RSSI_CIRCULAR_BUFFER_UPGRADE.md) for details.

---

### ℹ️ Section 3.3 - `ble_discovery_packet.{h,c}` Enhancements
**Status**: ⏭️ **SKIPPED** - Listed as enhancements rather than discrete functions

**Changes**:
- Added `ble_score_weights_t` struct + `BLE_DEFAULT_SCORE_WEIGHTS`
- Expanded `ble_discovery_packet_t` with `is_clusterhead_message` flag
- New `ble_pdsf_history_t` structures for PDSF prediction tracking
- Enhanced score calculation with weighted metrics
- Last Π storage/serialization for rebroadcast continuity

**Rationale**: These are structural enhancements and data structure additions rather than standalone functions requiring design analysis. The scoring functionality was already reviewed in section 3.2 (`ble_election_calculate_candidacy_score`).

---

### ℹ️ Section 3.4 - `ble_mesh_node.c` Minor Tweaks
**Status**: ⏭️ **SKIPPED** - Listed as minor state field additions

**Changes**:
- New state fields for noise tracking
- Candidate-heard cycle tracking
- Support for dynamic threshold adaptation

**Rationale**: These are state field additions supporting the election module reviewed in section 3.2, not standalone functions requiring separate analysis.

---

## ✅ **SECTION 3 COMPLETE**

**Core Functions Reviewed**: 27 (14 + 13)
**All Functions Passed**: ✅
**Enhancements Acknowledged**: 2 sections (3.3, 3.4)

---

## 📋 Next: Section 4 - C++ Model Wrappers

Section 4 covers NS-3 wrapper classes that bridge the reviewed C modules to the NS-3 simulator environment:

### 4.1 `model/ble-broadcast-timing.{h,cc}` - NS-3 Broadcast Timing Wrapper
Wraps the C broadcast timing module reviewed in section 3.1. Provides:
- Public API mirroring C functions (`Initialize`, `SetSeed`, `AdvanceSlot`, `RecordSuccess/Failure`)
- NS-3 integration (stats, logging, Time/Simulator compatibility)
- Type conversion between NS-3 types and C structures

### 4.2 `model/ble-election.{h,cc}` - NS-3 Election Wrapper
Wraps the C election module reviewed in section 3.2. Provides:
- Complete method set: `UpdateNeighbor`, `AddRssiSample`, `CalculateCrowding`, `CountDirectConnections`
- Metrics and score calculation: `CalculateGeographicDistribution`, `UpdateMetrics`, `CalculateCandidacyScore`
- Candidacy decision: `ShouldBecomeCandidate`
- Database access: `GetMetrics`, `GetNeighbors`, `GetNeighbor`, `CleanOldNeighbors`
- Configuration: `SetThresholds`, `SetDirectRssiThreshold`, `SetScoreWeights`
- Forwarding hooks: `RecordMessageForwarded`, `RecordMessageReceived`
- Type conversion: NS-3 `Vector` ↔ `ble_gps_location_t`, `Time` ↔ milliseconds

### 4.3 `model/ble-election-header.{h,cc}` - Specialized Election Header
Specialized `BleDiscoveryHeaderWrapper` subclass for election announcements:
- Enforces election-announcement semantics
- Overrides: `GetSerializedSize`, `Serialize`, `Deserialize`
- Ensures clusterhead flags are properly set

### 4.4 Updates to Existing Wrappers
- **`BleMeshNode`**: Now holds `Ptr<BleElection>` (`m_election`), forwards candidate methods, records forwarding success
- **`BleDiscoveryHeaderWrapper`**: Updated for `is_clusterhead_message`, election fields (direct_connections, class_id, pdsf history)

**Purpose of Section 4**: These wrappers enable NS-3 simulation of the protocol logic reviewed in section 3, providing the glue between pure C embedded code and the NS-3 simulator framework.

---

## Review Summary

**Total Functions Reviewed**: 27
**Functions Passed**: 27
**Functions Failed**: 0

**Sections Completed**:
- ✅ Section 3.1: `ble_broadcast_timing.{h,c}` - 14 functions
- ✅ Section 3.2: `ble_election.{h,c}` - 13 functions
- ⏭️ Section 3.3: `ble_discovery_packet.{h,c}` - Enhancements acknowledged
- ⏭️ Section 3.4: `ble_mesh_node.c` - Minor tweaks acknowledged
- 📋 Section 4: C++ Model Wrappers - **READY FOR REVIEW**

**Last Updated**: 2025-11-22