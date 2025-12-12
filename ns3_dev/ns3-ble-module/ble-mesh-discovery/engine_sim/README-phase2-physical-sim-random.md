# Phase 2 Physical-Layer BLE Mesh Discovery Simulation - Random Topology

## Overview

This simulation implements a realistic physical-layer BLE mesh discovery protocol with randomized node positions. It models real-world wireless propagation effects including path loss, shadowing, multipath fading, timing errors, and GPS-based positioning.

**File:** `phase2-discovery-physical-sim-random.cc`

---

## How to Run

### Basic Execution

```bash
./ns3 run "phase2-discovery-physical-sim-random"
```

### With Custom Parameters

```bash
./ns3 run "phase2-discovery-physical-sim-random --nodes=10 --duration=30 --pathLoss=3.0 --seed=42"
```

---

## Command Line Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `--nodes` | uint32 | 4 | Number of BLE nodes in the simulation |
| `--duration` | double | 5.0 | Simulation duration in seconds |
| `--slot` | uint32 | 50 | Discovery slot duration in milliseconds |
| `--areaWidth` | double | 100.0 | Deployment area width in meters |
| `--areaHeight` | double | 100.0 | Deployment area height in meters |
| `--txPower` | double | 0.0 | Transmit power in dBm (BLE typical: 0 dBm) |
| `--pathLoss` | double | 2.5 | Path loss exponent (2.0=free space, 2.5=indoor) |
| `--shadowing` | double | 6.0 | Shadowing standard deviation in dB |
| `--fading` | bool | true | Enable/disable Rayleigh fading |
| `--drift` | double | 50.0 | Clock drift in parts per million (±ppm) |
| `--trace` | string | "physical_simulation_trace_random.csv" | Output CSV trace file path |
| `--lat` | double | 37.4275 | Base GPS latitude (default: Stanford) |
| `--lon` | double | -122.1697 | Base GPS longitude |
| `--seed` | uint32 | 1 | Random seed for reproducibility |

### Example Commands

**Small indoor network:**
```bash
./ns3 run "phase2-discovery-physical-sim-random --nodes=5 --areaWidth=20 --areaHeight=20 --pathLoss=3.0"
```

**Large outdoor network:**
```bash
./ns3 run "phase2-discovery-physical-sim-random --nodes=20 --areaWidth=200 --areaHeight=200 --pathLoss=2.0 --duration=60"
```

**Test reproducibility:**
```bash
./ns3 run "phase2-discovery-physical-sim-random --seed=123"
```

---

## Physical Layer Models

### 1. Path Loss (Distance-Based Attenuation)

**Purpose:** Models average signal attenuation over distance in a given environment.

**Implementation:** Log-distance path loss model ([lines 265-275](phase2-discovery-physical-sim-random.cc#L265-L275))

```cpp
PL(d) = PL(d₀) + 10 × n × log₁₀(d/d₀)
```

Where:
- `PL(d₀)` = 40 dB (reference loss at 1 meter for 2.4 GHz)
- `n` = path loss exponent (configurable via `--pathLoss`)
- `d` = distance between transmitter and receiver

**Physical Meaning:**
- `n = 2.0`: Free space (line-of-sight, no obstacles)
- `n = 2.5`: Light indoor (default - some walls/furniture)
- `n = 3.0`: Moderate indoor (typical office environment)
- `n = 4.0`: Dense indoor (many obstacles)

**Code Reference:**
```cpp
double RealisticBleChannel::CalculatePathLoss (double distance) const
{
  // Log-distance path loss model: PL(d) = PL(d0) + 10*n*log10(d/d0)
  return m_referenceLoss + 10.0 * m_pathLossExponent *
         std::log10 (distance / m_referenceDistance);
}
```

---

### 2. Shadowing (Obstacle Variability)

**Purpose:** Models random variations in signal strength due to different obstacle densities between transmitter-receiver pairs at the same distance.

**Implementation:** Log-normal shadowing ([lines 278-282](phase2-discovery-physical-sim-random.cc#L278-L282))

```
Shadowing ~ N(0, σ²)  where σ = 6 dB (default)
```

**Physical Meaning:**
- Captures the fact that two links at the same distance can have different signal strengths
- One path might go through concrete walls (higher loss)
- Another path might have clear line-of-sight (lower loss)
- Statistical approximation of obstacle impedance effects

**Distribution:**
- Mean = 0 dB (no bias on average)
- ~68% of links experience ±6 dB variation
- ~95% of links experience ±12 dB variation

**Code Reference:**
```cpp
double RealisticBleChannel::CalculateShadowing () const
{
  // Log-normal shadowing (in dB)
  return m_shadowingRng->GetValue ();
}
```

Initialized at [lines 208-210](phase2-discovery-physical-sim-random.cc#L208-L210):
```cpp
m_shadowingRng = CreateObject<NormalRandomVariable> ();
m_shadowingRng->SetAttribute ("Mean", DoubleValue (0.0));
m_shadowingRng->SetAttribute ("Variance", DoubleValue (m_shadowingStdDev * m_shadowingStdDev));
```

---

### 3. Multipath Fading (Reflections & Refraction)

**Purpose:** Models rapid signal variations due to constructive and destructive interference from multiple signal paths (reflections, diffractions, refractions).

**Implementation:** Rayleigh fading ([lines 285-296](phase2-discovery-physical-sim-random.cc#L285-L296))

```
Fading (dB) = 10 × log₁₀(X)  where X ~ Exp(1)
```

**Physical Meaning:**
- Signal arrives via multiple paths (direct, reflected off walls/floors, refracted through materials)
- Paths have different phases → interference
- **Constructive interference**: Signal boost (X > 1 → positive dB)
- **Destructive interference**: Deep fades (X < 1 → negative dB)
- Models NLOS (Non-Line-Of-Sight) scenarios

**Statistical Properties:**
- Mean effect = 0 dB over time
- Can cause deep fades (occasionally -20 dB or worse)
- Can cause signal boosts (occasionally +10 dB)
- Changes rapidly with movement (wavelength = 12.5 cm at 2.4 GHz)

**Code Reference:**
```cpp
double RealisticBleChannel::CalculateFading () const
{
  if (!m_fadingEnabled)
    return 0.0;

  // Rayleigh fading: exponentially distributed power with mean 1
  double linearFading = m_fadingRng->GetValue ();
  return 10.0 * std::log10 (linearFading);
}
```

---

### 4. Complete RSSI Calculation

All three effects are combined ([lines 299-312](phase2-discovery-physical-sim-random.cc#L299-L312)):

```cpp
RSSI (dBm) = TxPower - PathLoss(distance) - Shadowing - Fading
```

**Example:**
- TxPower = 0 dBm
- Distance = 20 m → PathLoss = 72.5 dB
- Shadowing = +3.2 dB (random obstacle effect)
- Fading = -2.2 dB (random multipath effect)
- **RSSI = 0 - 72.5 - 3.2 - (-2.2) = -73.5 dBm**

---

### 5. Receiver Sensitivity & Noise Floor

**Purpose:** Determine if received signal is strong enough to decode.

**Implementation:** Dual threshold check ([lines 371-414](phase2-discovery-physical-sim-random.cc#L371-L414))

**Thresholds:**
1. **Receiver Sensitivity:** -90 dBm (typical BLE hardware limit)
2. **Noise Floor:** -95 dBm (thermal noise + receiver noise figure)

**Packet Reception Logic:**
```
if (RSSI < -90 dBm):
    Drop packet (below receiver sensitivity)
    Stats: droppedDueToDistance++
else if (RSSI < -95 dBm):
    Drop packet (below noise floor)
    Stats: droppedDueToFading++
else:
    Successfully receive packet
    Stats: successfulReceptions++
```

---

### 6. Timing Errors

#### A. Clock Drift (Systematic Error)

**Purpose:** Models crystal oscillator frequency offset causing clocks to run at different rates.

**Implementation:** Linear time scaling ([lines 562-572](phase2-discovery-physical-sim-random.cc#L562-L572))

```
LocalTime = RealTime × (1 + drift/1,000,000)
```

**Configuration:**
- Each node assigned random drift from Uniform(-50, +50) ppm ([lines 714-718](phase2-discovery-physical-sim-random.cc#L714-L718))
- Typical quartz crystal tolerance: ±50 ppm

**Physical Meaning:**
- Node with +50 ppm drift gains 50 μs per second
- After 1000 seconds, clock is 50 ms ahead
- Causes slot boundary misalignment between nodes

**Code Reference:**
```cpp
Time PhysicalSimNode::GetLocalTime () const
{
  Time elapsed = Simulator::Now () - m_startTime;
  double driftFactor = 1.0 + (m_clockDriftPpm / 1e6);
  Time localElapsed = NanoSeconds (elapsed.GetNanoSeconds () * driftFactor);
  return m_startTime + localElapsed;
}
```

**Note:** While implemented, this function may not be actively used in the current discovery protocol timing.

#### B. Timing Jitter (Random Error)

**Purpose:** Models random variations in transmission timing due to oscillator phase noise and processing delays.

**Implementation:** Gaussian jitter ([lines 491-493](phase2-discovery-physical-sim-random.cc#L491-L493))

```
Jitter ~ N(0, 0.5 ms)
```

**Applied in two places:**

1. **Initial Startup Offset** ([lines 536-541](phase2-discovery-physical-sim-random.cc#L536-L541)): Nodes don't all start at exactly t=0
2. **Per-Transmission Delay** ([lines 597-603](phase2-discovery-physical-sim-random.cc#L597-L603)): Each packet transmission has random delay

**Code Reference:**
```cpp
Time PhysicalSimNode::ApplyTimingError (Time nominalTime)
{
  double jitterMs = m_timingJitterRng->GetValue ();
  return nominalTime + MilliSeconds (std::abs (jitterMs));
}
```

---

### 7. Propagation Delay

**Purpose:** Models finite speed of light and additional random delays.

**Implementation:** ([lines 416-418](phase2-discovery-physical-sim-random.cc#L416-L418))

```
Delay = distance/c + RandomJitter(0, 0.5 ms)
```

Where c = 3×10⁸ m/s (speed of light)

**Physical Meaning:**
- EM waves propagate at finite speed
- Additional jitter models processing delays, medium effects

---

### 8. GPS Positioning

**Purpose:** Realistic geographic coordinates for nodes, distance calculations using Earth's curvature.

**Implementation:**
- Haversine formula for distance ([lines 74-89](phase2-discovery-physical-sim-random.cc#L74-L89))
- Random placement within defined area ([lines 697-708](phase2-discovery-physical-sim-random.cc#L697-L708))

**Coordinate System:**
- Base GPS location (default: Stanford University at 37.4275°N, 122.1697°W)
- Cartesian positions converted to GPS offsets
- ~111 km per degree latitude, ~111×cos(lat) km per degree longitude

**Code Reference:**
```cpp
double GpsCoordinate::DistanceTo (const GpsCoordinate &other) const
{
  const double R = 6371000.0; // Earth radius in meters
  // ... Haversine formula ...
  return R * c;
}
```

---

## Output and Results

### Console Output

The simulation prints summary statistics at completion:

```
=== Simulation Summary ===
Random Seed: 1
Deployment Area: 100m x 100m
Total messages sent: 120
Total messages received: 450
Total messages forwarded: 330
Total messages dropped: 15
Channel transmissions: 450
Successful receptions: 1350
Total packets lost: 45
Dropped (distance): 30
Dropped (fading): 15
Packet Delivery Ratio: 75.0%
Trace written to: physical_simulation_trace_random.csv
```

### CSV Trace File

Output file format (default: `physical_simulation_trace_random.csv`):

**Header:**
```
time_ms,event,sender_id,receiver_id,originator_id,ttl,path_length,rssi,latitude,longitude
```

**Event Types:**
- `TOPOLOGY`: Node placement (time=0)
- `SEND`: Packet transmission
- `RECV`: Packet reception
- `PACKET_LOSS`: Dropped packet
- `STATS`: Final per-node statistics

**Example Rows:**
```
0,TOPOLOGY,1,,37.4275,-122.1697,,,45.3,67.8
150,SEND,1,,1,6,0,,37.4275,-122.1697
152,RECV,,2,1,6,1,-68,37.4276,-122.1696
152,PACKET_LOSS,1,3,1,,,-92,37.4277,-122.1698
```

---

## What Successful Results Look Like

### Good Discovery Performance

**Packet Delivery Ratio (PDR):**
- **80-100%**: Excellent (nodes close together, good propagation)
- **60-80%**: Good (typical indoor environment)
- **40-60%**: Moderate (challenging environment, distant nodes)
- **<40%**: Poor (nodes too far apart or excessive interference)

**Expected Behavior:**
1. All nodes discover each other (appear in routing tables)
2. Messages successfully forwarded through mesh hops
3. Low packet loss due to distance (most nodes within range)
4. Some packet loss due to fading (expected from multipath)

### Parameter Impact on Results

**Path Loss Exponent:**
- Higher `n` → More distance-sensitive → Lower PDR
- `n=2.0` (free space) should give best results
- `n=4.0` (dense indoor) will reduce range significantly

**Shadowing Std Dev:**
- Higher `σ` → More variable link quality → Some links very poor
- `σ=0` (no shadowing) → Deterministic results
- `σ=10` dB → High variability

**Fading:**
- `--fading=false`: No multipath effects, more predictable
- `--fading=true`: Realistic but adds randomness, occasional deep fades

**Node Density:**
- More nodes in same area → More potential collisions
- Larger area with same nodes → Lower PDR (greater distances)

### Validation Checklist

- [ ] All nodes appear in TOPOLOGY events at t=0
- [ ] SEND events occur regularly (nodes transmitting)
- [ ] RECV events match SEND events (successful propagation)
- [ ] PACKET_LOSS events show RSSI below -90 dBm
- [ ] PDR makes sense for the environment parameters
- [ ] Distance-based drops > fading-based drops (typical)
- [ ] Each node discovers neighbors within communication range

---

## Troubleshooting

**No packets received:**
- Check if area too large for txPower (default 0 dBm, ~30-50m range with n=2.5)
- Reduce `--areaWidth` and `--areaHeight`
- Increase `--txPower`

**PDR too high (100%):**
- Increase area size or path loss exponent for more realistic scenario
- Enable fading: `--fading=true`

**PDR too low (<40%):**
- Decrease path loss exponent: `--pathLoss=2.0`
- Decrease shadowing: `--shadowing=3.0`
- Increase transmission power: `--txPower=4.0`

**Reproducibility issues:**
- Use same `--seed` value for identical random sequences
- Different seeds will produce different topologies and fading patterns

---

## References

**Physical Models:**
- Log-distance path loss: Standard wireless propagation model
- Log-normal shadowing: Empirically validated for indoor environments
- Rayleigh fading: NLOS multipath model (ITU, 3GPP standards)

**BLE Specifications:**
- TX power: 0 dBm typical (BLE Core Spec 5.3)
- RX sensitivity: -90 dBm minimum (Class 1 devices)
- Frequency: 2.4 GHz ISM band
- Crystal tolerance: ±50 ppm typical for commercial devices
