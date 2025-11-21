# BLE Mesh Discovery Protocol Module for NS-3

This NS-3 module implements a scalable BLE mesh discovery protocol with clusterhead election, designed to support thousands of nodes (up to 10,000+).

## Overview

Traditional BLE mesh networks can only feasibly support 100-150 node devices before communication starts failing due to crowding, collisions, and high latency. This protocol implements:

### Key Features

1. **Discovery Protocol**
   - Discovery messages with ID, TTL, PSF (Path So Far), and LHGPS (Last Heard GPS)
   - 4-slot discovery cycle (1 for own message, 3 for forwarding)
   - Single-channel operation

2. **Smart Message Forwarding**
   - **Picky Forwarding**: Uses crowding factor to forward only a percentage of messages
   - **GPS Proximity Filtering**: Prevents forwarding if previous sender too close
   - **TTL-Based Prioritization**: Top-3 messages selected by remaining TTL

3. **Clusterhead Election**
   - RSSI-based election with 3-round announcement
   - Connectivity metric calculation (direct neighbors, unique paths, geographic distribution)
   - Candidacy based on direct connection:noise ratio
   - Dynamic threshold based on local crowding factor
   - Conflict resolution and re-announcement

4. **Cluster Management**
   - Cluster capacity limiting (150 devices per cluster)
   - PDSF (Predicted Devices So Far) calculation: t(x) = Σᵢ Πᵢ(xᵢ)
   - Edge node alignment with clusterheads
   - Multi-path memorization with Dijkstra's shortest path
   - Routing tree construction

5. **Scalability Features**
   - Supports 1000+ nodes through hierarchical clustering
   - End-to-end latency in seconds range
   - Designed for ad hoc sensor networks, large-scale lighting, smart city deployments

## Project Status

**Current Phase:** Foundation & Setup (Task 2 Complete)

- [x] Task 1: Research NS-3 BLE/Bluetooth capabilities
- [x] Task 2: Set up NS-3 development environment
- [ ] Task 3: Design core BLE discovery message packet structure
- [ ] Task 4: Implement discovery message header class
- [ ] Task 5+: See TODO.md for complete task list

## Module Structure

```
ble-mesh-discovery/
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
├── model/                  # Core protocol implementation
│   ├── ble-discovery-header.{h,cc}
│   ├── ble-mesh-node.{h,cc}
│   ├── ble-discovery-protocol.{h,cc}
│   ├── ble-cluster-election.{h,cc}
│   ├── ble-cluster-manager.{h,cc}
│   └── ble-mesh-routing.{h,cc}
├── helper/                 # Helper classes for easy setup
│   ├── ble-mesh-helper.{h,cc}
│   └── ble-cluster-helper.{h,cc}
├── examples/               # Example simulation scripts
│   ├── ble-mesh-simple.cc
│   ├── ble-mesh-clustering.cc
│   └── ble-mesh-1000nodes.cc
├── test/                   # Unit and integration tests
│   └── ble-mesh-discovery-test-suite.cc
└── doc/                    # Documentation
    └── ble-mesh-discovery.rst
```

## Dependencies

This module requires:
- **NS-3 core modules**: core, network, spectrum, mobility, energy, propagation
- **BLE module**: Stijn's ns3-ble-module (https://gitlab.com/Stijng/ns3-ble-module)
  - Provides BlePhy and BleMac foundation
  - Must be ported to CMake and placed in `ns-3-dev/src/ble/`

## Building

Once BLE module is properly integrated:

```bash
cd ns-3-dev
./ns3 configure --enable-tests --enable-examples
./ns3 build
```

Run examples:
```bash
./ns3 run ble-mesh-simple
```

## Protocol Specification

Based on: "Clusterhead & BLE Mesh discovery process" by jason.peng (November 2025)

See `../TODO.md` for detailed implementation plan and references.

## Performance Targets

- **Network Size**: 1000-10,000 nodes
- **Latency**: Endpoint-to-endpoint in seconds range
- **Cluster Size**: Maximum 150 devices per cluster
- **Use Cases**: Ad hoc sensor networks, large-scale lighting/automation, smart city infrastructure

## Development Timeline

Estimated: 4-6 months for complete implementation
- Phase 1: Foundation & Setup (Weeks 1-3)
- Phase 2: Core Discovery Protocol (Weeks 4-8)
- Phase 3: Clusterhead Election (Weeks 9-12)
- Phase 4: Cluster Formation & Routing (Weeks 13-16)
- Phase 5: Testing & Optimization (Weeks 17-20)

## License

To be determined (follow NS-3 and BLE module licensing)

## References

- NS-3 Documentation: https://www.nsnam.org/documentation/
- Stijn's BLE Module: https://gitlab.com/Stijng/ns3-ble-module
- Protocol Specification: See PDF in parent directory
- Research Papers: See TODO.md for complete list
