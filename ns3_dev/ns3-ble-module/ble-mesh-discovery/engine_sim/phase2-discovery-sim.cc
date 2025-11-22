/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Phase 2 C-engine simulation.
 * Builds a tiny virtual BLE mesh using BleDiscoveryEngineWrapper instances
 * to validate that every node emits its own discovery advert (slot 0)
 * and that forwarding slots propagate packets between neighbors.
 */

#include "ns3/core-module.h"
#include "ns3/ble-discovery-engine-wrapper.h"
#include "ns3/ble-discovery-header-wrapper.h"

#include <map>
#include <vector>

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
                  Ptr<SimpleVirtualChannel> channel);

  void Start ();
  void ReceivePacket (Ptr<Packet> packet, int8_t rssi);
  const ble_mesh_node_t *GetNodeState () const;
  uint32_t GetNodeId () const;

private:
  void HandleEngineSend (Ptr<Packet> packet);

  Ptr<BleDiscoveryEngineWrapper> m_engine;
  Ptr<SimpleVirtualChannel> m_channel;
  uint32_t m_nodeId;
  bool m_started;
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
  it->second->ReceivePacket (packet, -45); // Fixed RSSI for now
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
    m_nodeId (0),
    m_started (false)
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

  NS_LOG_INFO ("Node " << m_nodeId << " broadcast TTL=" << (uint32_t)header.GetTtl ()
                       << " pathLen=" << header.GetPath ().size ());
  m_channel->Transmit (m_nodeId, packet);
}

int
main (int argc, char *argv[])
{
  uint32_t nodeCount = 4;
  double simDuration = 3.0;
  Time slotDuration = MilliSeconds (50);

  CommandLine cmd;
  cmd.AddValue ("nodes", "Number of simulated nodes", nodeCount);
  cmd.AddValue ("duration", "Simulation duration in seconds", simDuration);
  cmd.AddValue ("slot", "Discovery slot duration (ms)", slotDuration);
  cmd.Parse (argc, argv);

  LogComponentEnable ("Phase2DiscoveryEngineSim", LOG_LEVEL_INFO);

  Ptr<SimpleVirtualChannel> channel = CreateObject<SimpleVirtualChannel> ();
  std::vector<Ptr<EngineSimNode>> nodes;
  nodes.reserve (nodeCount);

  for (uint32_t i = 0; i < nodeCount; ++i)
    {
      Ptr<EngineSimNode> node = CreateObject<EngineSimNode> ();
      node->Configure (i + 1, slotDuration, 6 /* TTL */, 5.0 /* proximity */, channel);
      channel->AddNode (node);
      nodes.push_back (node);
    }

  // Simple connected topology with redundant paths to trigger forwarding.
  for (uint32_t i = 1; i < nodeCount; ++i)
    {
      channel->Connect (i, i + 1);
    }
  if (nodeCount >= 4)
    {
      channel->Connect (2, 4); // add a shortcut so node 2 forwards multiple neighbours
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
      NS_LOG_INFO ("Node " << node->GetNodeId ()
                           << " stats => sent: " << state->stats.messages_sent
                           << ", received: " << state->stats.messages_received
                           << ", forwarded: " << state->stats.messages_forwarded
                           << ", dropped: " << state->stats.messages_dropped);
    }

  NS_ABORT_MSG_IF (forwarders == 0, "No node forwarded a discovery message");

  NS_LOG_INFO ("Phase 2 discovery simulation completed. Forwarders observed: " << forwarders);

  Simulator::Destroy ();
  return 0;
}
