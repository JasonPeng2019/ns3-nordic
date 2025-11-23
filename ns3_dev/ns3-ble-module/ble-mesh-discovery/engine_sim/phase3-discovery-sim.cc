/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Phase 3 discovery simulation.
 *
 * Builds a virtual BLE mesh using BleDiscoveryEngineWrapper instances that
 * execute the full Phase 3 state machine (noisy window, neighbor discovery,
 * candidacy, election floods, renouncements). The simulated channel simply
 * broadcasts packets to neighbours with delay/attenuation derived from the
 * synthetic GPS layout.
 */

#include "ns3/core-module.h"
#include "ns3/vector.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ble-discovery-engine-wrapper.h"
#include "ns3/ble-discovery-header-wrapper.h"
#include <fstream>

#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include <cmath>

extern "C" {
#include "ns3/ble_mesh_node.h"
#include "ns3/ble_discovery_packet.h"
#include "ns3/ble_discovery_engine.h"
}

using namespace ns3;

/*
 * Global trace file for CSV output (same schema as phase2).
 * Columns: time_ms,event,sender_id,receiver_id,originator_id,ttl,path_length,rssi
 */
static const char TRACE_HEADER[] =
  "time_ms,event,sender_id,receiver_id,originator_id,ttl,path_length,rssi";
static std::ofstream g_traceFile;

// Speed up simulation while keeping the original phase proportions (noise vs neighbour).
static const double PHASE_DURATION_SCALE = 0.25; // 4x faster than default windows
static const Time NOISE_PHASE_DURATION =
  MilliSeconds (PHASE_DURATION_SCALE *
                BLE_ENGINE_DEFAULT_NOISE_SLOTS *
                BLE_ENGINE_DEFAULT_NOISE_SLOT_DURATION_MS);
static const Time NEIGHBOR_PHASE_DURATION =
  MilliSeconds (PHASE_DURATION_SCALE *
                BLE_ENGINE_DEFAULT_NEIGHBOR_SLOTS *
                BLE_ENGINE_DEFAULT_NEIGHBOR_SLOT_DURATION_MS);
static const Time NEIGHBOR_PHASE_START = NOISE_PHASE_DURATION + MilliSeconds (20);

NS_LOG_COMPONENT_DEFINE ("Phase3DiscoveryEngineSim");

class EngineSimNode;

/**
 * \brief Simple in-memory broadcast medium used to wire C engines together.
 *
 * RSSI is derived from sender/receiver distance; links exist only between
 * nodes registered via Connect().
 */
class SimpleVirtualChannel : public Object
{
public:
  static TypeId GetTypeId (void);

  void AddNode (Ptr<EngineSimNode> node);
  void Connect (uint32_t a, uint32_t b);
  void Transmit (uint32_t senderId, Ptr<Packet> packet) const;
  void StartPhaseTraffic (Time noiseDuration,
                          Time neighborDuration,
                          Time sampleInterval) const;

private:
  void Deliver (uint32_t receiverId,
                uint32_t senderId,
                Ptr<Packet> packet,
                int8_t rssi) const;
  void BootstrapLink (uint32_t a, uint32_t b) const;
  int8_t ComputeRssi (uint32_t senderId, uint32_t receiverId) const;
  std::map<uint32_t, Ptr<EngineSimNode>> m_nodes;
  std::map<uint32_t, std::vector<uint32_t>> m_adjacency;
};

/**
 * \brief Helper that wraps a BleDiscoveryEngineWrapper and exposes hooks
 *        for the virtual channel plus environmental controls.
 */
class EngineSimNode : public Object
{
public:
  static TypeId GetTypeId (void);

  EngineSimNode ();

  void Configure (uint32_t nodeId,
                  Time slotDuration,
                  uint8_t initialTtl,
                  double proximityThreshold,
                  Ptr<SimpleVirtualChannel> channel);
  void SetEnvironment (Vector position, double crowding, double noiseLevel);
  void Start ();
  void ReceivePacket (Ptr<Packet> packet, int8_t rssi);
  const ble_mesh_node_t *GetNodeState () const;
  uint32_t GetNodeId () const;
  Vector GetPosition () const;
  void SendBootstrapAdvertisement (Ptr<EngineSimNode> target) const;
  void SendNoiseSample (Ptr<EngineSimNode> target) const;
  void SendNeighborProbe (Ptr<EngineSimNode> target) const;

private:
  void HandleEngineSend (Ptr<Packet> packet);

  Ptr<BleDiscoveryEngineWrapper> m_engine;
  Ptr<SimpleVirtualChannel> m_channel;
  uint32_t m_nodeId;
  bool m_started;
  Vector m_position;
};

/* ---------------- SimpleVirtualChannel ---------------- */

TypeId
SimpleVirtualChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("SimpleVirtualChannel")
    .SetParent<Object> ()
    .SetGroupName ("BleMeshDiscovery");
  return tid;
}

void
SimpleVirtualChannel::AddNode (Ptr<EngineSimNode> node)
{
  NS_ABORT_MSG_IF (!node, "Cannot add null node to virtual channel");
  uint32_t id = node->GetNodeId ();
  NS_ABORT_MSG_IF (id == 0, "Nodes must be configured before linking");
  m_nodes[id] = node;
  m_adjacency[id]; // ensure entry exists
}

void
SimpleVirtualChannel::Connect (uint32_t a, uint32_t b)
{
  NS_ABORT_MSG_IF (m_nodes.find (a) == m_nodes.end (), "Unknown node " << a);
  NS_ABORT_MSG_IF (m_nodes.find (b) == m_nodes.end (), "Unknown node " << b);

  m_adjacency[a].push_back (b);
  m_adjacency[b].push_back (a);

  Simulator::Schedule (NEIGHBOR_PHASE_START,
                       &SimpleVirtualChannel::BootstrapLink,
                       this,
                       a,
                       b);
}

void
SimpleVirtualChannel::Transmit (uint32_t senderId, Ptr<Packet> packet) const
{
  auto it = m_adjacency.find (senderId);
  if (it == m_adjacency.end ())
    {
      return;
    }

  Ptr<Packet> headerCopy = packet->Copy ();
  BleDiscoveryHeaderWrapper header;
  headerCopy->RemoveHeader (header);
  NS_LOG_INFO ("Tx node " << senderId << " TTL=" << (uint32_t)header.GetTtl ()
                          << " pathLen=" << header.GetPath ().size ());
  double nowMs = Simulator::Now ().GetMilliSeconds ();

  // TRACE: SEND event (broadcast)
  std::vector<uint32_t> path = header.GetPath ();
  uint32_t originatorId = path.empty () ? senderId : path.front ();
  g_traceFile << nowMs << ",SEND," << senderId << ","
              << "" << ","  // receiver blank for broadcast
              << originatorId << ","
              << (uint32_t)header.GetTtl () << "," << path.size () << ","
              << "" << "\n";

  for (uint32_t neighbor : it->second)
    {
      Ptr<Packet> copy = packet->Copy ();
      int8_t rssi = ComputeRssi (senderId, neighbor);
      Simulator::Schedule (MilliSeconds (1),
                           &SimpleVirtualChannel::Deliver,
                           this,
                           neighbor,
                           senderId,
                           copy,
                           rssi);
    }
}

void
SimpleVirtualChannel::Deliver (uint32_t receiverId,
                               uint32_t senderId,
                               Ptr<Packet> packet,
                               int8_t rssi) const
{
  auto dstIt = m_nodes.find (receiverId);
  auto srcIt = m_nodes.find (senderId);
  if (dstIt == m_nodes.end () || srcIt == m_nodes.end ())
    {
      return;
    }

  Ptr<Packet> traceCopy = packet->Copy ();
  BleDiscoveryHeaderWrapper header;
  traceCopy->RemoveHeader (header);
  std::vector<uint32_t> path = header.GetPath ();
  uint32_t originatorId = path.empty () ? 0 : path.front ();

  g_traceFile << Simulator::Now ().GetMilliSeconds () << ",RECV,"
              << senderId << "," << receiverId << ","
              << originatorId << ","
              << (uint32_t)header.GetTtl () << ","
              << path.size () << ","
              << (int)rssi << "\n";

  dstIt->second->ReceivePacket (packet, rssi);
}

void
SimpleVirtualChannel::BootstrapLink (uint32_t a, uint32_t b) const
{
  auto nodeA = m_nodes.find (a);
  auto nodeB = m_nodes.find (b);
  if (nodeA == m_nodes.end () || nodeB == m_nodes.end ())
    {
      return;
    }

  nodeA->second->SendBootstrapAdvertisement (nodeB->second);
  nodeB->second->SendBootstrapAdvertisement (nodeA->second);
}

void
SimpleVirtualChannel::StartPhaseTraffic (Time noiseDuration,
                                         Time neighborDuration,
                                         Time sampleInterval) const
{
  NS_ABORT_MSG_IF (sampleInterval.IsZero (), "Sample interval must be > 0");
  for (const auto &entry : m_adjacency)
    {
      uint32_t src = entry.first;
      for (uint32_t dst : entry.second)
        {
          if (src >= dst)
            {
              continue; // handle each pair once
            }

          Ptr<EngineSimNode> nodeA = m_nodes.at (src);
          Ptr<EngineSimNode> nodeB = m_nodes.at (dst);

          for (Time t = Time (0); t < noiseDuration; t += sampleInterval)
            {
              Simulator::Schedule (t,
                                   &EngineSimNode::SendNoiseSample,
                                   nodeA,
                                   nodeB);
              Simulator::Schedule (t,
                                   &EngineSimNode::SendNoiseSample,
                                   nodeB,
                                   nodeA);
            }

          Time neighborStart = noiseDuration;
          Time neighborEnd = noiseDuration + neighborDuration;
          for (Time t = neighborStart; t < neighborEnd; t += sampleInterval)
            {
              Simulator::Schedule (t,
                                   &EngineSimNode::SendNeighborProbe,
                                   nodeA,
                                   nodeB);
              Simulator::Schedule (t,
                                   &EngineSimNode::SendNeighborProbe,
                                   nodeB,
                                   nodeA);
            }
        }
    }
}

int8_t
SimpleVirtualChannel::ComputeRssi (uint32_t senderId, uint32_t receiverId) const
{
  auto dstIt = m_nodes.find (receiverId);
  auto srcIt = m_nodes.find (senderId);
  if (dstIt == m_nodes.end () || srcIt == m_nodes.end ())
    {
      return -90;
    }

  Vector dst = dstIt->second->GetPosition ();
  Vector src = srcIt->second->GetPosition ();
  double dx = dst.x - src.x;
  double dy = dst.y - src.y;
  double distance = std::sqrt (dx * dx + dy * dy);
  return static_cast<int8_t> (-40.0 - (distance / 5.0));
}

/* ---------------- EngineSimNode ---------------- */

TypeId
EngineSimNode::GetTypeId (void)
{
  static TypeId tid = TypeId ("EngineSimNode")
    .SetParent<Object> ()
    .SetGroupName ("BleMeshDiscovery");
  return tid;
}

EngineSimNode::EngineSimNode ()
  : m_engine (CreateObject<BleDiscoveryEngineWrapper> ()),
    m_channel (nullptr),
    m_nodeId (0),
    m_started (false),
    m_position (Vector (0.0, 0.0, 0.0))
{
}

void
EngineSimNode::Configure (uint32_t nodeId,
                          Time slotDuration,
                          uint8_t initialTtl,
                          double proximityThreshold,
                          Ptr<SimpleVirtualChannel> channel)
{
  NS_ABORT_MSG_IF (m_started, "Configure must be called before Start");
  NS_ABORT_MSG_IF (nodeId == 0, "NodeId must be non-zero");

  m_nodeId = nodeId;
  m_channel = channel;

  m_engine->SetAttribute ("NodeId", UintegerValue (nodeId));
  m_engine->SetAttribute ("SlotDuration", TimeValue (slotDuration));
  m_engine->SetAttribute ("InitialTtl", UintegerValue (initialTtl));
  m_engine->SetAttribute ("ProximityThreshold", DoubleValue (proximityThreshold));
  m_engine->SetAttribute ("NeighborTimeoutCycles", UintegerValue (50));
  uint32_t slotMs = static_cast<uint32_t> (slotDuration.GetMilliSeconds ());
  slotMs = std::max (1u, slotMs);
  uint32_t noiseSlots = static_cast<uint32_t> (
    std::ceil (static_cast<double> (NOISE_PHASE_DURATION.GetMilliSeconds ()) /
               static_cast<double> (slotMs)));
  uint32_t neighborSlots = static_cast<uint32_t> (
    std::ceil (static_cast<double> (NEIGHBOR_PHASE_DURATION.GetMilliSeconds ()) /
               static_cast<double> (slotMs)));
  noiseSlots = std::max (1u, noiseSlots);
  neighborSlots = std::max (1u, neighborSlots);
  m_engine->SetAttribute ("NoiseSlotCount", UintegerValue (noiseSlots));
  m_engine->SetAttribute ("NeighborSlotCount", UintegerValue (neighborSlots));
  m_engine->SetSendCallback (MakeCallback (&EngineSimNode::HandleEngineSend, this));

  NS_ABORT_MSG_IF (!m_engine->Initialize (), "Failed to initialize engine for node " << nodeId);
}

void
EngineSimNode::SetEnvironment (Vector position, double crowding, double noiseLevel)
{
  m_position = position;
  m_engine->SetGpsLocation (position, true);
  m_engine->SetCrowdingFactor (crowding);
  m_engine->SetNoiseLevel (noiseLevel);
}

void
EngineSimNode::Start ()
{
  NS_ABORT_MSG_IF (m_started, "Node already started");
  m_started = true;
  m_engine->Start ();
}

void
EngineSimNode::ReceivePacket (Ptr<Packet> packet, int8_t rssi)
{
  BleDiscoveryHeaderWrapper header;
  packet->RemoveHeader (header);
  m_engine->Receive (header, rssi);
}

const ble_mesh_node_t *
EngineSimNode::GetNodeState () const
{
  return m_engine->GetNode ();
}

uint32_t
EngineSimNode::GetNodeId () const
{
  return m_nodeId;
}

Vector
EngineSimNode::GetPosition () const
{
  return m_position;
}

void
EngineSimNode::SendBootstrapAdvertisement (Ptr<EngineSimNode> target) const
{
  NS_ASSERT (target);

  BleDiscoveryHeaderWrapper header;
  header.SetSenderId (m_nodeId);
  header.SetTtl (6);
  header.SetClusterheadFlag (false);
  header.SetGpsAvailable (true);
  header.SetGpsLocation (m_position);
  header.AddToPath (m_nodeId);

  Ptr<Packet> pkt = Create<Packet> ();
  pkt->AddHeader (header);

  Vector dst = target->GetPosition ();
  double dx = dst.x - m_position.x;
  double dy = dst.y - m_position.y;
  double distance = std::sqrt (dx * dx + dy * dy);
  int8_t rssi = static_cast<int8_t> (-35.0 - (distance / 5.0));

  target->ReceivePacket (pkt, rssi);
}

void
EngineSimNode::SendNoiseSample (Ptr<EngineSimNode> target) const
{
  NS_ASSERT (target);
  BleDiscoveryHeaderWrapper header;
  header.SetSenderId (m_nodeId);
  header.SetTtl (1);
  header.SetClusterheadFlag (false);
  header.SetGpsAvailable (true);
  header.SetGpsLocation (m_position);
  header.AddToPath (m_nodeId);

  Ptr<Packet> pkt = Create<Packet> ();
  pkt->AddHeader (header);

  Vector dst = target->GetPosition ();
  double dx = dst.x - m_position.x;
  double dy = dst.y - m_position.y;
  double distance = std::sqrt (dx * dx + dy * dy);
  const double baseRssi = -85.0; // every node transmits the same weak signal
  int8_t rssi = static_cast<int8_t> (baseRssi - (distance / 8.0));

  target->ReceivePacket (pkt, rssi);
}

void
EngineSimNode::SendNeighborProbe (Ptr<EngineSimNode> target) const
{
  NS_ASSERT (target);
  BleDiscoveryHeaderWrapper header;
  header.SetSenderId (m_nodeId);
  header.SetTtl (1);
  header.SetClusterheadFlag (false);
  header.SetGpsAvailable (true);
  header.SetGpsLocation (m_position);
  header.AddToPath (m_nodeId);

  Ptr<Packet> pkt = Create<Packet> ();
  pkt->AddHeader (header);

  Vector dst = target->GetPosition ();
  double dx = dst.x - m_position.x;
  double dy = dst.y - m_position.y;
  double distance = std::sqrt (dx * dx + dy * dy);
  int8_t rssi = static_cast<int8_t> (-35.0 - (distance / 8.0));

  target->ReceivePacket (pkt, rssi);
}

void
EngineSimNode::HandleEngineSend (Ptr<Packet> packet)
{
  NS_ABORT_MSG_IF (!m_channel, "Channel not bound");
  m_channel->Transmit (m_nodeId, packet);
}

/* ---------------- Simulation driver ---------------- */

static double
RandomCrowding (uint32_t nodeIdx, uint32_t totalNodes)
{
  double density = std::min (1.0, static_cast<double> (totalNodes) / 200.0);
  double phaseShift = (nodeIdx % 5) * 0.02;
  double base = std::max (0.05, density * 0.2 + phaseShift);
  return std::min (0.3, base);
}

static double
RandomNoise (double crowding)
{
  return 0.05 + (crowding * 0.1);
}

int
main (int argc, char *argv[])
{
  uint32_t nodeCount = 12;
  double simDuration = 12.0;
  Time slotDuration = MilliSeconds (50);
  double areaSize = 200.0;
  double maxRange = 120.0;
  uint32_t seed = 1;
  std::string traceFile = "phase3_trace.csv";

  CommandLine cmd;
  cmd.AddValue ("nodes", "Number of simulated nodes", nodeCount);
  cmd.AddValue ("duration", "Simulation duration in seconds", simDuration);
  cmd.AddValue ("slot", "Discovery slot duration (ms)", slotDuration);
  cmd.AddValue ("area", "Square area size in meters", areaSize);
  cmd.AddValue ("range", "Maximum neighbour range (meters)", maxRange);
  cmd.AddValue ("seed", "Seed for random placement", seed);
  cmd.AddValue ("trace", "CSV trace output for visualization", traceFile);
  cmd.Parse (argc, argv);

  LogComponentEnable ("Phase3DiscoveryEngineSim", LOG_LEVEL_INFO);

  // Open trace (phase2-style)
  g_traceFile.open (traceFile);
  g_traceFile << TRACE_HEADER << "\n";

  Ptr<UniformRandomVariable> rv = CreateObject<UniformRandomVariable> ();
  rv->SetStream (seed);
  rv->SetAttribute ("Min", DoubleValue (0.0));
  rv->SetAttribute ("Max", DoubleValue (1.0));

  Ptr<SimpleVirtualChannel> channel = CreateObject<SimpleVirtualChannel> ();
  std::vector<Ptr<EngineSimNode>> nodes;
  nodes.reserve (nodeCount);
  std::set<std::pair<uint32_t, uint32_t>> loggedEdges;

  for (uint32_t i = 0; i < nodeCount; ++i)
    {
      Ptr<EngineSimNode> node = CreateObject<EngineSimNode> ();
      node->Configure (i + 1, slotDuration, 6 /* TTL */, 5.0 /* proximity */, channel);
      double x = rv->GetValue (0.0, areaSize);
      double y = rv->GetValue (0.0, areaSize);
      double crowding = RandomCrowding (i, nodeCount);
      double noise = RandomNoise (crowding);
      node->SetEnvironment (Vector (x, y, 0.0), crowding, noise);
      channel->AddNode (node);
      nodes.push_back (node);
    }

  auto connectEdge = [&channel, &loggedEdges](uint32_t idA, uint32_t idB) {
    auto key = std::minmax (idA, idB);
    if (loggedEdges.insert (key).second)
      {
        g_traceFile << "0,TOPOLOGY," << key.first << "," << key.second << ",,,,\n";
      }
    channel->Connect (idA, idB);
  };

  // Basic connectivity: ensure ring graph so every node hears someone.
  for (uint32_t i = 0; i < nodeCount; ++i)
    {
      uint32_t next = (i + 1) % nodeCount;
      if (i != next)
        {
          connectEdge (nodes[i]->GetNodeId (), nodes[next]->GetNodeId ());
        }
    }

  // Additional connectivity based on distance threshold.
  for (uint32_t i = 0; i < nodeCount; ++i)
    {
      for (uint32_t j = i + 1; j < nodeCount; ++j)
        {
          Vector a = nodes[i]->GetPosition ();
          Vector b = nodes[j]->GetPosition ();
          double dx = a.x - b.x;
          double dy = a.y - b.y;
          double distance = std::sqrt (dx * dx + dy * dy);
          if (distance <= maxRange)
            {
              connectEdge (nodes[i]->GetNodeId (), nodes[j]->GetNodeId ());
            }
        }
    }

  for (const auto &node : nodes)
    {
      node->Start ();
    }

  // Use a finer sampling interval than the slot duration to produce smoother trace timelines.
  Time traceInterval = std::max (MilliSeconds (5), slotDuration / 4);

  channel->StartPhaseTraffic (NOISE_PHASE_DURATION,
                              NEIGHBOR_PHASE_DURATION,
                              traceInterval);

  Simulator::Stop (Seconds (simDuration));
  Simulator::Run ();

  uint32_t clusterheads = 0;
  uint32_t alignedEdges = 0;
  uint32_t totalForwarders = 0;

  for (const auto &node : nodes)
    {
      const ble_mesh_node_t *state = node->GetNodeState ();
      uint16_t directNeighbors = ble_mesh_node_count_direct_neighbors (state);
      double neighborRatio = BLE_DISCOVERY_MAX_CLUSTER_SIZE > 0
                               ? static_cast<double> (directNeighbors) /
                                 static_cast<double> (BLE_DISCOVERY_MAX_CLUSTER_SIZE)
                               : 0.0;
      double effectiveNoise = std::max (0.1, state->noise_level);
      double ratio = neighborRatio / effectiveNoise;
      uint32_t cyclesSinceHeard = 0;
      if (state->current_cycle >= state->last_candidate_heard_cycle)
        {
          cyclesSinceHeard = state->current_cycle - state->last_candidate_heard_cycle;
        }
      uint32_t requirement = 6;
      if (cyclesSinceHeard == 1)
        {
          requirement = 3;
        }
      else if (cyclesSinceHeard > 1)
        {
          requirement = 1;
        }
      double threshold = (BLE_DISCOVERY_MAX_CLUSTER_SIZE > 0)
                           ? ((double)requirement * (double)requirement) /
                               (0.5 * (double)BLE_DISCOVERY_MAX_CLUSTER_SIZE)
                           : 0.0;

      NS_LOG_INFO ("Node " << node->GetNodeId ()
                           << " state=" << ble_mesh_node_state_name (state->state)
                           << " sent=" << state->stats.messages_sent
                           << " recv=" << state->stats.messages_received
                           << " fwd=" << state->stats.messages_forwarded
                           << " dropped=" << state->stats.messages_dropped
                           << " clusterhead=" << state->clusterhead_id
                           << " directNeighbors=" << directNeighbors
                           << " noise=" << state->noise_level
                           << " ratio=" << ratio
                           << " threshold=" << threshold
                           << " requirement=" << requirement
                           << " cyclesSinceHeard=" << cyclesSinceHeard);

      if (state->stats.messages_forwarded > 0)
        {
          totalForwarders++;
        }

      if (state->state == BLE_NODE_STATE_CLUSTERHEAD)
        {
          clusterheads++;
        }
      else if (state->clusterhead_id != BLE_MESH_INVALID_NODE_ID)
        {
          alignedEdges++;
        }

      // TRACE: STATS event (reuse columns like phase2)
      g_traceFile << Simulator::Now ().GetMilliSeconds () << ","
                  << "STATS" << ","
                  << node->GetNodeId () << ","
                  << state->stats.messages_sent << ","
                  << state->stats.messages_received << ","
                  << state->stats.messages_forwarded << ","
                  << state->stats.messages_dropped << ","
                  << "\n";
    }

  NS_ABORT_MSG_IF (clusterheads == 0,
                   "Phase 3 simulation failed: no clusterheads elected");
  NS_ABORT_MSG_IF (alignedEdges == 0,
                   "Phase 3 simulation failed: no edge aligned to a clusterhead");
  NS_ABORT_MSG_IF (totalForwarders == 0,
                   "Phase 3 simulation failed: no node forwarded packets");

  NS_LOG_INFO ("Phase 3 discovery simulation: "
               << clusterheads << " clusterheads, "
               << alignedEdges << " edges aligned, "
              << totalForwarders << " forwarders observed.");

  Simulator::Destroy ();
  return 0;
}
