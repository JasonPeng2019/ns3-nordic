/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Hybrid BLE + LoRA simulation skeleton.
 *
 * Wires BleDiscoveryEngineWrapper (existing Phase 3) alongside the placeholder
 * LoraEngineWrapper and routes packets over simple virtual channels. Discovery
 * stays single-channel for BLE; LoRA uses an independent virtual channel.
 */

#include "ns3/core-module.h"
#include "ns3/vector.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ble-discovery-engine-wrapper.h"
#include "ns3/ble-discovery-header-wrapper.h"
#include "ns3/lora-engine-wrapper.h"
#include "ns3/log.h"
#include <fstream>
#include <map>
#include <set>

extern "C" {
#include "ns3/lora_discovery_packet.h"
}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HybridBleLoraSim");

class HybridNode;

class SimpleChannel : public Object
{
public:
  static TypeId GetTypeId (void);
  void AddNode (Ptr<HybridNode> node);
  void Connect (uint32_t a, uint32_t b);
  void TransmitBle (uint32_t senderId, Ptr<Packet> packet) const;
  void TransmitLora (uint32_t senderId, Ptr<Packet> packet) const;

private:
  void DeliverBle (uint32_t receiverId,
                   uint32_t senderId,
                   Ptr<Packet> packet,
                   int8_t rssi) const;
  void DeliverLora (uint32_t receiverId,
                    uint32_t senderId,
                    Ptr<Packet> packet,
                    int8_t rssi) const;
  int8_t ComputeRssi (uint32_t senderId, uint32_t receiverId, double range) const;

  std::map<uint32_t, Ptr<HybridNode>> m_nodes;
  std::map<uint32_t, std::vector<uint32_t>> m_adj;
};

class HybridNode : public Object
{
public:
  static TypeId GetTypeId (void);
  HybridNode ();
  void Configure (uint32_t nodeId,
                  Time slotDuration,
                  uint8_t bleTtl,
                  Ptr<SimpleChannel> channel);
  void SetEnvironment (Vector position);
  void Start ();
  void ReceiveBle (Ptr<Packet> packet, int8_t rssi);
  void ReceiveLora (Ptr<Packet> packet, int8_t rssi);
  uint32_t GetNodeId () const { return m_nodeId; }
  Vector GetPosition () const { return m_position; }

private:
  void HandleBleSend (Ptr<Packet> packet);
  void HandleLoraSend (Ptr<Packet> packet);

  Ptr<BleDiscoveryEngineWrapper> m_ble;
  Ptr<LoraEngineWrapper> m_lora;
  Ptr<SimpleChannel> m_channel;
  uint32_t m_nodeId;
  bool m_started;
  Vector m_position;
};

TypeId
SimpleChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("SimpleChannel")
    .SetParent<Object> ()
    .SetGroupName ("BleMeshDiscovery");
  return tid;
}

void
SimpleChannel::AddNode (Ptr<HybridNode> node)
{
  NS_ABORT_MSG_IF (!node, "null node");
  uint32_t id = node->GetNodeId ();
  m_nodes[id] = node;
  m_adj[id];
}

void
SimpleChannel::Connect (uint32_t a, uint32_t b)
{
  m_adj[a].push_back (b);
  m_adj[b].push_back (a);
}

void
SimpleChannel::TransmitBle (uint32_t senderId, Ptr<Packet> packet) const
{
  auto it = m_adj.find (senderId);
  if (it == m_adj.end ())
    {
      return;
    }
  for (uint32_t dst : it->second)
    {
      Ptr<Packet> copy = packet->Copy ();
      int8_t rssi = ComputeRssi (senderId, dst, 120.0);
      Simulator::Schedule (MilliSeconds (1),
                           &SimpleChannel::DeliverBle,
                           this,
                           dst,
                           senderId,
                           copy,
                           rssi);
    }
}

void
SimpleChannel::TransmitLora (uint32_t senderId, Ptr<Packet> packet) const
{
  auto it = m_adj.find (senderId);
  if (it == m_adj.end ())
    {
      return;
    }
  for (uint32_t dst : it->second)
    {
      Ptr<Packet> copy = packet->Copy ();
      int8_t rssi = ComputeRssi (senderId, dst, 500.0); // LoRA longer range
      Simulator::Schedule (MilliSeconds (5),
                           &SimpleChannel::DeliverLora,
                           this,
                           dst,
                           senderId,
                           copy,
                           rssi);
    }
}

void
SimpleChannel::DeliverBle (uint32_t receiverId,
                           uint32_t senderId,
                           Ptr<Packet> packet,
                           int8_t rssi) const
{
  auto dstIt = m_nodes.find (receiverId);
  if (dstIt == m_nodes.end ())
    {
      return;
    }
  dstIt->second->ReceiveBle (packet, rssi);
}

void
SimpleChannel::DeliverLora (uint32_t receiverId,
                            uint32_t senderId,
                            Ptr<Packet> packet,
                            int8_t rssi) const
{
  auto dstIt = m_nodes.find (receiverId);
  if (dstIt == m_nodes.end ())
    {
      return;
    }
  dstIt->second->ReceiveLora (packet, rssi);
}

int8_t
SimpleChannel::ComputeRssi (uint32_t senderId, uint32_t receiverId, double range) const
{
  auto a = m_nodes.find (senderId);
  auto b = m_nodes.find (receiverId);
  if (a == m_nodes.end () || b == m_nodes.end ())
    {
      return -90;
    }
  Vector pa = a->second->GetPosition ();
  Vector pb = b->second->GetPosition ();
  double dx = pa.x - pb.x;
  double dy = pa.y - pb.y;
  double dist = std::sqrt (dx * dx + dy * dy);
  if (dist > range)
    {
      return -120;
    }
  return static_cast<int8_t> (-40.0 - (dist / (range / 20.0)));
}

HybridNode::HybridNode ()
  : m_ble (CreateObject<BleDiscoveryEngineWrapper> ()),
    m_lora (CreateObject<LoraEngineWrapper> ()),
    m_channel (nullptr),
    m_nodeId (0),
    m_started (false),
    m_position (Vector (0.0, 0.0, 0.0))
{
}

TypeId
HybridNode::GetTypeId (void)
{
  static TypeId tid = TypeId ("HybridNode")
    .SetParent<Object> ()
    .SetGroupName ("BleMeshDiscovery");
  return tid;
}

void
HybridNode::Configure (uint32_t nodeId,
                       Time slotDuration,
                       uint8_t bleTtl,
                       Ptr<SimpleChannel> channel)
{
  m_nodeId = nodeId;
  m_channel = channel;
  m_ble->SetAttribute ("NodeId", UintegerValue (nodeId));
  m_ble->SetAttribute ("SlotDuration", TimeValue (slotDuration));
  m_ble->SetAttribute ("InitialTtl", UintegerValue (bleTtl));
  m_ble->SetSendCallback (MakeCallback (&HybridNode::HandleBleSend, this));
  m_lora->SetAttribute ("NodeId", UintegerValue (nodeId));
  m_lora->SetSendCallback (MakeCallback (&HybridNode::HandleLoraSend, this));
  NS_ABORT_MSG_IF (!m_ble->Initialize (), "BLE init failed");
  NS_ABORT_MSG_IF (!m_lora->Initialize (), "LoRA init failed");
}

void
HybridNode::SetEnvironment (Vector position)
{
  m_position = position;
  m_ble->SetGpsLocation (position, true);
}

void
HybridNode::Start ()
{
  NS_ABORT_MSG_IF (m_started, "already started");
  m_started = true;
  m_ble->Start ();
  m_lora->Start ();
}

void
HybridNode::ReceiveBle (Ptr<Packet> packet, int8_t rssi)
{
  BleDiscoveryHeaderWrapper header;
  packet->RemoveHeader (header);
  m_ble->Receive (header, rssi);
}

void
HybridNode::ReceiveLora (Ptr<Packet> packet, int8_t rssi)
{
  lora_discovery_packet_t raw;
  std::vector<uint8_t> buf(packet->GetSize ());
  packet->CopyData (buf.data (), buf.size ());
  uint32_t read = lora_discovery_deserialize (&raw, buf.data (), buf.size ());
  if (read > 0)
    {
      m_lora->Receive (raw, rssi);
    }
}

void
HybridNode::HandleBleSend (Ptr<Packet> packet)
{
  NS_ABORT_MSG_IF (!m_channel, "no channel");
  m_channel->TransmitBle (m_nodeId, packet);
}

void
HybridNode::HandleLoraSend (Ptr<Packet> packet)
{
  NS_ABORT_MSG_IF (!m_channel, "no channel");
  m_channel->TransmitLora (m_nodeId, packet);
}

int
main (int argc, char *argv[])
{
  uint32_t nodeCount = 6;
  double areaSize = 200.0;
  Time bleSlot = MilliSeconds (50);
  uint8_t bleTtl = 6;

  CommandLine cmd;
  cmd.AddValue ("nodes", "Number of nodes", nodeCount);
  cmd.AddValue ("area", "Square area size (meters)", areaSize);
  cmd.Parse (argc, argv);

  Ptr<UniformRandomVariable> rv = CreateObject<UniformRandomVariable> ();
  rv->SetAttribute ("Min", DoubleValue (0.0));
  rv->SetAttribute ("Max", DoubleValue (areaSize));

  Ptr<SimpleChannel> channel = CreateObject<SimpleChannel> ();
  std::vector<Ptr<HybridNode>> nodes;
  nodes.reserve (nodeCount);
  for (uint32_t i = 0; i < nodeCount; ++i)
    {
      Ptr<HybridNode> n = CreateObject<HybridNode> ();
      n->Configure (i + 1, bleSlot, bleTtl, channel);
      double x = rv->GetValue ();
      double y = rv->GetValue ();
      n->SetEnvironment (Vector (x, y, 0.0));
      channel->AddNode (n);
      nodes.push_back (n);
    }

  // simple ring connectivity
  for (uint32_t i = 0; i < nodeCount; ++i)
    {
      uint32_t next = (i + 1) % nodeCount;
      if (i != next)
        {
          channel->Connect (nodes[i]->GetNodeId (), nodes[next]->GetNodeId ());
        }
    }

  for (auto &n : nodes)
    {
      n->Start ();
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
