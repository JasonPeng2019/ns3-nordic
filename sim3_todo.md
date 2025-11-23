# Phase 3 Simulation TODO (NS-3 Workflow)

This document describes how to exercise the Phase 3 discovery engine inside NS-3 using the `phase3-discovery-engine-sim` program, along with companion C-level regression tests. Follow each section literally.

---

## 1. Build Instructions

```
cd /Users/oliravaneswaramoorthy/Projects/Compass/ns3-jason/ns3-nordic/ns3_dev/ns-3-dev
./waf configure --enable-examples --enable-tests --disable-werror
./waf build src/ns3-ble-module
```

The `--disable-werror` flag suppresses the Clang VLA warning observed during earlier builds. Re-run `./waf configure` whenever you add new files (e.g., `phase3-discovery-sim.cc`).

---

## 2. Run the NS-3 Phase 3 Simulation

### Program
- Target: `phase3-discovery-engine-sim` (defined in `ble-mesh-discovery/engine_sim/phase3-discovery-sim.cc`)
- Built via `engine_sim/wscript`

### Purpose
- Instantiates `BleDiscoveryEngineWrapper` nodes.
- Positions nodes randomly in a square area.
- Applies synthetic crowding/noise to each node.
- Connects nodes via a virtual channel that forwards packets between neighbours with delay/RSSI derived from distance.
- Verifies that at least one node becomes a clusterhead and other nodes align to it.

### Command
```
./waf --run "phase3-discovery-engine-sim \
             --nodes=20 \
             --duration=8 \
             --slot=50ms \
             --area=200 \
             --range=120 \
             --seed=3"
```

Meaning of parameters:
- `nodes` – number of simulated discovery engines (default 12).
- `duration` – total simulation time in seconds (default 6).
- `slot` – discovery slot duration; must match `BleDiscoveryEngineWrapper` expectation (default 50 ms).
- `area` – side length of the square (meters) used for GPS placement.
- `range` – maximum neighbour distance. Links are created for any pair within this range and for the default ring to guarantee connectivity.
- `seed` – RNG stream for reproducible placement.

Expected behaviour:
- NS_LOG output lists per-node stats (`state`, sent/received/forwarded counts, selected clusterhead).
- Simulation aborts if no clusterheads are elected, if no edge devices align, or if no forwarding events occur.

To capture more detail, enable wrapper logging:
```
NS_LOG=BleDiscoveryEngineWrapper=level_info,Phase3DiscoveryEngineSim=level_info ./waf --run "phase3-discovery-engine-sim"
```

---

## 3. Regression Test (C Core Only)

File: `ble-mesh-discovery/test/ble-phase3-engine-c-test.c`

Compile example (standalone):
```
cd /Users/oliravaneswaramoorthy/Projects/Compass/ns3-jason/ns3-nordic/ns3_dev/ns3-ble-module/ble-mesh-discovery
gcc -Imodel/protocol-core -Imodel/engine-core \
    test/ble-phase3-engine-c-test.c \
    model/protocol-core/ble_discovery_packet.c \
    model/protocol-core/ble_mesh_node.c \
    model/protocol-core/ble_discovery_cycle.c \
    model/protocol-core/ble_message_queue.c \
    model/protocol-core/ble_forwarding_logic.c \
    model/protocol-core/ble_election.c \
    model/protocol-core/ble_broadcast_timing.c \
    model/engine-core/ble_discovery_engine.c \
    -o test-phase3-engine-c
./test-phase3-engine-c
```

Purpose:
- Instantiates several `ble_engine_t` objects.
- Wires their send callbacks through an in-memory “bus”.
- Drives the Phase 3 state machine (including queue saturation) and asserts that at least one node becomes a clusterhead while others align to it.

---

## 4. Extending the NS-3 Simulation

To stress scalability beyond the defaults:
1. Increase `--nodes` (e.g., `200`).
2. Adjust `--range` upward to maintain connectivity at higher densities (e.g., `range=180` for 200 nodes in a 300 m area).
3. Increase `--duration` so multiple discovery cycles complete (10–15 s recommended for 200 nodes).
4. Review `phase3-discovery-sim.cc` for knobs:
   - `RandomCrowding()` – modify to emulate dense/ sparse deployments.
   - Connectivity scaffold (ring + distance-based edges) – replace with custom topology if needed.
   - `SimpleVirtualChannel::Deliver()` – adapt RSSI model or introduce packet drops.

After any change:
```
./waf build src/ns3-ble-module
./waf --run "phase3-discovery-engine-sim ..."
```

---

## 5. Diagnostics Checklist

- **Clusterhead election** – Ensure `clusterheads > 0` in the final summary line. If zero, lower `slot` duration or increase `range` so nodes accumulate more neighbours.
- **Edge alignment** – `alignedEdges` should equal `nodes - clusterheads`. If not, inspect log lines for `clusterhead=0`.
- **Forwarders** – `totalForwarders` should be >0; if zero, reduce crowding factors or TTL.
- **Queue pressure** – monitor `state->stats.messages_dropped`. Sustained non-zero values indicate forwarding throttling under load; adjust parameters accordingly.

---

Keep this TODO updated whenever the NS-3 program or tests change so anyone can reproduce the Phase 3 simulation end-to-end.
