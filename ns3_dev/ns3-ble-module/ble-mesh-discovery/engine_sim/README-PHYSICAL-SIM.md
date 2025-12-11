# Phase 2 Physical Layer BLE Mesh Discovery Simulation

## Overview

`phase2-discovery-physical-sim.cc` is a comprehensive NS-3 simulation that models realistic BLE mesh discovery with full physical layer effects:

- **GPS Location Modeling**: Realistic latitude/longitude coordinates with Haversine distance calculation
- **Path Loss**: Log-distance path loss model with configurable exponent
- **Multipath Fading**: Rayleigh fading simulation for realistic signal variation
- **Shadowing**: Log-normal shadowing for obstacle effects
- **Timing Errors**: Clock drift and jitter modeling
- **RSSI Calculation**: Distance-based received signal strength
- **Packet Loss**: Realistic delivery based on signal strength vs. noise floor

## Comparison with phase2-discovery-sim.cc

| Feature | phase2-discovery-sim.cc | phase2-discovery-physical-sim.cc |
|---------|------------------------|----------------------------------|
| **Channel Model** | Perfect virtual channel | Path loss + shadowing + fading |
| **RSSI** | Fixed -45 dBm | Distance-based calculation |
| **Packet Loss** | 0% (perfect delivery) | Based on SINR threshold |
| **GPS** | Not modeled | Realistic lat/lon coordinates |
| **Timing** | Perfect sync | Clock drift + jitter |
| **Topology** | Manual connections | Grid with distance-based connectivity |
| **Purpose** | Engine logic validation | Physical layer realism |

## Building

From the NS-3 root directory:

```bash
cd /Users/oliravaneswaramoorthy/Projects/Compass/ns3-jason/ns3-nordic/ns3_dev/ns-3-dev

# Configure with examples enabled
./waf configure --enable-examples

# Build
./waf build

# The executable will be in build/
```

## Running the Simulation

### Basic Usage

```bash
./waf --run phase2-discovery-physical-sim
```

### With Command-Line Arguments

```bash
./waf --run "phase2-discovery-physical-sim \
  --nodes=9 \
  --duration=10.0 \
  --slot=50 \
  --spacing=15.0 \
  --txPower=4.0 \
  --pathLoss=2.5 \
  --shadowing=6.0 \
  --fading=true \
  --drift=50.0 \
  --trace=my_simulation.csv"
```

## Command-Line Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `--nodes` | 4 | Number of nodes in the simulation |
| `--duration` | 5.0 | Simulation duration (seconds) |
| `--slot` | 50 | Discovery slot duration (ms) |
| `--spacing` | 20.0 | Grid spacing between nodes (meters) |
| `--txPower` | 0.0 | Transmit power (dBm) |
| `--pathLoss` | 2.5 | Path loss exponent (2.0=free space, 2.5=indoor) |
| `--shadowing` | 6.0 | Log-normal shadowing std deviation (dB) |
| `--fading` | true | Enable Rayleigh fading |
| `--drift` | 50.0 | Clock drift (parts per million) |
| `--trace` | `physical_simulation_trace.csv` | Output trace file |
| `--lat` | 37.4275 | Base latitude (default: Stanford) |
| `--lon` | -122.1697 | Base longitude |

## Physical Layer Models

### 1. Path Loss Model

**Log-Distance Path Loss:**
```
PL(d) = PL(d0) + 10·n·log10(d/d0)
```

Where:
- `PL(d0)` = 40 dB (reference loss at 1 meter for 2.4 GHz BLE)
- `n` = path loss exponent (configurable)
  - 2.0 = Free space
  - 2.5 = Indoor (default)
  - 3.0-4.0 = Dense indoor/urban

**Example RSSI at different distances (n=2.5, TxPower=0 dBm):**
- 1m: -40 dBm
- 5m: -57 dBm
- 10m: -65 dBm
- 20m: -72 dBm
- 50m: -82 dBm

### 2. Shadowing Model

Log-normal shadowing with configurable standard deviation:
```
Shadowing ~ N(0, σ²)
```

- Typical values: σ = 4-8 dB
- Models slow fading due to obstacles (walls, furniture)
- Independent for each transmission

### 3. Multipath Fading

Rayleigh fading for non-line-of-sight conditions:
```
Fading power ~ Exp(1)
Fading (dB) = 10·log10(X), where X ~ Exp(1)
```

- Models rapid signal fluctuations
- Can be disabled with `--fading=false`

### 4. Total RSSI Calculation

```
RSSI = TxPower - PathLoss(d) - Shadowing - Fading
```

Packet is received if:
```
RSSI > max(RxSensitivity, NoiseFloor, ProximityThreshold)
```

Defaults:
- RxSensitivity = -90 dBm
- NoiseFloor = -95 dBm
- ProximityThreshold = -70 dBm (configurable per node)

### 5. Timing Errors

**Clock Drift:**
- Each node has random drift: ±50 ppm (default)
- Local time = Real time × (1 + drift/10⁶)
- Over 1 second: ±50 μs error

**Transmission Jitter:**
- Gaussian jitter: N(0, 0.5 ms)
- Models crystal oscillator imperfections

### 6. GPS Modeling

**Coordinate System:**
- Nodes placed in Cartesian grid for NS-3 simulation
- GPS coordinates calculated from grid positions
- Base location: Stanford University (37.4275°N, 122.1697°W)

**Distance Calculation:**
- Haversine formula for GPS distances
- Euclidean distance for RSSI calculation
- Both available in trace output

## Output Trace Format

CSV file with the following events:

### SEND Event
```
time_ms,SEND,sender_id,,originator_id,ttl,path_length,,latitude,longitude
```

### RECV Event
```
time_ms,RECV,,receiver_id,originator_id,ttl,path_length,rssi,latitude,longitude
```

### TOPOLOGY Event
```
0,TOPOLOGY,node_id,,,,,,,x_position,y_position
```

### STATS Event (per node)
```
end_time,STATS,node_id,sent,received,forwarded,dropped,,latitude,longitude
```

### CHANNEL_STATS Event
```
end_time,CHANNEL_STATS,total_tx,successful_rx,collisions,drop_distance,drop_fading,,,,
```

## Example Scenarios

### 1. Dense Urban Environment
```bash
./waf --run "phase2-discovery-physical-sim \
  --nodes=16 \
  --spacing=10.0 \
  --pathLoss=3.5 \
  --shadowing=8.0"
```

### 2. Outdoor Open Area
```bash
./waf --run "phase2-discovery-physical-sim \
  --nodes=9 \
  --spacing=50.0 \
  --pathLoss=2.0 \
  --shadowing=4.0 \
  --fading=false"
```

### 3. High Power Long Range
```bash
./waf --run "phase2-discovery-physical-sim \
  --nodes=25 \
  --spacing=30.0 \
  --txPower=8.0 \
  --pathLoss=2.5"
```

### 4. Low Power IoT Devices
```bash
./waf --run "phase2-discovery-physical-sim \
  --nodes=12 \
  --txPower=-12.0 \
  --spacing=5.0"
```

## Performance Metrics

The simulation calculates:

1. **Packet Delivery Ratio (PDR)**: Successful receptions / total transmissions
2. **Per-Node Statistics**:
   - Messages sent
   - Messages received
   - Messages forwarded
   - Messages dropped

3. **Channel Statistics**:
   - Total transmissions
   - Successful receptions
   - Packets dropped due to distance
   - Packets dropped due to fading

## Visualizing Results

Use the same Python visualization tools as phase2-discovery-sim.cc:

```bash
python3 visualize_trace.py physical_simulation_trace.csv
```

The trace includes additional columns for GPS coordinates and RSSI values.

## Debugging

Enable detailed logging:

```bash
export NS_LOG="Phase2PhysicalDiscoverySim=level_all|prefix_time"
./waf --run phase2-discovery-physical-sim
```

## Implementation Details

### Key Classes

1. **RealisticBleChannel**: Implements physical layer propagation
   - Path loss calculation
   - Shadowing generation
   - Fading simulation
   - Packet delivery based on RSSI

2. **PhysicalSimNode**: Enhanced node with physical layer features
   - GPS location
   - Clock drift modeling
   - Timing jitter
   - RSSI conversion

3. **GpsCoordinate**: GPS coordinate structure
   - Haversine distance calculation
   - Lat/lon to offset conversion

### Simplifications vs. Full BLE Stack

Still simplified compared to a complete BLE implementation:

- **No MAC layer**: Direct packet delivery (no scanning windows)
- **No collisions**: Multiple simultaneous transmissions don't interfere
- **No retransmissions**: Single delivery attempt per packet
- **Simplified interference**: Only noise floor, no co-channel interference

For full BLE MAC/PHY, use the NS-3 BLE module with SpectrumPhy.

## Next Steps

To add even more realism:

1. **Mobility**: Use NS-3 mobility models (RandomWalk, RandomWaypoint)
2. **Energy**: Integrate battery models to track power consumption
3. **Interference**: Model co-channel interference from multiple transmitters
4. **MAC**: Add realistic advertising/scanning timing
5. **Link Quality**: Model packet error rate based on SINR

## References

- **Path Loss Models**: Rappaport, "Wireless Communications" (2002)
- **BLE Specification**: Bluetooth Core Spec v5.3, Vol 6, Part A
- **NS-3 Propagation**: https://www.nsnam.org/docs/models/html/propagation.html
