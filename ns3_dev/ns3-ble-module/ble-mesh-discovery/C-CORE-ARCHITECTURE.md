# C Core + C++ Wrapper Architecture

## Overview

The BLE Mesh Discovery protocol is implemented in **two layers**:

1. **Pure C Protocol Core** - Portable, embeddable, no dependencies
2. **C++ NS-3 Wrapper** - Thin integration layer for NS-3 simulator

This architecture allows the protocol logic to be:
- Compiled for embedded systems (bare metal, RTOS)
- Tested independently of NS-3
- Ported to real BLE hardware
- Maintained separately from simulation code

## Architecture Diagram

```
┌─────────────────────────────────────────────────┐
│         NS-3 Application Layer                   │
│              (C++ Code)                          │
└────────────────────┬────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────┐
│    C++ Wrapper (BleDiscoveryHeaderWrapper)      │
│    - NS-3 Header interface                      │
│    - TypeId registration                        │
│    - Buffer::Iterator conversion                │
│    - Vector ↔ GPS struct conversion             │
└────────────────────┬────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────┐
│  Pure C Protocol Core (ble_discovery_packet.c)  │
│  - Packet structures                            │
│  - Serialization/deserialization                │
│  - Protocol logic                               │
│  - Election calculations                        │
│  - NO external dependencies                     │
└─────────────────────────────────────────────────┘
```

## File Structure

### Pure C Core (Portable)

**[model/protocol-core/ble_discovery_packet.h](model/protocol-core/ble_discovery_packet.h)**
- C header with packet structures
- Function prototypes
- No C++ or NS-3 dependencies
- Can be `#include`d in C or C++ code
- Uses `extern "C"` for C++ compatibility

**[model/protocol-core/ble_discovery_packet.c](model/protocol-core/ble_discovery_packet.c)**
- Pure C implementation
- Standard C library only (`stdint.h`, `stdbool.h`, `string.h`)
- Network byte order serialization
- All protocol logic

### C++ Wrapper (NS-3 Integration)

**[model/ble-discovery-header-wrapper.h](model/ble-discovery-header-wrapper.h)**
- C++ class inheriting from `ns3::Header`
- Wraps C structures internally
- Provides C++ API for NS-3 code
- Converts between NS-3 types and C types

**[model/ble-discovery-header-wrapper.cc](model/ble-discovery-header-wrapper.cc)**
- Thin wrapper implementation
- Delegates all logic to C core
- Handles NS-3 specific features:
  - `Buffer::Iterator` serialization
  - `TypeId` registration
  - Logging

### Original C++ Implementation (Reference)

**[model/ble-discovery-header.{h,cc}](model/ble-discovery-header.h)**
- Full C++ implementation (for comparison)
- Can be used if pure C++ is preferred
- Same functionality as C core + wrapper

## Usage

### Option 1: Use C Core + C++ Wrapper (Recommended)

```cpp
#include "ns3/ble-discovery-header-wrapper.h"

// Create packet
BleDiscoveryHeaderWrapper header;
header.SetSenderId(42);
header.SetTtl(10);
header.AddToPath(42);

// Set GPS
Vector gps(37.7749, -122.4194, 50.0);
header.SetGpsLocation(gps);
header.SetGpsAvailable(true);

// Serialize to NS-3 packet
Ptr<Packet> packet = Create<Packet>();
packet->AddHeader(header);
```

### Option 2: Use Pure C Core Directly

```c
#include "ble_discovery_packet.h"

// Create packet
ble_discovery_packet_t packet;
ble_discovery_packet_init(&packet);

packet.sender_id = 42;
packet.ttl = 10;
ble_discovery_add_to_path(&packet, 42);

// Set GPS
ble_discovery_set_gps(&packet, 37.7749, -122.4194, 50.0);

// Serialize to buffer
uint8_t buffer[256];
uint32_t size = ble_discovery_serialize(&packet, buffer, sizeof(buffer));

// Send buffer over BLE...
```

### Option 3: Use Original C++ Implementation

```cpp
#include "ns3/ble-discovery-header.h"

// Same API as wrapper, but pure C++
BleDiscoveryHeader header;
header.SetSenderId(42);
// ...
```

## Benefits of C Core Architecture

### 1. Portability
- **Embedded Systems**: Compile C core for ARM Cortex-M, ESP32, nRF52, etc.
- **Real Hardware**: Deploy on actual BLE devices
- **Multiple Platforms**: Linux, FreeRTOS, Zephyr, bare metal

### 2. Testing
- **Unit Tests**: Test C core without NS-3
- **Hardware-in-Loop**: Test on dev boards
- **CI/CD**: Faster builds without NS-3 dependency

### 3. Maintainability
- **Separation of Concerns**: Protocol logic separate from simulation
- **Code Reuse**: Same core for simulation and deployment
- **Easier Debugging**: C code is simpler to debug

### 4. Performance
- **Smaller Binary**: C code compiles to smaller binaries
- **Faster**: C is faster than C++ (no vtables, exceptions)
- **Memory Efficient**: Better for resource-constrained devices

## C Core API Reference

### Initialization
```c
void ble_discovery_packet_init(ble_discovery_packet_t *packet);
void ble_election_packet_init(ble_election_packet_t *packet);
```

### TTL Operations
```c
bool ble_discovery_decrement_ttl(ble_discovery_packet_t *packet);
```

### Path Operations
```c
bool ble_discovery_add_to_path(ble_discovery_packet_t *packet, uint32_t node_id);
bool ble_discovery_is_in_path(const ble_discovery_packet_t *packet, uint32_t node_id);
```

### GPS Operations
```c
void ble_discovery_set_gps(ble_discovery_packet_t *packet, double x, double y, double z);
```

### Serialization
```c
uint32_t ble_discovery_get_size(const ble_discovery_packet_t *packet);
uint32_t ble_discovery_serialize(const ble_discovery_packet_t *packet,
                                   uint8_t *buffer, uint32_t buffer_size);
uint32_t ble_discovery_deserialize(ble_discovery_packet_t *packet,
                                     const uint8_t *buffer, uint32_t buffer_size);
```

### Election Functions
```c
uint32_t ble_election_calculate_pdsf(uint32_t previous_pdsf,
                                     uint32_t previous_pi,
                                     uint32_t direct_neighbors,
                                     uint32_t *new_pi_out);
double ble_election_calculate_score(uint32_t direct_connections,
                                      double noise_level);
uint32_t ble_election_generate_hash(uint32_t node_id);
```

## Building

The C core is automatically compiled with the NS-3 module:

```bash
./waf configure --enable-tests --enable-examples
./waf build
```

To compile C core standalone (without NS-3):

```bash
cd src/ble-mesh-discovery/model/protocol-core
gcc -c ble_discovery_packet.c -std=c99 -Wall -Wextra
```

## Testing C Core Standalone

Create a test file `test_c_core.c`:

```c
#include "ble_discovery_packet.h"
#include <stdio.h>
#include <assert.h>

int main() {
    // Test initialization
    ble_discovery_packet_t packet;
    ble_discovery_packet_init(&packet);
    assert(packet.ttl == 10);

    // Test path operations
    ble_discovery_add_to_path(&packet, 1);
    ble_discovery_add_to_path(&packet, 2);
    assert(ble_discovery_is_in_path(&packet, 1) == true);
    assert(ble_discovery_is_in_path(&packet, 3) == false);

    // Test serialization
    uint8_t buffer[256];
    uint32_t size = ble_discovery_serialize(&packet, buffer, sizeof(buffer));
    assert(size > 0);

    printf("All tests passed!\n");
    return 0;
}
```

Compile and run:
```bash
gcc test_c_core.c ble_discovery_packet.c -o test_c_core -std=c99
./test_c_core
```

## Embedded System Integration Example

```c
// On nRF52 BLE device
#include "ble_discovery_packet.h"
#include "nrf_ble.h"

void send_discovery_message(uint32_t my_id) {
    // Create packet
    ble_discovery_packet_t packet;
    ble_discovery_packet_init(&packet);
    packet.sender_id = my_id;
    packet.ttl = 5;

    // Add to path
    ble_discovery_add_to_path(&packet, my_id);

    // Serialize
    uint8_t ble_buffer[64];
    uint32_t size = ble_discovery_serialize(&packet, ble_buffer, sizeof(ble_buffer));

    // Send over BLE advertising
    nrf_ble_advertise(ble_buffer, size);
}
```

## Future Extensions

### Additional C Modules
- `ble_discovery_forwarding.c` - Forwarding logic (Picky, GPS, TTL)
- `ble_discovery_election.c` - Clusterhead election state machine
- `ble_discovery_routing.c` - Routing algorithms
- `ble_discovery_crypto.c` - Optional security features

### Hardware Abstraction Layer
```c
// ble_hal.h - Hardware abstraction
typedef struct {
    void (*send)(const uint8_t *data, uint32_t len);
    uint32_t (*receive)(uint8_t *buffer, uint32_t max_len);
    double (*get_rssi)(void);
    void (*get_gps)(double *x, double *y, double *z);
} ble_hal_t;
```

## Comparison: C Core vs Pure C++

| Feature | C Core + Wrapper | Pure C++ |
|---------|------------------|----------|
| NS-3 Integration | ✅ Yes (via wrapper) | ✅ Yes (native) |
| Embedded Portability | ✅ Yes | ❌ Limited |
| Code Size | ⭐ Smaller | Larger |
| Performance | ⭐ Faster | Good |
| Complexity | Moderate (2 layers) | Simple (1 layer) |
| Maintainability | ⭐ Better (separation) | Good |

## Recommendation

**Use the C Core + Wrapper approach** because:
1. You get portability to real BLE hardware
2. Protocol logic can be tested independently
3. Same code runs in simulation and on devices
4. Better for future commercial/production use
5. Only small wrapper overhead in NS-3

**Use Pure C++ approach** if:
1. You only care about NS-3 simulation
2. Never plan to deploy on real hardware
3. Want simpler single-layer architecture

## Files Summary

```
model/
├── protocol-core/              ← Pure C (portable)
│   ├── ble_discovery_packet.h
│   └── ble_discovery_packet.c
│
├── ble-discovery-header-wrapper.h  ← C++ wrapper for NS-3
├── ble-discovery-header-wrapper.cc
│
├── ble-discovery-header.h      ← Pure C++ alternative
└── ble-discovery-header.cc
```

Choose the approach that fits your goals!
