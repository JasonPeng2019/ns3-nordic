# ch02: Discovery Cycle + Message Queue (C cores with NS-3 wrappers)

## Scope
This README documents only the code in this directory:
- `ble_discovery_cycle.h`
- `ble_discovery_cycle.c`
- `ble-discovery-cycle-wrapper.h`
- `ble-discovery-cycle-wrapper.cc`
- `ble_message_queue.h`
- `ble_message_queue.c`
- `ble-message-queue.h`
- `ble-message-queue.cc`

It is intended to be a complete implementation overview for this chunk.

## What This Chunk Implements
This chunk provides two protocol primitives, each in two layers:

1. Discovery-cycle state machine (4 slots per cycle)
- Pure C core: `ble_discovery_cycle.h/.c`
- NS-3 C++ wrapper/scheduler glue: `ble-discovery-cycle-wrapper.h/.cc`

2. Forwarding message queue (dedup + loop checks + TTL priority)
- Pure C core: `ble_message_queue.h/.c`
- NS-3 C++ wrapper: `ble-message-queue.h/.cc`

Design intent is portability: C files hold platform-agnostic logic; C++ files integrate with NS-3 APIs.

## Alignment to `split.md` Chunk 2 Plan
Reference: `model/chunks/split.md` (Spec `ble-mesh-discovery-v1-plan`, updated `2026-03-31`).

Overall status for this chunk implementation versus plan: **partial fit**.

What matches the plan:
- 4-slot discovery-cycle structure exists with `slot0..slot3` mapping and per-slot callbacks.
- RX queue and seen-message cache exist.
- PSF loop rejection hook exists (`ble_queue_is_in_path` -> `ble_discovery_is_in_path`).

Where this implementation deviates from chunk02 requirements:
- Missing per-node runtime state container with `BleMeshNodeState` in this chunk.
- Dedupe identity does not match required key contract:
  - Plan requires key fields that avoid mutable forwarding fields (`ttl`, trailing path growth).
  - Current code uses only `(sender_id, ttl)` via `ble_queue_generate_message_id`, which includes mutable `ttl` and omits required election/discovery identity fields.
- Dedupe expiry default is not implemented per plan:
  - Plan default: `2 * cycle_duration * initial_ttl`.
  - Current code exposes generic cleanup API (`ble_queue_clean_old_entries`) but no required default policy in this chunk.
- TTL decrement and drop-at-zero forwarding enforcement are not implemented in this chunk’s queue/cycle logic.
- Chunk02 deliverable naming/ownership in plan calls for `BleDiscoveryEngine`; this chunk currently provides `BleDiscoveryCycleWrapper` + queue wrapper primitives.
- No explicit convergence criterion artifact for the 20-node gate in this chunk.

This aligns with `split.md` status section marking Chunk 2 as **PARTIAL**.

## File-Level Dependencies

### `ble_discovery_cycle.h`
Direct includes:
- `<stdint.h>`
- `<stdbool.h>`

Provides:
- constants/macros for slot layout
- `ble_slot_type_t`
- callback typedefs
- `ble_discovery_cycle_t`
- full C API prototypes for cycle lifecycle, slot control, callbacks, metadata

### `ble_discovery_cycle.c`
Direct includes:
- `"ble_discovery_cycle.h"`
- `<string.h>`

Depends on:
- types/macros from `ble_discovery_cycle.h`
- `memset` from libc

### `ble-discovery-cycle-wrapper.h`
Direct includes:
- `"ns3/object.h"`
- `"ns3/nstime.h"`
- `"ns3/event-id.h"`
- `"ns3/callback.h"`
- `"ns3/ble_discovery_cycle.h"`

Depends on:
- NS-3 object system and callback/event/time types
- C cycle API/type (`ble_discovery_cycle_t` and C functions)

Provides:
- `ns3::BleDiscoveryCycleWrapper` class interface

### `ble-discovery-cycle-wrapper.cc`
Direct includes:
- `"ble-discovery-cycle-wrapper.h"`
- `"ns3/log.h"`
- `"ns3/simulator.h"`

Depends on:
- wrapper class declaration
- NS-3 logging and scheduler
- C cycle API functions (`ble_discovery_cycle_*`)

### `ble_message_queue.h`
Direct includes:
- `<stdint.h>`
- `<stdbool.h>`
- `<stddef.h>`
- `"ble_discovery_packet.h"`

Depends on:
- packet types/functions declared by `ble_discovery_packet.h` (outside this folder)

Provides:
- queue and seen-cache config constants
- queue/seen entry structs
- `ble_message_queue_t`
- full C API prototypes for queue operations and utilities

### `ble_message_queue.c`
Direct includes:
- `"ble_message_queue.h"`
- `<string.h>`

Depends on:
- queue types/prototypes from local header
- external function `ble_discovery_is_in_path(...)` via `ble_discovery_packet.h`
- libc `memset`/`memcpy`

### `ble-message-queue.h`
Direct includes:
- `"ns3/object.h"`
- `"ns3/packet.h"`
- `"ns3/nstime.h"`
- `"ble-discovery-header-wrapper.h"`
- `"ns3/ble_message_queue.h"` inside `extern "C"`

Depends on:
- NS-3 object/packet/time types
- discovery header wrapper class (outside this folder)
- C queue API/type (`ble_message_queue_t` and `ble_queue_*`)

Provides:
- `ns3::BleMessageQueue` class interface

### `ble-message-queue.cc`
Direct includes:
- `"ble-message-queue.h"`
- `"ns3/log.h"`
- `"ns3/simulator.h"`
- `"ns3/vector.h"` (not used)

Depends on:
- wrapper class declaration
- NS-3 logging/time/scheduler
- C queue API functions (`ble_queue_*`)
- `BleDiscoveryHeaderWrapper` conversion helpers

## High-Level Runtime Flow

### Discovery cycle flow
1. `BleDiscoveryCycleWrapper::Start()` starts C state (`ble_discovery_cycle_start`) then schedules 5 NS-3 events:
- slot 0 at `t+0`
- slot 1 at `t+1*slotDuration`
- slot 2 at `t+2*slotDuration`
- slot 3 at `t+3*slotDuration`
- cycle rollover at `t+4*slotDuration`
2. Slot events call `ExecuteSlot0()` / `ExecuteForwardingSlot(n)` and fire user callbacks.
3. Rollover event calls `ScheduleNextCycle()`, increments cycle count, calls cycle-complete callback, then schedules the next cycle.
4. `Stop()` flips C running flag and cancels all queued `EventId`s.

### Message queue flow
1. C++ wrapper `Enqueue(...)` converts wrapper header -> C packet pointer and calls `ble_queue_enqueue(...)`.
2. C queue checks:
- loop detection (`ble_queue_is_in_path`)
- dedup cache (`ble_queue_has_seen` + generated message id)
- max queue size
3. Accepted packets are stored in fixed array with computed priority (`255 - ttl`).
4. `Dequeue(...)` or `Peek(...)` finds the lowest numeric priority entry (highest priority), converts back to wrapper header.

## Detailed Function Reference

## `ble_discovery_cycle.c` functions
- `ble_discovery_cycle_init(cycle)`
  - Zero-initializes state; default slot duration = `BLE_DISCOVERY_DEFAULT_SLOT_DURATION_MS`.
- `ble_discovery_cycle_set_slot_duration(cycle, duration_ms)`
  - Fails if `cycle` is null or currently running.
- `ble_discovery_cycle_get_slot_duration(cycle)`
  - Returns `0` on null.
- `ble_discovery_cycle_get_cycle_duration(cycle)`
  - Returns `slot_duration_ms * 4`, or `0` on null.
- `ble_discovery_cycle_start(cycle)`
  - Sets `running=true`, `current_slot=0`; fails if already running/null.
- `ble_discovery_cycle_stop(cycle)`
  - Sets `running=false`.
- `ble_discovery_cycle_is_running(cycle)`
  - Null-safe false.
- `ble_discovery_cycle_get_current_slot(cycle)`
  - Null-safe `0`.
- `ble_discovery_cycle_get_slot_type(slot_number)`
  - Slot 0 => own-message; all other values => forwarding.
- `ble_discovery_cycle_is_valid_slot(slot_number)`
  - Valid iff `< BLE_DISCOVERY_NUM_SLOTS`.
- `ble_discovery_cycle_is_forwarding_slot(slot_number)`
  - Valid forwarding slots are 1..3.
- `ble_discovery_cycle_set_slot_callback(cycle, slot_number, callback)`
  - Installs per-slot callback if slot is valid.
- `ble_discovery_cycle_set_complete_callback(cycle, callback)`
  - Installs cycle-complete callback.
- `ble_discovery_cycle_set_user_data(cycle, user_data)`
  - Stores callback context pointer.
- `ble_discovery_cycle_execute_slot(cycle)`
  - If running, executes callback for `current_slot` (if non-null); does not advance slot.
- `ble_discovery_cycle_advance_slot(cycle)`
  - Increments slot; on wrap to 0 increments `cycle_count` and calls cycle-complete callback.
- `ble_discovery_cycle_get_slot_offset(cycle, slot_number)`
  - Returns `slot_number * slot_duration_ms` for valid slot; otherwise 0.
- `ble_discovery_cycle_get_cycle_count(cycle)`
  - Null-safe 0.
- `ble_discovery_cycle_reset_count(cycle)`
  - Sets `cycle_count=0`.
- `ble_discovery_cycle_slot_type_name(slot_type)`
  - Returns `"OWN_MESSAGE"`, `"FORWARDING"`, or `"UNKNOWN"`.
- `ble_discovery_cycle_slot_name(slot_number)`
  - Returns human-readable slot names for 0..3; else `"Invalid Slot"`.

Internal call dependencies in this file:
- `set_slot_callback` -> `is_valid_slot`
- `get_slot_offset` -> `is_valid_slot`

## `BleDiscoveryCycleWrapper` methods (`ble-discovery-cycle-wrapper.cc`)
Public:
- `GetTypeId()`
  - Registers NS-3 type `ns3::BleDiscoveryCycleWrapper`.
- `BleDiscoveryCycleWrapper()`
  - Calls `ble_discovery_cycle_init(&m_cycle)`.
- `~BleDiscoveryCycleWrapper()`
  - Calls `Stop()`.
- `Start()`
  - Calls `ble_discovery_cycle_start`; then `ScheduleAllSlots()` if successful.
- `Stop()`
  - Calls `ble_discovery_cycle_stop` and `CancelAllEvents()` if running.
- `IsRunning() const`
  - Calls `ble_discovery_cycle_is_running`.
- `SetSlotDuration(Time)`
  - Converts to ms and calls `ble_discovery_cycle_set_slot_duration`.
- `GetSlotDuration() const`
  - Wraps `ble_discovery_cycle_get_slot_duration`.
- `GetCycleDuration() const`
  - Wraps `ble_discovery_cycle_get_cycle_duration`.
- `GetCurrentSlot() const`
  - Wraps `ble_discovery_cycle_get_current_slot`.
- `GetCycleCount() const`
  - Wraps `ble_discovery_cycle_get_cycle_count`.
- `SetSlot0Callback(Callback<void>)`
  - Stores callback.
- `SetForwardingSlotCallback(slotNumber, Callback<void>)`
  - Validates slot with `ble_discovery_cycle_is_forwarding_slot`; stores slot-specific callback.
- `SetCycleCompleteCallback(Callback<void>)`
  - Stores callback.

Private:
- `ExecuteSlot0()`
  - Sets `m_cycle.current_slot=0`; fires slot0 callback.
- `ExecuteForwardingSlot(slotNumber)`
  - Sets current slot to argument; dispatches callback for slot 1/2/3.
- `ScheduleNextCycle()`
  - If running: increments `m_cycle.cycle_count`, invokes cycle-complete callback, resets slot to 0, re-calls `ScheduleAllSlots()`.
- `ScheduleAllSlots()`
  - Schedules slot and cycle events with NS-3 `Simulator::Schedule`.
- `CancelAllEvents()`
  - Cancels all 5 stored event IDs.

Internal call dependencies in this file:
- `Start` -> `ScheduleAllSlots`
- `Stop` -> `CancelAllEvents`
- `ScheduleNextCycle` -> `ScheduleAllSlots`
- `ScheduleAllSlots` schedules `ExecuteSlot0`, `ExecuteForwardingSlot`, `ScheduleNextCycle`

## `ble_message_queue.c` functions
- `ble_queue_init(queue)`
  - Zeroes entire struct, initializes counters.
- `ble_queue_enqueue(queue, packet, node_id, current_time_ms)`
  - Validates input; rejects loops, duplicates, and overflow; inserts into first free slot; records seen-cache entry if capacity allows; increments stats.
- `ble_queue_dequeue(queue, packet)`
  - Selects highest priority message (lowest `priority` value), copies out, invalidates slot, updates stats.
- `ble_queue_peek(queue, packet)`
  - Same selection logic as dequeue without removal.
- `ble_queue_is_empty(queue)`
  - Null-safe true.
- `ble_queue_get_size(queue)`
  - Null-safe 0.
- `ble_queue_clear(queue)`
  - Invalidates all queue and seen-cache entries; resets `size` and `seen_count`.
- `ble_queue_has_seen(queue, sender_id, message_id)`
  - Linear scan on seen cache.
- `ble_queue_is_in_path(packet, node_id)`
  - Calls external `ble_discovery_is_in_path(packet, node_id)`.
- `ble_queue_generate_message_id(packet)`
  - Builds ID as `(sender_id << 32) | ttl`.
- `ble_queue_calculate_priority(packet)`
  - `255 - ttl` (`ttl==0` maps to 255).
- `ble_queue_clean_old_entries(queue, current_time_ms, max_age_ms)`
  - Invalidates aged seen-cache entries and decrements `seen_count`.
- `ble_queue_get_statistics(queue, ...)`
  - Copies statistics to non-null output pointers.

Internal call dependencies in this file:
- `enqueue` -> `is_in_path`, `generate_message_id`, `has_seen`, `calculate_priority`
- `is_in_path` -> external `ble_discovery_is_in_path`

## `BleMessageQueue` methods (`ble-message-queue.cc`)
Public:
- `GetTypeId()`
  - Registers NS-3 type `ns3::BleMessageQueue`.
- `BleMessageQueue()`
  - Calls `ble_queue_init(&m_queue)`.
- `~BleMessageQueue()`
  - Destructor only logs.
- `Enqueue(packet, header, nodeId)`
  - Converts header to C packet pointer (election or base), gets simulation time in ms, calls `ble_queue_enqueue`.
- `Dequeue(header)`
  - Calls `ble_queue_dequeue`; converts C packet -> `BleDiscoveryHeaderWrapper`; returns newly created empty `Packet` or `nullptr`.
- `Peek(header) const`
  - Calls `ble_queue_peek`; converts C packet -> wrapper header; no removal.
- `IsEmpty() const`
  - Calls `ble_queue_is_empty`.
- `GetSize() const`
  - Calls `ble_queue_get_size`.
- `Clear()`
  - Calls `ble_queue_clear`.
- `CleanOldEntries(maxAge)`
  - Converts NS-3 time to ms and calls `ble_queue_clean_old_entries`.
- `GetStatistics(...) const`
  - Calls `ble_queue_get_statistics`.

Internal call dependencies in this file:
- `Enqueue` -> `ble_queue_enqueue`
- `Dequeue` -> `ble_queue_dequeue`
- `Peek` -> `ble_queue_peek`
- `IsEmpty` -> `ble_queue_is_empty`
- `GetSize` -> `ble_queue_get_size`
- `Clear` -> `ble_queue_clear`
- `CleanOldEntries` -> `ble_queue_clean_old_entries`
- `GetStatistics` -> `ble_queue_get_statistics`

## Highlights
- Strong portability split: protocol logic is plain C and avoids NS-3/C++ STL dependencies.
- Deterministic memory model in queue: fixed-size arrays, no dynamic allocation in C core.
- Null-safe API style in C layer (many functions fail safe instead of crashing).
- Wrappers are relatively thin and keep simulation-time conversion localized.

## Potential Issues / Risks

1. **Queue dedup hash is very weak** (`ble_queue_generate_message_id`)
- Current ID is only `sender_id` + `ttl`.
- Different packets from same sender with same TTL collide and may be falsely dropped as duplicates.

2. **Queue wrapper does not preserve payload data** (`BleMessageQueue::Dequeue`)
- Returned `Ptr<Packet>` is always an empty newly-created packet.
- Only header metadata is effectively round-tripped.

3. **Cycle wrapper duplicates C-core state transitions instead of reusing C execution helpers**
- Wrapper directly writes `m_cycle.current_slot` and `m_cycle.cycle_count`.
- It does not call `ble_discovery_cycle_execute_slot` / `ble_discovery_cycle_advance_slot`.
- Behavior can drift between C core and C++ wrapper over time.

4. **Zero slot duration is allowed**
- `ble_discovery_cycle_set_slot_duration` accepts `duration_ms == 0`.
- Wrapper scheduling with zero duration can bunch all events at same simulation timestamp and create event churn.

5. **Seen-cache saturation degrades dedup correctness**
- When `seen_count == BLE_SEEN_CACHE_SIZE`, enqueue still accepts packets but stops recording new seen entries.
- Duplicate suppression quality drops for newly arriving traffic.

6. **Ambiguous return semantics in `ble_discovery_cycle_advance_slot`**
- Returns `0` both when wrapping a valid cycle and when called with null/not-running cycle.

7. **`ble_discovery_cycle_get_slot_type` treats invalid slots as forwarding**
- Any non-zero slot number maps to forwarding, including out-of-range values.

8. **Naming/include consistency risk**
- Both hyphenated and underscored queue files exist (`ble-message-queue.*` vs `ble_message_queue.*`), and wrappers include headers via `ns3/...` path.
- This is workable but easy to miswire during refactors/build-system changes.

9. **Minor code hygiene**
- `removed_count` in `ble_queue_clean_old_entries` is unused.
- `ns3/vector.h` is included but unused in `ble-message-queue.cc`.

## Prerequisites (Embedded Deployment Requirements)
These are strict requirements for using this chunk as an embedded-target protocol core and for meeting chunk02 intent.

1. Protocol identity and dedupe contract (mandatory)
- Replace current dedupe ID logic with a stable key that excludes mutable forwarding fields.
- Required default key fields: `message_type`, `sender_id`, `is_clusterhead_message`, `class_id`, `hash`, `is_renouncement`, and `path[0]` when present.
- Dedupe must be deterministic and byte-stable across builds on the same target architecture.

2. TTL and forwarding invariants (mandatory)
- TTL must be decremented exactly once per forward decision.
- Messages with `ttl == 0` must never be forwarded.
- Slot budget invariant must hold: at most `1` own-message transmission and at most `3` forwarded transmissions per cycle.

3. Loop prevention (mandatory)
- PSF loop check must run before enqueue/forward.
- Any packet containing local node ID in path must be dropped and counted.

4. Cache expiry policy (mandatory)
- Default seen-cache expiry must be `2 * cycle_duration * initial_ttl` unless explicitly overridden by config.
- Expiry policy must be test-covered for hit/miss and stale-entry eviction behavior.

5. Determinism and time base (mandatory)
- Use a monotonic time source for all queue/cycle timing.
- Handle timer rollover explicitly (32-bit ms wraps in long-running embedded uptime).
- Fixed seed + fixed event timeline must produce deterministic replay (`D0`) on the same architecture.

6. Concurrency model and thread safety (mandatory)
- C core must document and enforce one of:
  - single-threaded ownership contract, or
  - explicit lock/critical-section protection for queue/cycle mutation.
- If called from ISR + task contexts, shared state access must be atomic or guarded; plain boolean state flags are insufficient.

7. Memory and allocation policy (industry-standard requirement)
- No unbounded dynamic allocation in runtime hot path.
- Fixed-capacity buffers must be sized by configuration profile and validated at boot.
- Overflow behavior must be explicit, non-crashing, and telemetry-visible.

8. Serialization and ABI portability (industry-standard requirement)
- Use fixed-width integer types only (`uint8_t`, `uint16_t`, etc.).
- Define explicit wire endianness and serialize/deserialize through tested routines (no raw struct layout assumptions).
- Validate packet bounds before read/write to prevent out-of-bounds access.

9. Toolchain quality gates (industry-standard requirement)
- Build with warnings-as-errors for C and C++ (`-Wall -Wextra -Werror` equivalent).
- Run static analysis on C core (at minimum `clang-tidy`/`cppcheck`; MISRA C:2012 checks recommended for production firmware).
- Enforce unit tests for slot mapping, dedupe key correctness, loop rejection, TTL handling, and expiry.

10. Observability and failure accounting (mandatory)
- Maintain counters for: enqueue success, dequeues, duplicates, loops, ttl-zero drops, capacity drops, stale-cache evictions.
- Counters must be readable without disturbing runtime behavior.

## Practical Integration Notes
- The discovery-cycle C core is callback-driven and scheduler-agnostic; in this chunk, NS-3 scheduling is implemented only in `BleDiscoveryCycleWrapper`.
- The queue C core expects `current_time_ms` from caller; wrappers pass `Simulator::Now().GetMilliSeconds()`.
- Election packets are handled by copying full `ble_election_packet_t`; non-election packets use only the base packet region.

## Quick Reference: Constants
- Discovery cycle slots: `BLE_DISCOVERY_NUM_SLOTS = 4`
- Default slot duration: `BLE_DISCOVERY_DEFAULT_SLOT_DURATION_MS = 100`
- Queue capacity: `BLE_QUEUE_MAX_SIZE = 100`
- Seen-cache capacity: `BLE_SEEN_CACHE_SIZE = 200`
