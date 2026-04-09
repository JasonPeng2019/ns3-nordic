# ch01: BLE Discovery + Election Packet Core and NS-3 Wrappers

## Scope

This chunk contains a pure-C packet protocol core (`ble_discovery_packet.*`) and two NS-3 C++ header wrappers (`ble-discovery-header-wrapper.*`, `ble-election-header.*`).

The C layer defines packet structures, serialization/deserialization, PDSF/election math, and hash-based slot mapping. The C++ layer adapts that core to `ns3::Header`.

## Protocol Boundary (PDF vs Chunks)

- This chunk primarily covers the wire/data-carrier layer from the PDF: packet fields, encoding/decoding, and packet-level helper math.
- The election/cluster behavior layer (candidate policy, conflict lifecycle, convergence behavior, assignment/path policy) is implemented in later chunks and shared runtime modules.
- In practical terms: `ch01` owns field carriage + codec behavior; higher-level protocol decisions are chunk4+ concerns.

## File Inventory and Dependencies

### `ble_discovery_packet.h`

- Purpose: Public protocol API and wire-model types for C and C++ callers.
- Includes:
  - `<stdint.h>`: fixed-width integer types.
  - `<stdbool.h>`: `bool` in C.
  - `<string.h>`: needed by implementation for `memcpy`/`memset` via this header inclusion chain.
- Defines:
  - Constants: `BLE_DISCOVERY_MAX_PATH_LENGTH`, `BLE_DISCOVERY_DEFAULT_TTL`, `BLE_DISCOVERY_MAX_CLUSTER_SIZE`, `BLE_PDSF_MAX_HOPS`, `BLE_DISCOVERY_MIN_FRAME_MS`.
  - Types: `ble_message_type_t`, `ble_gps_location_t`, `ble_discovery_packet_t`, `ble_pdsf_history_t`, `ble_election_data_t`, `ble_election_packet_t`.
  - All exported C functions for init, path/TTL/GPS, size, serialize/deserialize, PDSF, score, hash/slot mapping.

### `ble_discovery_packet.c`

- Purpose: Pure-C implementation of protocol logic and binary encoding.
- Includes:
  - `"ble_discovery_packet.h"`: own API/types.
  - `<stdlib.h>`: provides `size_t` used in hash-mix loop.
  - `<limits.h>`: `UINT32_MAX` for saturation logic.
- Internal-only helpers:
  - `write_u8`, `write_u16`, `write_u32`, `write_double`
  - `read_u8`, `read_u16`, `read_u32`, `read_double`
  - `clamp_unit` (declared unused)
- Implements all exported C functions.

### `ble-discovery-header-wrapper.h`

- Purpose: NS-3 `Header` wrapper around C packet core.
- Includes:
  - `"ns3/header.h"`: base class + `Buffer::Iterator` + `TypeId` context.
  - `"ns3/vector.h"`: GPS bridge to/from `ns3::Vector`.
  - `"ns3/ble_discovery_packet.h"`: C core API in ns-3 include layout.
  - `<vector>`: C++ convenience return types.
- Defines class `ns3::BleDiscoveryHeaderWrapper` with discovery + election convenience APIs and direct C-struct accessors.

### `ble-discovery-header-wrapper.cc`

- Purpose: Implementation of `BleDiscoveryHeaderWrapper`.
- Includes:
  - `"ble-discovery-header-wrapper.h"`
  - `"ns3/log.h"`: `NS_LOG_*` macros.
  - `"ns3/assert.h"`: `NS_ABORT_MSG_IF` runtime guards.
- Depends on C APIs (`ble_*`) for almost all stateful operations.

### `ble-election-header.h`

- Purpose: Specialized wrapper type for election announcements only.
- Includes:
  - `"ble-discovery-header-wrapper.h"`
- Defines class `ns3::BleElectionHeader : public BleDiscoveryHeaderWrapper`.

### `ble-election-header.cc`

- Purpose: Implementation of `BleElectionHeader` constraints.
- Includes:
  - `"ble-election-header.h"`
  - `"ns3/log.h"`
  - `"ns3/assert.h"`
- Adds election-only runtime assertions around base wrapper serialize/deserialize.

## Data Model and Wire Layout

### Core packet model

- `ble_discovery_packet_t` fields:
  - `message_type`, `is_clusterhead_message`, `sender_id`, `ttl`
  - Path-so-far: `path_length` + `path[]`
  - GPS: `gps_available` + `gps_location`
- `ble_election_packet_t` = `base` discovery packet + `election` extension:
  - `class_id`, `direct_connections`, `pdsf`, `last_pi`, `score`, `hash`, `pdsf_history`, `is_renouncement`

### PDF Term to Code Field Mapping

| PDF term | ch01 field(s) | Notes |
|---|---|---|
| `ID` | `sender_id` | Sender/origin identifier carried on wire |
| `TTL` | `ttl` | Hop budget, decremented by forwarding logic outside this chunk |
| `PSF` / Path So Far | `path_length`, `path[]` | Ordered node ID sequence |
| `LHGPS` / GPS availability + coordinates | `gps_available`, `gps_location.{x,y,z}` | Coordinates present only when `gps_available=1` |
| `Class ID` | `election.class_id` | Election/class identifier |
| `PDSF` | `election.pdsf` | Running estimate; helper state includes `last_pi` + `pdsf_history` |
| `Score` | `election.score` | Carrier field; policy semantics are finalized in later chunks |
| `Hash` / `h(ID)` | `election.hash` | Slot/hash identifier |
| Renouncement flag | `election.is_renouncement` | Encoded in election extension flags byte |

### Discovery wire format (in order)

1. `message_type` (1 byte)
2. `is_clusterhead_message` (1 byte)
3. `sender_id` (4 bytes)
4. `ttl` (1 byte)
5. `path_length` (2 bytes)
6. `path[i]` (`path_length * 4` bytes)
7. `gps_available` (1 byte)
8. If GPS available: `x`, `y`, `z` as 3 IEEE-754 doubles (24 bytes)

Encoding notes:

- Integer fields are serialized in network byte order (big-endian).
- `double` values are serialized by copying the host 64-bit bit pattern and emitting two big-endian `uint32` words.
- This relies on the platform using 64-bit IEEE-754 `double`; it is a host-bit-pattern carriage scheme, not a canonical cross-platform float wire primitive.
- This is deterministic on the same host architecture (`D0` scope), but cross-architecture float-bit identity is not guaranteed by this encoding approach alone.

Semantics note:

- `message_type` and `is_clusterhead_message` are distinct wire fields with overlapping intent.
- Keeping both is part of the current wire contract, but readers should treat `message_type` as the canonical packet-kind discriminator to avoid mode-drift bugs.

### Election wire format

- Discovery base payload above, then extension:

1. `flags` (1 byte; bit0 = `is_renouncement`)
2. `class_id` (2 bytes)
3. `direct_connections` (4 bytes)
4. `pdsf` (4 bytes)
5. `last_pi` (4 bytes)
6. `score` (8 bytes)
7. `hash` (4 bytes)
8. `pdsf_history.hop_count` (2 bytes)
9. `pdsf_history.direct_counts[i]` (`hop_count * 4` bytes)

## Function Reference

## `ble_discovery_packet.c` internal helpers

- `write_u8(uint8_t **buf, uint8_t value)`
  - Dependency: raw pointer write/increment.
  - Used by: both discovery/election serializers.
- `write_u16(uint8_t **buf, uint16_t value)`
  - Dependency: manual big-endian encoding.
  - Used by: serializers for lengths/class IDs.
- `write_u32(uint8_t **buf, uint32_t value)`
  - Dependency: manual big-endian encoding.
  - Used by: serializers and `write_double`.
- `write_double(uint8_t **buf, double value)`
  - Dependency: `memcpy`, `write_u32`.
  - Used by: GPS and score serialization.
- `read_u8(const uint8_t **buf)`
  - Dependency: raw pointer read/increment.
  - Used by: both deserializers.
- `read_u16(const uint8_t **buf)`
  - Dependency: manual big-endian decoding.
  - Used by: deserializers.
- `read_u32(const uint8_t **buf)`
  - Dependency: manual big-endian decoding.
  - Used by: deserializers and `read_double`.
- `read_double(const uint8_t **buf)`
  - Dependency: `read_u32`, `memcpy`.
  - Used by: GPS/score deserialization.
- `clamp_unit(double value)`
  - Dependency: none.
  - Status: unused helper (`__attribute__((unused))`).

## Exported C API (`ble_discovery_packet.h/.c`)

### Initialization

- `ble_discovery_packet_init(ble_discovery_packet_t *packet)`
  - Sets discovery defaults (`message_type=DISCOVERY`, TTL default, no GPS, empty path).
- `ble_election_packet_init(ble_election_packet_t *packet)`
  - Calls discovery init for `base`, sets election defaults (`message_type=ELECTION_ANNOUNCEMENT`, clusterhead flag true, `last_pi=1`, zeroed history).
  - Dependency: `ble_discovery_packet_init`, `ble_election_pdsf_history_reset`.

### TTL and path

- `ble_discovery_decrement_ttl(ble_discovery_packet_t *packet)`
  - Decrements TTL if nonzero.
- `ble_discovery_add_to_path(ble_discovery_packet_t *packet, uint32_t node_id)`
  - Appends node ID if `path_length < BLE_DISCOVERY_MAX_PATH_LENGTH`.
- `ble_discovery_is_in_path(const ble_discovery_packet_t *packet, uint32_t node_id)`
  - Linear lookup for loop detection.

### GPS

- `ble_discovery_set_gps(ble_discovery_packet_t *packet, double x, double y, double z)`
  - Writes coordinates and marks GPS available.

### Size and binary codec

- `ble_discovery_get_size(const ble_discovery_packet_t *packet)`
  - Computes dynamic discovery serialized size.
- `ble_election_get_size(const ble_election_packet_t *packet)`
  - Base discovery size + election extension + variable hop-history bytes.
  - Dependency: `ble_discovery_get_size`.
- `ble_discovery_serialize(const ble_discovery_packet_t *packet, uint8_t *buffer, uint32_t buffer_size)`
  - Encodes discovery packet to network-order binary.
  - Dependencies: `ble_discovery_get_size`, `write_u*`, `write_double`.
- `ble_discovery_deserialize(ble_discovery_packet_t *packet, const uint8_t *buffer, uint32_t buffer_size)`
  - Decodes discovery payload from binary.
  - Dependencies: `read_u*`, `read_double`.
- `ble_election_serialize(const ble_election_packet_t *packet, uint8_t *buffer, uint32_t buffer_size)`
  - Encodes election packet (base discovery + extension).
  - Dependencies: `ble_election_get_size`, `ble_discovery_serialize`, `write_u*`, `write_double`.
- `ble_election_deserialize(ble_election_packet_t *packet, const uint8_t *buffer, uint32_t buffer_size)`
  - Decodes election packet and validates `hop_count <= BLE_PDSF_MAX_HOPS`.
  - Note: this range check does not by itself guarantee remaining-byte safety for the implied variable-length section.
  - Dependencies: `ble_discovery_deserialize`, `ble_election_pdsf_history_reset`, `read_u*`, `read_double`.

### PDSF/election math

- `ble_election_pdsf_history_reset(ble_pdsf_history_t *history)`
  - Zeros hop count and per-hop counts.
  - Dependency: `memset`.
- `ble_election_pdsf_history_add(ble_pdsf_history_t *history, uint32_t direct_connections)`
  - Appends one hop contribution if capacity remains.
- `ble_election_update_pdsf(ble_election_packet_t *packet, uint32_t direct_connections, uint32_t already_reached)`
  - Implements "exclude already reached devices" by clamping overlap and deriving `unique_connections = direct_connections - already_reached`.
  - Records per-hop unique contribution, updates cumulative PDSF and `last_pi`.
  - Dependencies: `ble_election_pdsf_history_add`, `ble_election_calculate_pdsf`.
- `ble_election_calculate_pdsf(uint32_t previous_pdsf, uint32_t previous_pi, uint32_t direct_neighbors, uint32_t *new_pi_out)`
  - Saturating product/sum update: `pi_term = previous_pi_or_1 * direct_neighbors`, `new_pdsf = previous_pdsf + pi_term`.
  - Dependency: `UINT32_MAX` saturation.
- `ble_election_calculate_score(uint32_t direct_connections, double noise_level)`
  - Current implementation returns `direct_connections + ((direct_connections / MAX_CLUSTER_SIZE) * (1/(noise_level+1)))`.
  - This is an interim utility formula and is not the locked weighted Chunk 4 score; it also does not match the PDF candidacy-ratio intent as a final policy.

### Hashing and slot mapping

- `ble_election_generate_hash(uint32_t node_id)`
  - FNV-1a-style hash over node ID bytes.
- `ble_hash_combine_cluster_edge(uint32_t cluster_hash, uint32_t edge_id)`
  - FNV-1a-style mix of cluster hash and edge ID.
- `ble_hash_map_to_slot(uint32_t hash, uint32_t tdma_slots, uint32_t fdma_channels, uint32_t *slot_index_out, uint32_t *channel_index_out)`
  - Maps hash to one `(slot, channel)` bucket.
- `ble_hash_next_slot_time_ms(uint64_t now_ms, uint32_t frame_ms, uint32_t tdma_slots, uint32_t slot_index)`
  - Calculates next absolute start time for the given slot index.
  - Uses `BLE_DISCOVERY_MIN_FRAME_MS` as a lower bound safety clamp for downstream slot scheduling helpers.
- `ble_hash_map_edge_slot(uint32_t cluster_hash, uint32_t edge_id, uint32_t tdma_slots, uint32_t fdma_channels, uint32_t *slot_index_out, uint32_t *channel_index_out)`
  - Convenience wrapper: combine then map.
  - Dependencies: `ble_hash_combine_cluster_edge`, `ble_hash_map_to_slot`.

## `BleDiscoveryHeaderWrapper` API (`ble-discovery-header-wrapper.h/.cc`)

### NS-3 header lifecycle

- `BleDiscoveryHeaderWrapper()`
  - Initializes discovery-mode packet via `ble_discovery_packet_init`.
- `~BleDiscoveryHeaderWrapper()`
- `static TypeId GetTypeId()`
- `TypeId GetInstanceTypeId() const`
- `void Print(std::ostream &os) const`
- `uint32_t GetSerializedSize() const`
  - Delegates to C size function based on `m_isElection`.
- `void Serialize(Buffer::Iterator start) const`
  - Allocates temp buffer, calls C serializer, writes into NS-3 buffer.
- `uint32_t Deserialize(Buffer::Iterator start)`
  - Peeks message type, reads remaining bytes into temp buffer, calls C deserializer, syncs `m_packet`/`m_election`.

### Discovery convenience methods

- `bool IsElectionMessage() const`
- `void SetClusterheadFlag(bool isClusterhead)`
- `bool HasClusterheadFlag() const`
- `void SetSenderId(uint32_t id)` / `uint32_t GetSenderId() const`
- `void SetTtl(uint8_t ttl)` / `uint8_t GetTtl() const`
- `bool DecrementTtl()`
  - Dependency: `ble_discovery_decrement_ttl`.
- `bool AddToPath(uint32_t nodeId)`
  - Dependency: `ble_discovery_add_to_path`.
- `bool IsInPath(uint32_t nodeId) const`
  - Dependency: `ble_discovery_is_in_path`.
- `std::vector<uint32_t> GetPath() const`
- `void SetGpsLocation(Vector position)`
  - Dependency: `ble_discovery_set_gps`.
- `Vector GetGpsLocation() const`
- `void SetGpsAvailable(bool available)` / `bool IsGpsAvailable() const`

### Election convenience methods

- `void SetAsElectionMessage()`
  - Converts current discovery state into initialized election packet state.
  - Dependency: `ble_election_packet_init`.
- `void SetRenouncement(bool renounce)` / `bool IsRenouncement() const`
- `void SetClassId(uint16_t classId)` / `uint16_t GetClassId() const`
- `void SetPdsf(uint32_t pdsf)` / `uint32_t GetPdsf() const`
- `void SetLastPi(uint32_t lastPi)` / `uint32_t GetLastPi() const`
- `void ResetPdsfHistory()`
  - Dependency: `ble_election_pdsf_history_reset`.
- `uint32_t UpdatePdsf(uint32_t directConnections, uint32_t alreadyReached)`
  - Dependency: `ble_election_update_pdsf`.
- `std::vector<uint32_t> GetPdsfHopHistory() const`
- `void SetScore(double score)` / `double GetScore() const`
- `void SetHash(uint32_t hash)` / `uint32_t GetHash() const`

### Direct C-struct accessors

- `const ble_discovery_packet_t& GetCPacket() const`
- `const ble_election_packet_t& GetCElectionPacket() const`
- `ble_discovery_packet_t& GetCPacketMutable()`
- `ble_election_packet_t& GetCElectionPacketMutable()`

## `BleElectionHeader` API (`ble-election-header.h/.cc`)

- `BleElectionHeader()`
  - Calls `SetAsElectionMessage()` immediately.
- `~BleElectionHeader() override`
- `static TypeId GetTypeId()`
- `TypeId GetInstanceTypeId() const override`
- `uint32_t GetSerializedSize() const override`
  - Asserts election mode, then delegates to base wrapper.
- `void Serialize(Buffer::Iterator start) const override`
  - Asserts election mode, delegates to base wrapper.
- `uint32_t Deserialize(Buffer::Iterator start) override`
  - Delegates to base wrapper, then asserts packet remained election type.

## Cross-File Call Dependencies

- `ble-discovery-header-wrapper.cc` -> `ble_discovery_packet.c`
  - Calls: init, size, serialize/deserialize, TTL/path/GPS, PDSF history/update.
- `ble-election-header.cc` -> `ble-discovery-header-wrapper.cc`
  - Calls base wrapper methods and adds assertion constraints.
- C core (`ble_discovery_packet.c`) is self-contained and does not depend on NS-3.

## Important Implementation Highlights

- Clean layering: protocol core is C-only and portable; NS-3 code is a thin bridge.
- Binary format is explicit: integer fields use manual big-endian encoding, and `double` fields use host-bit-pattern carriage via big-endian word writes.
- Election packets extend discovery packets without duplicating codec logic.
- PDSF handling includes overflow saturation to `UINT32_MAX`.
- Slotting utilities support deterministic FDMA/TDMA mapping from hash inputs.

## Chunk 1 Fit Against `split.md`

Assessment basis: `ns3_dev/ns3-ble-module/ble-mesh-discovery/model/chunks/split.md` (Chunk 1 requirements) compared only to files in this `ch01` folder.

### What fits Chunk 1

- Canonical wire fields in Section 2 are implemented in the C structs and codecs.
- Discovery/election serialization and deserialization APIs exist and are integrated into NS-3 wrappers.
- Boundary constants for path and hop-history limits are present in public headers.
- Local module path layout is consistent with the active repo target path under `ns3_dev/ns3-ble-module/ble-mesh-discovery`.

### Where this deviates from Chunk 1 plan

- Required Chunk 1 deliverable `ble-mesh-wire-v1.0.0` lock metadata (constant + test reference) is missing in this chunk.
- Defaults/tunables are hardcoded as C macros here, not routed through a centralized `BleMeshDiscoveryConfig` mapping layer.
- Deterministic replay controls required by Chunk 1 (`D0` seed/timing/RNG ownership) are not implemented in this folder.
- Chunk 1 test-plan artifacts are missing in this folder:
  - No 1,000-case randomized roundtrip tests for discovery/election.
  - No boundary/GPS/TTL unit test suite included in `ch01`.
  - No deterministic 30/30 replay evidence or reference trace artifacts.
- No explicit build-layout upstream mirror note is included in this chunk docs/code.

Overall status for Chunk 1 from this folder alone: `PARTIAL`.

### Cross-Chunk Audit Notes (2026-04-05)

- `BleMeshDiscoveryConfig` and `BleMeshNodeState` are implemented in `model/shared/ble-mesh-discovery-config.h`, not in `ch01`.
- This chunk intentionally provides shared score/hash/wire helpers consumed by later chunks (`ble_election_calculate_score` is used from `ch04` and `ch05`).
- Per-node runtime/state-machine behavior is implemented in shared `ble_mesh_node.*` and integrated by `ch05`.
- `BLE_DISCOVERY_MIN_FRAME_MS` is exposed here for shared hash/slot helper safety, but frame-cycle ownership and scheduling policy are chunk2/chunk5 runtime concerns.
- Therefore, this folder-local `PARTIAL` status remains accurate, but repo-level status for some APIs depends on `model/shared` plus later chunks.

## Chunk 1 Exit Gate Checklist (`split.md`)

Chunk 1 exit gate requires all of the following:

1. Packet tests pass 100%.
2. `D0` determinism pass rate is 30/30.
3. Contract version + schema are documented and referenced by tests.

Current folder-local status:

- No evidence in `ch01` of the required 1,000 randomized packet roundtrip test artifacts.
- No evidence in `ch01` of `D0` 30/30 replay artifacts.
- No machine-checkable `ble-mesh-wire-v1.0.0` lock/reference tied to tests in `ch01`.

## Prerequisites (Required for Embedded Deployment)

These are strict requirements for this implementation family when targeting production embedded devices.

1. Wire-contract governance is mandatory.

- Freeze and expose a machine-checkable contract/version constant (for example `ble-mesh-wire-v1.0.0`) in code.
- Reject incompatible version/schema at decode boundaries.

2. Decoder memory safety is mandatory.

- Every read step must check remaining buffer length before access.
- Invalid/truncated inputs must fail closed (no partial over-read, no undefined behavior).
- `path_length` and `hop_count` must always be range-validated before loops.

3. No dynamic heap allocation is allowed in packet hot paths.

- `Serialize`/`Deserialize` wrappers must use fixed-capacity stack or caller-provided buffers.
- Heap usage in high-rate RX/TX paths is disallowed for deterministic latency and fragmentation control.

4. Bounded memory and bounded execution must be enforced.

- All arrays and counters must have compile-time maxima and checked updates.
- Worst-case serialize/deserialize CPU and memory cost must be documented and verified.

5. Determinism controls are required.

- Fixed-seed replay path and owned RNG streams per subsystem are required.
- Any floating-point behavior affecting decisions must be deterministic on target architecture, or replaced with fixed-point.

6. Initialization correctness is required.

- All packet fields, including flags such as `is_renouncement`, must be explicitly initialized in constructors/init functions.
- Reading uninitialized state is prohibited.

7. Single-source configuration is required.

- Protocol constants used by runtime behavior must map from centralized config (`BleMeshDiscoveryConfig`) rather than scattered macros.
- Build-time defaults and runtime overrides must be traceable and logged.

8. Toolchain quality gates are required.

- Compile with `-Wall -Wextra -Werror` (or stricter project equivalent) for all targets.
- Run static analysis (`clang-tidy`, `cppcheck`, or equivalent) in CI.
- Run sanitizer-enabled host tests (`ASan`, `UBSan`) for codec and wrapper layers.

9. Protocol robustness tests are required.

- Roundtrip property tests with randomized packets.
- Boundary tests for all max/min fields.
- Fuzzing of deserialize entry points with malformed inputs.

10. Safety and interoperability rules are required.

- Use fixed-width integer types only for wire fields.
- Keep explicit network byte order handling for all multi-byte fields.
- Keep boolean wire values canonical (`0`/`1`) and validate on input.

## Potential Issues / Risks in Current Implementation

1. `ble_discovery_deserialize` and `ble_election_deserialize` do not fully bounds-check reads against `buffer_size`.

- They range-validate fields (for example `path_length`, `hop_count`), but do not validate that the implied byte spans remain within `buffer_size`.
- They still read variable sections without per-step remaining-length checks.
- Risk: malformed/truncated payload can cause out-of-bounds reads.

2. `ble_discovery_deserialize` minimum-size guard is too small.

- It checks `buffer_size < 7`, but fixed discovery header without path/GPS is 10 bytes.
- Risk: short buffers can pass early guard and still be over-read.

3. `ble_election_packet_init` does not explicitly initialize `election.is_renouncement`.

- Field may retain stale/indeterminate memory unless caller zeroes struct externally.
- Risk: serialized flags may become nondeterministic.

4. Wrapper election-state tracking can diverge from wire `message_type`.

- In `BleDiscoveryHeaderWrapper::Deserialize`, `m_isElection` is reassigned from `is_clusterhead_message` after decode, not strictly from `message_type`.
- In `SetClusterheadFlag(false)`, wrapper may leave `message_type` as election while treating object as non-election.
- Risk: inconsistent mode decisions and wrong codec path on later serialization/deserialization.

5. Wrapper path syncing in election mode bypasses C bounds helper.

- `AddToPath` appends directly into `m_election.base.path[...]` and increments `path_length` without its own capacity check.
- Risk: if wrapper internals desync, this can overflow.

6. Size/serialize functions trust structure counters.

- `path_length` and `pdsf_history.hop_count` are used directly during serialization loops.
- Risk: if mutable accessors or external code set invalid counts, serialization can read beyond array bounds.

7. Score API documentation and behavior are misaligned.

- Header comment says score is `0.0-1.0`, but implementation returns roughly `direct_connections + tiny_bonus`.
- Current score helper is also not aligned to the PDF's candidacy-ratio intent and not aligned to the locked Chunk 4 weighted score formula.
- Unused score normalizer macros (`BLE_SCORE_DIRECT_NORMALIZER`, `BLE_SCORE_CN_RATIO_NORMALIZER`) and unused `clamp_unit` suggest incomplete formula migration.

8. `ble_hash_next_slot_time_ms` can produce slot times outside nominal frame partitioning when `tdma_slots > frame_ms`.

- Because integer division can force `slot_duration` to 1 ms.
- Risk: schedule semantics may drift from expected strict frame geometry.

9. Frequent heap allocation in wrapper `Serialize`/`Deserialize`.

- New/delete buffer per call.
- Risk: avoidable overhead on high packet rates.

## Practical Reading Order

1. `ble_discovery_packet.h` (types/constants/API contract)
2. `ble_discovery_packet.c` (real wire + algorithm behavior)
3. `ble-discovery-header-wrapper.h/.cc` (NS-3 bridging and state policy)
4. `ble-election-header.h/.cc` (strict election-mode wrapper)
