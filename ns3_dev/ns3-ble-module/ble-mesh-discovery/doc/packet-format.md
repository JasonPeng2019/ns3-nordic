# BLE Mesh Discovery Packet Format

## Overview

This document describes the packet format for the BLE Mesh Discovery Protocol based on the specification by jason.peng (November 2025).

## Message Types

The protocol supports two types of messages:
1. **Discovery Messages** - Basic neighbor discovery and network topology learning
2. **Election Announcement Messages** - Clusterhead election announcements with additional fields

## Discovery Message Format

### Common Fields (Present in All Messages)

| Field | Size | Type | Description |
|-------|------|------|-------------|
| Message Type | 1 byte | uint8 | 0 = DISCOVERY, 1 = ELECTION_ANNOUNCEMENT |
| Sender ID | 4 bytes | uint32 | Unique identifier of message sender |
| TTL | 1 byte | uint8 | Time To Live - hops remaining (0-255) |
| PSF Length | 2 bytes | uint16 | Number of nodes in Path So Far |
| PSF | Variable | uint32[] | Array of node IDs (4 bytes each) |
| GPS Available | 1 byte | bool | 1 if GPS available, 0 otherwise |
| GPS X | 8 bytes | double | X coordinate (only if GPS available) |
| GPS Y | 8 bytes | double | Y coordinate (only if GPS available) |
| GPS Z | 8 bytes | double | Z coordinate (only if GPS available) |

**Total Size (Discovery):** 9 bytes + (4 * PSF_length) + (24 if GPS available)

### Example: Discovery Message with GPS and 3-hop path
```
Size: 9 + (4 * 3) + 24 = 45 bytes

[Type:1][ID:4][TTL:1][PSFLen:2][PSF1:4][PSF2:4][PSF3:4][GPS?:1][X:8][Y:8][Z:8]
```

## Election Announcement Message Format

Election announcement messages include all discovery fields PLUS:

| Field | Size | Type | Description |
|-------|------|------|-------------|
| Class ID | 2 bytes | uint16 | Clusterhead class identifier |
| PDSF | 4 bytes | uint32 | Predicted Devices So Far: t(x) = Σᵢ Πᵢ(xᵢ) |
| Score | 8 bytes | double | Clusterhead candidacy score (0.0 - 1.0) |
| Hash | 4 bytes | uint32 | FDMA/TDMA hash function h(ID) |

**Additional Size:** 18 bytes

**Total Size (Election):** 27 bytes + (4 * PSF_length) + (24 if GPS available)

### Example: Election Announcement with GPS and 3-hop path
```
Size: 27 + (4 * 3) + 24 = 63 bytes

Discovery fields (45 bytes) + Election fields (18 bytes)
```

## Field Details

### Message Type (1 byte)
- `0x00` = DISCOVERY
- `0x01` = ELECTION_ANNOUNCEMENT
- Values 2-255 reserved for future use

### Sender ID (4 bytes)
- Unique 32-bit identifier assigned to each node
- Used for routing and path tracking
- Must be unique within the network

### TTL - Time To Live (1 byte)
- Number of hops remaining before message is discontinued
- Initial value typically 5-10 for local discovery
- Decremented by 1 at each hop
- Message dropped when TTL reaches 0

### PSF - Path So Far (Variable)
- Ordered list of node IDs the message has traversed
- Used for:
  - Loop detection (don't forward if own ID in path)
  - Route discovery
  - Network topology mapping
- Length field allows up to 65,535 nodes in path
- Practical limit: ~50 nodes (200 bytes)

### LHGPS - Last Heard GPS Location (25 bytes total)
- **GPS Available Flag (1 byte):**
  - `0x01` = GPS available (coordinates follow)
  - `0x00` = GPS unavailable (coordinates omitted, saves 24 bytes)

- **Coordinates (24 bytes if available):**
  - X, Y, Z as IEEE 754 double-precision floats
  - Typically latitude, longitude, altitude
  - Used for GPS proximity filtering in forwarding decisions

### Class ID (2 bytes) - Election Only
- Identifies the clusterhead class/type
- Used for hierarchical clustering
- Allows different clusterhead priorities

### PDSF - Predicted Devices So Far (4 bytes) - Election Only
- Mathematical function: t(x) = Σᵢ Πᵢ(xᵢ)
- Estimates total devices reached by this clusterhead
- Used for cluster capacity limiting (max 150 devices)
- Calculation:
  - At each hop i, multiply direct connection counts
  - Sum across all hops
  - Excludes previously reached devices

### Score (8 bytes) - Election Only
- Clusterhead candidacy quality score (0.0 to 1.0)
- Computed from:
  - Direct connection count : noise ratio (primary)
  - Geographic distribution of neighbors
  - Successful forwarding rate
  - Energy level (future)
- Higher score = better clusterhead candidate

### Hash (4 bytes) - Election Only
- FDMA/TDMA hash function h(ID)
- Generated from clusterhead ID
- Used for:
  - Time slot assignment for cluster communication
  - Frequency slot assignment
  - Collision avoidance in cluster
- Deterministic based on ID

## Forwarding Decisions

Messages are forwarded based on three metrics:

### 1. Picky Forwarding
- Uses crowding factor to forward only a percentage of messages
- Prevents network congestion
- Higher crowding = lower forwarding probability

### 2. GPS Proximity Filtering
- Compare LHGPS of previous sender with current node location
- Don't forward if previous sender too close (configurable threshold)
- Skipped if GPS unavailable

### 3. TTL-Based Prioritization
- After applying filters 1 & 2, sort remaining messages by TTL
- Forward top 3 messages (limited by 4-slot discovery cycle)
- Higher TTL = higher priority

## Discovery Cycle

Each discovery cycle has **4 message slots**:
- **Slot 1:** Node sends its own discovery message (if needed)
- **Slots 2-4:** Forward up to 3 messages from queue (after filtering)

Typical cycle duration: 100-500ms

## Network Parameters

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| Initial TTL | 10 | 5-15 | Hops for discovery messages |
| Cluster Capacity | 150 | 100-200 | Max devices per cluster |
| Forwarding Slots | 3 | 1-3 | Slots for message relay |
| GPS Proximity Threshold | 20m | 10-50m | Min distance for forwarding |
| Crowding Threshold | 0.5 | 0.2-0.8 | Picky forwarding percentage |

## Wire Format Example

### Discovery Message (hex dump):
```
00                    // Message Type: DISCOVERY
00 00 00 2A          // Sender ID: 42
0A                    // TTL: 10
00 03                 // PSF Length: 3 nodes
00 00 00 01          // PSF[0]: Node 1
00 00 00 02          // PSF[1]: Node 2
00 00 00 03          // PSF[2]: Node 3
01                    // GPS Available: true
40 25 00 00 00 00 00 00  // X: 10.5
40 34 80 00 00 00 00 00  // Y: 20.5
40 3E 80 00 00 00 00 00  // Z: 30.5
```

## Future Extensions

Reserved space for future protocol enhancements:
- Security signatures
- Encryption flags
- Quality of Service (QoS) indicators
- Multi-channel support
- Power/energy indicators

## Implementation Notes

- All multi-byte integers use network byte order (big-endian)
- Doubles are serialized as IEEE 754 format
- Path So Far uses dynamic sizing to minimize overhead
- GPS coordinates optional to save bandwidth indoors
- Election fields only present in election messages

## References

- Protocol Specification: "Clusterhead & BLE Mesh discovery process" by jason.peng (November 2025)
- Implementation: `ble-discovery-header.{h,cc}`
- Tests: `ble-discovery-header-test.cc`
