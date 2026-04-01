/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Phase 2 C-engine simulation.
 * Builds a tiny virtual BLE mesh using BleDiscoveryEngineWrapper instances
 * to validate that every node emits its own discovery advert (slot 0)
 * and that forwarding slots propagate packets between neighbors.
 *
 * TRACING: This simulation outputs CSV trace data for visualization.
 * Run the companion Python script (visualize_trace.py) to see the results.
 */

#include "ns3/core-module.h"
#include "ns3/ble-discovery-engine-wrapper.h"
#include "ns3/ble-discovery-header-wrapper.h"
#include "ns3/random-variable-stream.h"

#include <map>
#include <vector>
#include <fstream>

/*
 * Global trace file for CSV output.
 * Format: time_ms,event,sender_id,receiver_id,originator_id,ttl,path_length,rssi
 *
 * Events traced:
 *   SEND     - Node broadcasts a packet (either own discovery or forwarded)
 *   RECV     - Node receives a packet from a neighbor
 *   TOPOLOGY - Records the mesh topology connections (logged once at start)
 *   STATS    - Final statistics per node (logged at end)
 */
std::ofstream g_traceFile;

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Phase2DiscoveryEngineSim");

class EngineSimNode;

/**
 * \brief Simple in-memory broadcast medium used to wire C engines together.
 *
 * No PHY/MAC behaviour is emulated; we simply copy the NS-3 Packet carrying
 * the BleDiscoveryHeaderWrapper to every neighbour after a 1 ms delay.
 */
class SimpleVirtualChannel : public Object
{
public:
  static TypeId GetTypeId (void);

  void AddNode (Ptr<EngineSimNode> node);
  void Connect (uint32_t a, uint32_t b);
  void Transmit (uint32_t senderId, Ptr<Packet> packet) const;

private:
  void Deliver (uint32_t receiverId, Ptr<Packet> packet) const;

  std::map<uint32_t, Ptr<EngineSimNode>> m_nodes;
  std::map<uint32_t, std::vector<uint32_t>> m_adjacency;
};

/**
 * \brief Helper that wraps a BleDiscoveryEngineWrapper instance and exposes
 *        hooks for the virtual channel.
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
                  Ptr<SimpleVirtualChannel> channel,
                  Time maxSendOffset);

  void Start ();
  void ReceivePacket (Ptr<Packet> packet, int8_t rssi);
  const ble_mesh_node_t *GetNodeState () const;
  uint32_t GetNodeId () const;

private:
  void HandleEngineSend (Ptr<Packet> packet);

  Ptr<BleDiscoveryEngineWrapper> m_engine;
  Ptr<SimpleVirtualChannel> m_channel;
  Ptr<UniformRandomVariable> m_rng;  // Per-node RNG for random send offsets
  uint32_t m_nodeId;
  bool m_started;
  Time m_maxSendOffset;  // Maximum offset window for randomizing sends
};

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
}

void
SimpleVirtualChannel::Transmit (uint32_t senderId, Ptr<Packet> packet) const
{
  auto it = m_adjacency.find (senderId);
  if (it == m_adjacency.end ())
    {
      return;
    }

  Ptr<Packet> loggerCopy = packet->Copy ();
  BleDiscoveryHeaderWrapper header;
  loggerCopy->RemoveHeader (header);

  /*
   * TRACE: SEND event
   * Logged when a node broadcasts a discovery packet to all neighbors.
   * - sender_id: The node transmitting the packet
   * - originator_id: The node that originally created this discovery packet
   *                  (extracted from the path - first element is the originator)
   * - ttl: Time-to-live remaining for this packet
   * - path_length: Number of hops this packet has traveled so far
   *
   * Note: If path is empty, the sender is the originator (slot 0 self-advertisement).
   */
  std::vector<uint32_t> path = header.GetPath ();
  uint32_t originatorId = path.empty () ? senderId : path.front ();

  g_traceFile << Simulator::Now ().GetMilliSeconds () << ","
              << "SEND" << ","
              << senderId << ","
              << "" << ","  // receiver_id is empty for broadcast
              << originatorId << ","
              << (uint32_t)header.GetTtl () << ","
              << path.size () << ","
              << "" << "\n";  // rssi is empty for sends

  std::ostringstream oss;
  oss << "Sender " << senderId << " -> neighbours ";
  for (uint32_t neighbor : it->second)
    {
      oss << neighbor << " ";
      Ptr<Packet> copy = packet->Copy ();
      Simulator::Schedule (MilliSeconds (1),
                           &SimpleVirtualChannel::Deliver,
                           this,
                           neighbor,
                           copy);
    }
  NS_LOG_INFO (oss.str () << "(TTL=" << (uint32_t)header.GetTtl ()
                          << ", pathLen=" << header.GetPath ().size () << ")");
}

void
SimpleVirtualChannel::Deliver (uint32_t receiverId, Ptr<Packet> packet) const
{
  auto it = m_nodes.find (receiverId);
  if (it == m_nodes.end ())
    {
      return;
    }

  /*
   * TRACE: RECV event
   * Logged when a node receives a discovery packet from a neighbor.
   * - receiver_id: The node receiving the packet
   * - originator_id: The original source of this discovery packet
   * - ttl: Time-to-live remaining (decremented by receiver's engine)
   * - path_length: Number of hops traveled so far
   * - rssi: Received Signal Strength Indicator (-45 dBm fixed in this sim)
   *
   * This event fires BEFORE the engine processes the packet, so the TTL
   * shown is the value as received (before any decrement for forwarding).
   */
  Ptr<Packet> traceCopy = packet->Copy ();
  BleDiscoveryHeaderWrapper header;
  traceCopy->RemoveHeader (header);

  std::vector<uint32_t> path = header.GetPath ();
  uint32_t originatorId = path.empty () ? 0 : path.front ();

  int8_t rssi = -45;  // Fixed RSSI for this simulation

  g_traceFile << Simulator::Now ().GetMilliSeconds () << ","
              << "RECV" << ","
              << "" << ","  // sender_id reconstructed from context in visualization
              << receiverId << ","
              << originatorId << ","
              << (uint32_t)header.GetTtl () << ","
              << path.size () << ","
              << (int)rssi << "\n";

  it->second->ReceivePacket (packet, rssi);
}

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
    m_rng (CreateObject<UniformRandomVariable> ()),
    m_nodeId (0),
    m_started (false),
    m_maxSendOffset (Seconds (0))
{
}

void
EngineSimNode::Configure (uint32_t nodeId,
                          Time slotDuration,
                          uint8_t initialTtl,
                          double proximityThreshold,
                          Ptr<SimpleVirtualChannel> channel,
                          Time maxSendOffset)
{
  NS_ABORT_MSG_IF (m_started, "Configure must be called before Start");
  NS_ABORT_MSG_IF (nodeId == 0, "NodeId must be non-zero");
  NS_ABORT_MSG_IF (maxSendOffset.IsNegative (), "Max send offset cannot be negative");

  m_nodeId = nodeId;
  m_channel = channel;
  m_maxSendOffset = maxSendOffset;

  // Assign a unique stream to this node's RNG to ensure independent random sequences
  m_rng->SetStream (nodeId);

  m_engine->SetAttribute ("NodeId", UintegerValue (nodeId));
  m_engine->SetAttribute ("SlotDuration", TimeValue (slotDuration));
  m_engine->SetAttribute ("InitialTtl", UintegerValue (initialTtl));
  m_engine->SetAttribute ("ProximityThreshold", DoubleValue (proximityThreshold));
  // Keep pre-discovery phases short so nodes advertise within the simulation window.
  m_engine->SetAttribute ("NoiseSlotCount", UintegerValue (1));
  m_engine->SetAttribute ("NoiseSlotDuration", TimeValue (MilliSeconds (10)));
  m_engine->SetAttribute ("NeighborSlotCount", UintegerValue (4));
  m_engine->SetAttribute ("NeighborSlotDuration", TimeValue (MilliSeconds (5)));
  m_engine->SetSendCallback (MakeCallback (&EngineSimNode::HandleEngineSend, this));

  NS_ABORT_MSG_IF (!m_engine->Initialize (), "Failed to initialize engine for node " << nodeId);
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

void
EngineSimNode::HandleEngineSend (Ptr<Packet> packet)
{
  NS_ABORT_MSG_IF (!m_channel, "Channel not bound");
  Ptr<Packet> headerCopy = packet->Copy ();
  BleDiscoveryHeaderWrapper header;
  headerCopy->RemoveHeader (header);

  // Generate a NEW random offset for each send event
  Time sendOffset = NanoSeconds (m_rng->GetInteger (0, m_maxSendOffset.GetNanoSeconds ()));

  NS_LOG_INFO ("Node " << m_nodeId << " queued broadcast TTL=" << (uint32_t)header.GetTtl ()
                       << " pathLen=" << header.GetPath ().size ()
                       << " offset=" << sendOffset.GetMilliSeconds () << "ms");

  Ptr<Packet> delayedPacket = packet->Copy ();
  Simulator::Schedule (sendOffset,
                       &SimpleVirtualChannel::Transmit,
                       m_channel,
                       m_nodeId,
                       delayedPacket);
}

int
main (int argc, char *argv[])
{
  uint32_t nodeCount = 4;
  double simDuration = 3.0;
  uint32_t slotDurationMs = 50;  // Parse as integer milliseconds to avoid Time unit confusion
  uint32_t timeFrames = 4;
  std::string traceFile = "simulation_trace_random.csv";

  CommandLine cmd;
  cmd.AddValue ("nodes", "Number of simulated nodes", nodeCount);
  cmd.AddValue ("duration", "Simulation duration in seconds", simDuration);
  cmd.AddValue ("slot", "Discovery slot duration in milliseconds", slotDurationMs);
  cmd.AddValue ("frames",
                "Number of mini time frames per slot used to stagger node transmissions",
                timeFrames);
  cmd.AddValue ("trace", "Output trace file path", traceFile);
  cmd.Parse (argc, argv);

  // Convert to NS-3 Time after parsing
  Time slotDuration = MilliSeconds (slotDurationMs);
  NS_ABORT_MSG_IF (timeFrames == 0, "frames must be at least 1");
  Time frameDuration = NanoSeconds (slotDuration.GetNanoSeconds () / timeFrames);

  LogComponentEnable ("Phase2DiscoveryEngineSim", LOG_LEVEL_INFO);

  /*
   * TRACE: Open the CSV trace file and write the header.
   * The CSV format allows easy parsing by Python/pandas for visualization.
   */
  g_traceFile.open (traceFile);
  g_traceFile << "time_ms,event,sender_id,receiver_id,originator_id,ttl,path_length,rssi\n";

  Ptr<SimpleVirtualChannel> channel = CreateObject<SimpleVirtualChannel> ();
  std::vector<Ptr<EngineSimNode>> nodes;
  nodes.reserve (nodeCount);

  // Maximum random offset window = frameDuration * (timeFrames - 1)
  // Each node will pick a random offset within [0, maxSendOffset] for each send
  Time maxSendOffset = frameDuration * (timeFrames - 1);

  for (uint32_t i = 0; i < nodeCount; ++i)
    {
      Ptr<EngineSimNode> node = CreateObject<EngineSimNode> ();
      node->Configure (i + 1, slotDuration, 6 /* TTL */, 5.0 /* proximity */, channel, maxSendOffset);
      channel->AddNode (node);
      nodes.push_back (node);
    }

  // Simple connected topology with redundant paths to trigger forwarding.
  // Topology: 1 -- 2 -- 3
  //                |
  //                4
  // This creates a tree with node 2 as a hub that connects to 3 nodes.
  for (uint32_t i = 1; i < nodeCount; ++i)
    {
      channel->Connect (i, i + 1);
    }
  if (nodeCount >= 4)
    {
      channel->Connect (2, 4); // add a shortcut so node 2 forwards to multiple neighbours
    }

  /*
   * TRACE: TOPOLOGY events
   * Log the mesh topology so the visualization can draw the network graph.
   * Each TOPOLOGY line represents a bidirectional link between two nodes.
   * Format: time_ms=0, event=TOPOLOGY, sender_id=nodeA, receiver_id=nodeB
   */
  // Log the linear chain: 1-2, 2-3, 3-4, ...
  for (uint32_t i = 1; i < nodeCount; ++i)
    {
      g_traceFile << "0,TOPOLOGY," << i << "," << (i + 1) << ",,,,\n";
    }
  // Log the shortcut: 2-4
  if (nodeCount >= 4)
    {
      g_traceFile << "0,TOPOLOGY,2,4,,,,\n";
    }

  for (const auto &node : nodes)
    {
      node->Start ();
    }

  Simulator::Stop (Seconds (simDuration));
  Simulator::Run ();

  uint32_t forwarders = 0;
  for (const auto &node : nodes)
    {
      const ble_mesh_node_t *state = node->GetNodeState ();
      NS_ABORT_MSG_IF (state->stats.messages_sent == 0,
                       "Node " << node->GetNodeId () << " never transmitted its own discovery packet");
      if (state->stats.messages_forwarded > 0)
        {
          forwarders++;
        }

      /*
       * TRACE: STATS events
       * Log final statistics for each node at the end of simulation.
       * This allows the visualization to show summary data.
       * Format reuses columns:
       *   - sender_id: node_id
       *   - receiver_id: messages_sent
       *   - originator_id: messages_received
       *   - ttl: messages_forwarded
       *   - path_length: messages_dropped
       */
      g_traceFile << Simulator::Now ().GetMilliSeconds () << ","
                  << "STATS" << ","
                  << node->GetNodeId () << ","
                  << state->stats.messages_sent << ","
                  << state->stats.messages_received << ","
                  << state->stats.messages_forwarded << ","
                  << state->stats.messages_dropped << ","
                  << "\n";

      NS_LOG_INFO ("Node " << node->GetNodeId ()
                           << " stats => sent: " << state->stats.messages_sent
                           << ", received: " << state->stats.messages_received
                           << ", forwarded: " << state->stats.messages_forwarded
                           << ", dropped: " << state->stats.messages_dropped);
    }

  NS_ABORT_MSG_IF (forwarders == 0, "No node forwarded a discovery message");

  NS_LOG_INFO ("Phase 2 discovery simulation completed. Forwarders observed: " << forwarders);

  /*
   * TRACE: Close the trace file.
   */
  g_traceFile.close ();
  NS_LOG_INFO ("Trace data written to: " << traceFile);

  Simulator::Destroy ();
  return 0;
}
