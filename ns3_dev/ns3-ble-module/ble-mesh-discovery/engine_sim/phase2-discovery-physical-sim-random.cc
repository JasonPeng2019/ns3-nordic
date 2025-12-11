/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Phase 2 Physical-Layer BLE Mesh Discovery Simulation - Randomized Topology
 *
 * This simulation adds realistic physical layer effects with RANDOMIZED node positions:
 * - GPS location modeling with realistic coordinates
 * - Path loss with distance-based RSSI calculation
 * - Multipath fading (Rayleigh/Rician models)
 * - Timing errors and clock drift
 * - Packet loss based on SINR
 * - Realistic BLE advertising/scanning behavior
 * - RANDOM topology instead of fixed grid
 *
 * Unlike phase2-discovery-physical-sim.cc which uses a deterministic grid,
 * this version randomizes node positions within a specified area.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/ble-discovery-engine-wrapper.h"
#include "ns3/ble-discovery-header-wrapper.h"
#include "ns3/random-variable-stream.h"

#include <map>
#include <vector>
#include <fstream>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Phase2PhysicalDiscoverySimRandom");

// Global trace file for CSV output
std::ofstream g_traceFile;

// Global statistics
struct SimulationStats
{
  uint64_t totalTransmissions;
  uint64_t successfulReceptions;
  uint64_t collisions;
  uint64_t droppedDueToDistance;
  uint64_t droppedDueToFading;
  uint64_t totalPacketsLost;  // Total packets lost (distance + fading)

  SimulationStats ()
    : totalTransmissions (0),
      successfulReceptions (0),
      collisions (0),
      droppedDueToDistance (0),
      droppedDueToFading (0),
      totalPacketsLost (0)
  {}
};

SimulationStats g_stats;

/**
 * \brief GPS coordinate structure with realistic lat/lon
 */
struct GpsCoordinate
{
  double latitude;   // degrees
  double longitude;  // degrees
  double altitude;   // meters

  GpsCoordinate (double lat = 0.0, double lon = 0.0, double alt = 0.0)
    : latitude (lat), longitude (lon), altitude (alt)
  {}

  // Calculate Haversine distance between two GPS coordinates (meters)
  double DistanceTo (const GpsCoordinate &other) const
  {
    const double R = 6371000.0; // Earth radius in meters
    double lat1 = latitude * M_PI / 180.0;
    double lat2 = other.latitude * M_PI / 180.0;
    double dLat = (other.latitude - latitude) * M_PI / 180.0;
    double dLon = (other.longitude - longitude) * M_PI / 180.0;

    double a = std::sin (dLat / 2.0) * std::sin (dLat / 2.0) +
               std::cos (lat1) * std::cos (lat2) *
               std::sin (dLon / 2.0) * std::sin (dLon / 2.0);
    double c = 2.0 * std::atan2 (std::sqrt (a), std::sqrt (1.0 - a));

    return R * c;
  }
};

class PhysicalSimNode;

/**
 * \brief Realistic wireless channel with path loss, fading, and interference
 */
class RealisticBleChannel : public Object
{
public:
  static TypeId GetTypeId (void);

  RealisticBleChannel ();

  void AddNode (Ptr<PhysicalSimNode> node);
  void Transmit (uint32_t senderId, Ptr<Packet> packet, double txPowerDbm);

  void SetPathLossExponent (double exponent);
  void SetReferenceDistance (double d0);
  void SetReferenceLoss (double pl0);
  void SetShadowingStdDev (double stdDev);
  void SetFadingEnabled (bool enable);
  void SetNoiseFloorDbm (double noiseDbm);

private:
  void Deliver (uint32_t receiverId, Ptr<Packet> packet, double rxPowerDbm);
  double CalculatePathLoss (double distance) const;
  double CalculateShadowing () const;
  double CalculateFading () const;
  double CalculateRssi (double txPowerDbm, double distance) const;

  std::map<uint32_t, Ptr<PhysicalSimNode>> m_nodes;

  // Propagation model parameters
  double m_pathLossExponent;        // Typical: 2.0-4.0 (2.0 = free space)
  double m_referenceDistance;       // d0 in meters (typically 1.0m)
  double m_referenceLoss;          // Path loss at d0 in dB (typically 40 dB for BLE)
  double m_shadowingStdDev;        // Log-normal shadowing std dev (typically 4-8 dB)
  bool m_fadingEnabled;            // Enable Rayleigh fading
  double m_noiseFloorDbm;          // Receiver noise floor (typically -95 dBm)

  Ptr<NormalRandomVariable> m_shadowingRng;
  Ptr<ExponentialRandomVariable> m_fadingRng;
  Ptr<UniformRandomVariable> m_propDelayRng;
};

/**
 * \brief Physical layer node with GPS, timing errors, and realistic radio
 */
class PhysicalSimNode : public Object
{
public:
  static TypeId GetTypeId (void);

  PhysicalSimNode ();

  void Configure (uint32_t nodeId,
                  const GpsCoordinate &gpsLocation,
                  const Vector &nsPosition,
                  Time slotDuration,
                  uint8_t initialTtl,
                  double proximityThreshold,
                  Ptr<RealisticBleChannel> channel,
                  double txPowerDbm,
                  double rxSensitivityDbm);

  void Start ();
  void ReceivePacket (Ptr<Packet> packet, double rssiDbm);

  uint32_t GetNodeId () const { return m_nodeId; }
  GpsCoordinate GetGpsLocation () const { return m_gpsLocation; }
  Vector GetPosition () const { return m_position; }
  const ble_mesh_node_t* GetNodeState () const;

  // Timing error simulation
  Time GetLocalTime () const;
  void SetClockDrift (double ppm); // Parts per million

private:
  void HandleEngineSend (Ptr<Packet> packet);
  Time ApplyTimingError (Time nominalTime);
  int8_t ConvertDbmToInt8 (double dbm) const;

  Ptr<BleDiscoveryEngineWrapper> m_engine;
  Ptr<RealisticBleChannel> m_channel;

  uint32_t m_nodeId;
  GpsCoordinate m_gpsLocation;
  Vector m_position;               // NS-3 Cartesian position

  double m_txPowerDbm;             // Transmit power (typical: 0 dBm for BLE)
  double m_rxSensitivityDbm;       // Receiver sensitivity (typical: -90 dBm)

  // Timing error modeling
  double m_clockDriftPpm;          // Clock drift in parts per million
  Time m_startTime;                // Simulation start time
  Ptr<NormalRandomVariable> m_timingJitterRng;

  bool m_started;
};

TypeId
RealisticBleChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("RealisticBleChannel")
    .SetParent<Object> ()
    .SetGroupName ("BleMeshDiscovery");
  return tid;
}

RealisticBleChannel::RealisticBleChannel ()
  : m_pathLossExponent (2.5),        // Indoor environment
    m_referenceDistance (1.0),       // 1 meter
    m_referenceLoss (40.0),          // Free space path loss at 1m for 2.4 GHz
    m_shadowingStdDev (6.0),         // Moderate shadowing
    m_fadingEnabled (true),
    m_noiseFloorDbm (-95.0)
{
  m_shadowingRng = CreateObject<NormalRandomVariable> ();
  m_shadowingRng->SetAttribute ("Mean", DoubleValue (0.0));
  m_shadowingRng->SetAttribute ("Variance", DoubleValue (m_shadowingStdDev * m_shadowingStdDev));

  m_fadingRng = CreateObject<ExponentialRandomVariable> ();
  m_fadingRng->SetAttribute ("Mean", DoubleValue (1.0));

  m_propDelayRng = CreateObject<UniformRandomVariable> ();
  m_propDelayRng->SetAttribute ("Min", DoubleValue (0.0));
  m_propDelayRng->SetAttribute ("Max", DoubleValue (0.5)); // Up to 0.5ms additional delay
}

void
RealisticBleChannel::AddNode (Ptr<PhysicalSimNode> node)
{
  NS_ABORT_MSG_IF (!node, "Cannot add null node");
  m_nodes[node->GetNodeId ()] = node;
}

void
RealisticBleChannel::SetPathLossExponent (double exponent)
{
  m_pathLossExponent = exponent;
}

void
RealisticBleChannel::SetReferenceDistance (double d0)
{
  m_referenceDistance = d0;
}

void
RealisticBleChannel::SetReferenceLoss (double pl0)
{
  m_referenceLoss = pl0;
}

void
RealisticBleChannel::SetShadowingStdDev (double stdDev)
{
  m_shadowingStdDev = stdDev;
  m_shadowingRng->SetAttribute ("Variance", DoubleValue (stdDev * stdDev));
}

void
RealisticBleChannel::SetFadingEnabled (bool enable)
{
  m_fadingEnabled = enable;
}

void
RealisticBleChannel::SetNoiseFloorDbm (double noiseDbm)
{
  m_noiseFloorDbm = noiseDbm;
}

double
RealisticBleChannel::CalculatePathLoss (double distance) const
{
  // Log-distance path loss model: PL(d) = PL(d0) + 10*n*log10(d/d0)
  if (distance < m_referenceDistance)
    {
      distance = m_referenceDistance;
    }

  return m_referenceLoss + 10.0 * m_pathLossExponent *
         std::log10 (distance / m_referenceDistance);
}

double
RealisticBleChannel::CalculateShadowing () const
{
  // Log-normal shadowing (in dB)
  return m_shadowingRng->GetValue ();
}

double
RealisticBleChannel::CalculateFading () const
{
  if (!m_fadingEnabled)
    {
      return 0.0;
    }

  // Rayleigh fading: exponentially distributed power with mean 1
  // Convert to dB: 10*log10(X) where X ~ Exp(1)
  double linearFading = m_fadingRng->GetValue ();
  return 10.0 * std::log10 (linearFading);
}

double
RealisticBleChannel::CalculateRssi (double txPowerDbm, double distance) const
{
  double pathLoss = CalculatePathLoss (distance);
  double shadowing = CalculateShadowing ();
  double fading = CalculateFading ();

  double rssi = txPowerDbm - pathLoss - shadowing - fading;

  NS_LOG_DEBUG ("Distance=" << distance << "m, PathLoss=" << pathLoss
                << "dB, Shadowing=" << shadowing << "dB, Fading=" << fading
                << "dB, RSSI=" << rssi << "dBm");

  return rssi;
}

void
RealisticBleChannel::Transmit (uint32_t senderId, Ptr<Packet> packet, double txPowerDbm)
{
  g_stats.totalTransmissions++;

  auto senderIt = m_nodes.find (senderId);
  if (senderIt == m_nodes.end ())
    {
      return;
    }

  Ptr<PhysicalSimNode> sender = senderIt->second;
  Vector senderPos = sender->GetPosition ();

  // Log transmission event
  Ptr<Packet> logCopy = packet->Copy ();
  BleDiscoveryHeaderWrapper header;
  logCopy->RemoveHeader (header);

  std::vector<uint32_t> path = header.GetPath ();
  uint32_t originatorId = path.empty () ? senderId : path.front ();

  GpsCoordinate senderGps = sender->GetGpsLocation ();

  g_traceFile << Simulator::Now ().GetMilliSeconds () << ","
              << "SEND" << ","
              << senderId << ","
              << "" << ","
              << originatorId << ","
              << (uint32_t)header.GetTtl () << ","
              << path.size () << ","
              << "" << ","
              << senderGps.latitude << ","
              << senderGps.longitude << "\n";

  NS_LOG_INFO ("T=" << Simulator::Now ().GetMilliSeconds ()
               << "ms: Node " << senderId << " transmits (TTL="
               << (uint32_t)header.GetTtl () << ", pathLen=" << path.size () << ")");

  // Broadcast to all other nodes
  for (const auto &entry : m_nodes)
    {
      uint32_t receiverId = entry.first;
      if (receiverId == senderId)
        {
          continue; // Don't receive own transmission
        }

      Ptr<PhysicalSimNode> receiver = entry.second;
      Vector receiverPos = receiver->GetPosition ();

      // Calculate Euclidean distance for RSSI
      double distance = CalculateDistance (senderPos, receiverPos);

      // Calculate received signal strength
      double rssi = CalculateRssi (txPowerDbm, distance);

      // Check if signal is above receiver sensitivity (use fixed threshold)
      double rxSensitivity = -90.0; // Default BLE receiver sensitivity
      if (rssi < rxSensitivity)
        {
          NS_LOG_DEBUG ("Packet dropped: RSSI " << rssi << " dBm below sensitivity");
          g_stats.droppedDueToDistance++;
          g_stats.totalPacketsLost++;

          // Log packet loss event
          GpsCoordinate receiverGps = receiver->GetGpsLocation ();
          g_traceFile << Simulator::Now ().GetMilliSeconds () << ","
                      << "PACKET_LOSS" << ","
                      << senderId << ","
                      << receiverId << ","
                      << originatorId << ","
                      << "" << ","  // ttl
                      << "" << ","  // path_length
                      << (int)rssi << ","
                      << receiverGps.latitude << ","
                      << receiverGps.longitude << "\n";
          continue;
        }

      // Check if signal is above noise floor
      if (rssi < m_noiseFloorDbm)
        {
          NS_LOG_DEBUG ("Packet dropped: RSSI " << rssi << " dBm below noise floor");
          g_stats.droppedDueToFading++;
          g_stats.totalPacketsLost++;

          // Log packet loss event
          GpsCoordinate receiverGps = receiver->GetGpsLocation ();
          g_traceFile << Simulator::Now ().GetMilliSeconds () << ","
                      << "PACKET_LOSS" << ","
                      << senderId << ","
                      << receiverId << ","
                      << originatorId << ","
                      << "" << ","  // ttl
                      << "" << ","  // path_length
                      << (int)rssi << ","
                      << receiverGps.latitude << ","
                      << receiverGps.longitude << "\n";
          continue;
        }

      // Calculate propagation delay (speed of light + random jitter)
      double propDelay = distance / 3e8 * 1000.0; // milliseconds
      propDelay += m_propDelayRng->GetValue (); // Add random jitter

      // Schedule packet delivery
      Ptr<Packet> rxPacket = packet->Copy ();
      Simulator::Schedule (MilliSeconds (propDelay),
                           &RealisticBleChannel::Deliver,
                           this,
                           receiverId,
                           rxPacket,
                           rssi);

      g_stats.successfulReceptions++;
    }
}

void
RealisticBleChannel::Deliver (uint32_t receiverId, Ptr<Packet> packet, double rssiDbm)
{
  auto it = m_nodes.find (receiverId);
  if (it == m_nodes.end ())
    {
      return;
    }

  Ptr<PhysicalSimNode> receiver = it->second;

  // Log reception event
  Ptr<Packet> logCopy = packet->Copy ();
  BleDiscoveryHeaderWrapper header;
  logCopy->RemoveHeader (header);

  std::vector<uint32_t> path = header.GetPath ();
  uint32_t originatorId = path.empty () ? 0 : path.front ();

  GpsCoordinate receiverGps = receiver->GetGpsLocation ();

  g_traceFile << Simulator::Now ().GetMilliSeconds () << ","
              << "RECV" << ","
              << "" << ","
              << receiverId << ","
              << originatorId << ","
              << (uint32_t)header.GetTtl () << ","
              << path.size () << ","
              << (int)rssiDbm << ","
              << receiverGps.latitude << ","
              << receiverGps.longitude << "\n";

  NS_LOG_INFO ("T=" << Simulator::Now ().GetMilliSeconds ()
               << "ms: Node " << receiverId << " receives from path originator "
               << originatorId << " (RSSI=" << rssiDbm << " dBm)");

  receiver->ReceivePacket (packet, rssiDbm);
}

TypeId
PhysicalSimNode::GetTypeId (void)
{
  static TypeId tid = TypeId ("PhysicalSimNode")
    .SetParent<Object> ()
    .SetGroupName ("BleMeshDiscovery");
  return tid;
}

PhysicalSimNode::PhysicalSimNode ()
  : m_engine (CreateObject<BleDiscoveryEngineWrapper> ()),
    m_channel (nullptr),
    m_nodeId (0),
    m_position (0, 0, 0),
    m_txPowerDbm (0.0),
    m_rxSensitivityDbm (-90.0),
    m_clockDriftPpm (0.0),
    m_started (false)
{
  m_timingJitterRng = CreateObject<NormalRandomVariable> ();
  m_timingJitterRng->SetAttribute ("Mean", DoubleValue (0.0));
  m_timingJitterRng->SetAttribute ("Variance", DoubleValue (0.5 * 0.5)); // ±0.5ms std dev
}

void
PhysicalSimNode::Configure (uint32_t nodeId,
                            const GpsCoordinate &gpsLocation,
                            const Vector &nsPosition,
                            Time slotDuration,
                            uint8_t initialTtl,
                            double proximityThreshold,
                            Ptr<RealisticBleChannel> channel,
                            double txPowerDbm,
                            double rxSensitivityDbm)
{
  NS_ABORT_MSG_IF (m_started, "Configure must be called before Start");

  m_nodeId = nodeId;
  m_gpsLocation = gpsLocation;
  m_position = nsPosition;
  m_channel = channel;
  m_txPowerDbm = txPowerDbm;
  m_rxSensitivityDbm = rxSensitivityDbm;

  m_engine->SetAttribute ("NodeId", UintegerValue (nodeId));
  m_engine->SetAttribute ("SlotDuration", TimeValue (slotDuration));
  m_engine->SetAttribute ("InitialTtl", UintegerValue (initialTtl));

  m_engine->SetSendCallback (MakeCallback (&PhysicalSimNode::HandleEngineSend, this));

  NS_ABORT_MSG_IF (!m_engine->Initialize (), "Failed to initialize engine for node " << nodeId);

  NS_LOG_INFO ("Node " << nodeId << " configured at GPS ("
               << gpsLocation.latitude << ", " << gpsLocation.longitude
               << "), position (" << nsPosition.x << ", " << nsPosition.y << ")");
}

void
PhysicalSimNode::Start ()
{
  NS_ABORT_MSG_IF (m_started, "Node already started");
  m_started = true;
  m_startTime = Simulator::Now ();

  // Apply random initial timing offset (simulates unsynchronized nodes)
  Time initialOffset = MilliSeconds (m_timingJitterRng->GetValue ());
  Simulator::Schedule (initialOffset, &BleDiscoveryEngineWrapper::Start, m_engine);

  NS_LOG_INFO ("Node " << m_nodeId << " starting with "
               << initialOffset.GetMilliSeconds () << "ms offset");
}

void
PhysicalSimNode::ReceivePacket (Ptr<Packet> packet, double rssiDbm)
{
  BleDiscoveryHeaderWrapper header;
  packet->RemoveHeader (header);

  // Convert RSSI to int8_t for engine
  int8_t rssi = ConvertDbmToInt8 (rssiDbm);

  m_engine->Receive (header, rssi);
}

const ble_mesh_node_t*
PhysicalSimNode::GetNodeState () const
{
  return m_engine->GetNode ();
}

Time
PhysicalSimNode::GetLocalTime () const
{
  Time elapsed = Simulator::Now () - m_startTime;

  // Apply clock drift: local_time = real_time * (1 + drift)
  double driftFactor = 1.0 + (m_clockDriftPpm / 1e6);
  Time localElapsed = NanoSeconds (elapsed.GetNanoSeconds () * driftFactor);

  return m_startTime + localElapsed;
}

void
PhysicalSimNode::SetClockDrift (double ppm)
{
  m_clockDriftPpm = ppm;
}

void
PhysicalSimNode::HandleEngineSend (Ptr<Packet> packet)
{
  NS_ABORT_MSG_IF (!m_channel, "Channel not bound");

  // Apply timing error to transmission
  Time sendDelay = ApplyTimingError (MilliSeconds (0));

  Ptr<Packet> txPacket = packet->Copy ();
  Simulator::Schedule (sendDelay,
                       &RealisticBleChannel::Transmit,
                       m_channel,
                       m_nodeId,
                       txPacket,
                       m_txPowerDbm);
}

Time
PhysicalSimNode::ApplyTimingError (Time nominalTime)
{
  // Add Gaussian jitter to transmission timing
  double jitterMs = m_timingJitterRng->GetValue ();
  return nominalTime + MilliSeconds (std::abs (jitterMs));
}

int8_t
PhysicalSimNode::ConvertDbmToInt8 (double dbm) const
{
  // Clamp to int8_t range [-128, 127]
  if (dbm > 127.0)
    {
      return 127;
    }
  else if (dbm < -128.0)
    {
      return -128;
    }
  return static_cast<int8_t> (std::round (dbm));
}

int
main (int argc, char *argv[])
{
  uint32_t nodeCount = 4;
  double simDuration = 5.0;
  uint32_t slotDurationMs = 50;
  double areaWidth = 100.0;        // meters - width of deployment area
  double areaHeight = 100.0;       // meters - height of deployment area
  double txPowerDbm = 0.0;         // BLE typical: 0 dBm
  double pathLossExp = 2.5;        // Indoor environment
  double shadowingStdDev = 6.0;    // Moderate shadowing
  bool fadingEnabled = true;
  double clockDriftPpm = 50.0;     // ±50 ppm typical crystal
  std::string traceFile = "physical_simulation_trace_random.csv";
  uint32_t randomSeed = 1;         // Random seed for reproducibility

  // Base GPS coordinate (e.g., Stanford University)
  double baseLatitude = 37.4275;
  double baseLongitude = -122.1697;

  CommandLine cmd;
  cmd.AddValue ("nodes", "Number of simulated nodes", nodeCount);
  cmd.AddValue ("duration", "Simulation duration in seconds", simDuration);
  cmd.AddValue ("slot", "Discovery slot duration in milliseconds", slotDurationMs);
  cmd.AddValue ("areaWidth", "Deployment area width in meters", areaWidth);
  cmd.AddValue ("areaHeight", "Deployment area height in meters", areaHeight);
  cmd.AddValue ("txPower", "Transmit power in dBm", txPowerDbm);
  cmd.AddValue ("pathLoss", "Path loss exponent", pathLossExp);
  cmd.AddValue ("shadowing", "Shadowing standard deviation (dB)", shadowingStdDev);
  cmd.AddValue ("fading", "Enable Rayleigh fading", fadingEnabled);
  cmd.AddValue ("drift", "Clock drift in ppm", clockDriftPpm);
  cmd.AddValue ("trace", "Output trace file path", traceFile);
  cmd.AddValue ("lat", "Base latitude", baseLatitude);
  cmd.AddValue ("lon", "Base longitude", baseLongitude);
  cmd.AddValue ("seed", "Random seed", randomSeed);
  cmd.Parse (argc, argv);

  Time slotDuration = MilliSeconds (slotDurationMs);

  // Set random seed for reproducibility
  RngSeedManager::SetSeed (randomSeed);

  LogComponentEnable ("Phase2PhysicalDiscoverySimRandom", LOG_LEVEL_INFO);

  // Open trace file
  g_traceFile.open (traceFile);
  g_traceFile << "time_ms,event,sender_id,receiver_id,originator_id,ttl,path_length,rssi,latitude,longitude\n";

  // Create realistic channel
  Ptr<RealisticBleChannel> channel = CreateObject<RealisticBleChannel> ();
  channel->SetPathLossExponent (pathLossExp);
  channel->SetReferenceDistance (1.0);
  channel->SetReferenceLoss (40.0);
  channel->SetShadowingStdDev (shadowingStdDev);
  channel->SetFadingEnabled (fadingEnabled);
  channel->SetNoiseFloorDbm (-95.0);

  // Create nodes with RANDOMIZED positions
  std::vector<Ptr<PhysicalSimNode>> nodes;
  nodes.reserve (nodeCount);

  // Conversion factor: approximately 111km per degree latitude, 111km * cos(lat) per degree longitude
  double metersPerDegreeLat = 111000.0;
  double metersPerDegreeLon = 111000.0 * std::cos (baseLatitude * M_PI / 180.0);

  // Random number generators for X and Y positions
  Ptr<UniformRandomVariable> xRng = CreateObject<UniformRandomVariable> ();
  xRng->SetAttribute ("Min", DoubleValue (0.0));
  xRng->SetAttribute ("Max", DoubleValue (areaWidth));

  Ptr<UniformRandomVariable> yRng = CreateObject<UniformRandomVariable> ();
  yRng->SetAttribute ("Min", DoubleValue (0.0));
  yRng->SetAttribute ("Max", DoubleValue (areaHeight));

  NS_LOG_INFO ("Creating " << nodeCount << " nodes with randomized positions in "
               << areaWidth << "m x " << areaHeight << "m area");

  for (uint32_t i = 0; i < nodeCount; ++i)
    {
      // RANDOMIZED Cartesian position
      double x = xRng->GetValue ();
      double y = yRng->GetValue ();
      Vector position (x, y, 0.0);

      // GPS coordinate (offset from base)
      double latOffset = y / metersPerDegreeLat;
      double lonOffset = x / metersPerDegreeLon;
      GpsCoordinate gpsLocation (baseLatitude + latOffset, baseLongitude + lonOffset, 0.0);

      Ptr<PhysicalSimNode> node = CreateObject<PhysicalSimNode> ();
      node->Configure (i + 1, gpsLocation, position, slotDuration,
                       6 /* TTL */, -70.0 /* proximity threshold */,
                       channel, txPowerDbm, -90.0 /* rx sensitivity */);

      // Set random clock drift
      Ptr<UniformRandomVariable> driftRng = CreateObject<UniformRandomVariable> ();
      driftRng->SetAttribute ("Min", DoubleValue (-clockDriftPpm));
      driftRng->SetAttribute ("Max", DoubleValue (clockDriftPpm));
      node->SetClockDrift (driftRng->GetValue ());

      channel->AddNode (node);
      nodes.push_back (node);

      // Log topology
      g_traceFile << "0,TOPOLOGY," << (i + 1) << ",,"
                  << gpsLocation.latitude << "," << gpsLocation.longitude
                  << ",,," << position.x << "," << position.y << "\n";

      NS_LOG_INFO ("Node " << (i + 1) << " placed at (" << x << ", " << y << ")");
    }

  // Start all nodes
  for (const auto &node : nodes)
    {
      node->Start ();
    }

  Simulator::Stop (Seconds (simDuration));
  Simulator::Run ();

  // Collect statistics
  uint32_t totalSent = 0;
  uint32_t totalReceived = 0;
  uint32_t totalForwarded = 0;
  uint32_t totalDropped = 0;

  for (const auto &node : nodes)
    {
      const ble_mesh_node_t *state = node->GetNodeState ();

      totalSent += state->stats.messages_sent;
      totalReceived += state->stats.messages_received;
      totalForwarded += state->stats.messages_forwarded;
      totalDropped += state->stats.messages_dropped;

      GpsCoordinate gps = node->GetGpsLocation ();

      g_traceFile << Simulator::Now ().GetMilliSeconds () << ","
                  << "STATS" << ","
                  << node->GetNodeId () << ","
                  << state->stats.messages_sent << ","
                  << state->stats.messages_received << ","
                  << state->stats.messages_forwarded << ","
                  << state->stats.messages_dropped << ","
                  << "" << ","
                  << gps.latitude << ","
                  << gps.longitude << "\n";

      NS_LOG_INFO ("Node " << node->GetNodeId () << " final stats:"
                   << " sent=" << state->stats.messages_sent
                   << " recv=" << state->stats.messages_received
                   << " fwd=" << state->stats.messages_forwarded
                   << " drop=" << state->stats.messages_dropped
                   << " GPS=(" << gps.latitude << "," << gps.longitude << ")"
                   << " Pos=(" << node->GetPosition ().x << "," << node->GetPosition ().y << ")");
    }

  // Write channel statistics
  g_traceFile << Simulator::Now ().GetMilliSeconds () << ","
              << "CHANNEL_STATS" << ","
              << g_stats.totalTransmissions << ","
              << g_stats.successfulReceptions << ","
              << g_stats.collisions << ","
              << g_stats.droppedDueToDistance << ","
              << g_stats.droppedDueToFading << ",,,\n";

  g_traceFile.close ();

  // Print summary
  NS_LOG_INFO ("=== Simulation Summary ===");
  NS_LOG_INFO ("Random Seed: " << randomSeed);
  NS_LOG_INFO ("Deployment Area: " << areaWidth << "m x " << areaHeight << "m");
  NS_LOG_INFO ("Total messages sent: " << totalSent);
  NS_LOG_INFO ("Total messages received: " << totalReceived);
  NS_LOG_INFO ("Total messages forwarded: " << totalForwarded);
  NS_LOG_INFO ("Total messages dropped: " << totalDropped);
  NS_LOG_INFO ("Channel transmissions: " << g_stats.totalTransmissions);
  NS_LOG_INFO ("Successful receptions: " << g_stats.successfulReceptions);
  NS_LOG_INFO ("Total packets lost: " << g_stats.totalPacketsLost);
  NS_LOG_INFO ("Dropped (distance): " << g_stats.droppedDueToDistance);
  NS_LOG_INFO ("Dropped (fading): " << g_stats.droppedDueToFading);

  double pdr = 0.0;
  if (g_stats.totalTransmissions > 0)
    {
      pdr = 100.0 * g_stats.successfulReceptions /
            (g_stats.totalTransmissions * (nodeCount - 1));
    }
  NS_LOG_INFO ("Packet Delivery Ratio: " << pdr << "%");
  NS_LOG_INFO ("Trace written to: " << traceFile);

  Simulator::Destroy ();
  return 0;
}
