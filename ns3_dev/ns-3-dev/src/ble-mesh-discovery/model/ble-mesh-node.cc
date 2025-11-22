/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025
 *
 * Author: Benjamin Huh <buh07@github>
 *
 * BLE Mesh Node - Main protocol coordinator implementation
 */

#include "ble-mesh-node.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BleMeshNode");
NS_OBJECT_ENSURE_REGISTERED (BleMeshNode);

TypeId
BleMeshNode::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BleMeshNode")
    .SetParent<Object> ()
    .SetGroupName ("BleMeshDiscovery")
    .AddConstructor<BleMeshNode> ()
    .AddAttribute ("InitialTtl",
                   "Initial TTL for discovery messages",
                   UintegerValue (10),
                   MakeUintegerAccessor (&BleMeshNode::m_initialTtl),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("ProximityThreshold",
                   "GPS proximity threshold for forwarding (meters)",
                   DoubleValue (10.0),
                   MakeDoubleAccessor (&BleMeshNode::m_proximityThreshold),
                   MakeDoubleChecker<double> (0.0))
    .AddAttribute ("CandidacyThreshold",
                   "Minimum neighbors to become clusterhead candidate",
                   UintegerValue (10),
                   MakeUintegerAccessor (&BleMeshNode::m_candidacyThreshold),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("StateChange",
                     "Node state changed",
                     MakeTraceSourceAccessor (&BleMeshNode::m_stateChangeTrace),
                     "ns3::BleMeshNode::StateChangeCallback")
  ;
  return tid;
}

BleMeshNode::BleMeshNode ()
  : m_nodeId (0),
    m_state (BLE_STATE_DISCOVERY),
    m_clusterheadId (0),
    m_initialTtl (10),
    m_proximityThreshold (10.0),
    m_candidacyThreshold (10),
    m_messagesSent (0),
    m_messagesReceived (0),
    m_messagesForwarded (0),
    m_messagesDropped (0)
{
  NS_LOG_FUNCTION (this);

  // Create protocol components
  m_cycle = CreateObject<BleDiscoveryCycle> ();
  m_queue = CreateObject<BleMessageQueue> ();
  m_forwarding = CreateObject<BleForwardingLogic> ();
  m_election = CreateObject<BleElection> ();  // Phase 3
}

BleMeshNode::~BleMeshNode ()
{
  NS_LOG_FUNCTION (this);
}

void
BleMeshNode::Initialize (uint32_t nodeId, Ptr<Node> node)
{
  NS_LOG_FUNCTION (this << nodeId << node);

  m_nodeId = nodeId;
  m_node = node;

  // Get mobility model for GPS location
  m_mobility = node->GetObject<MobilityModel> ();
  if (!m_mobility)
    {
      NS_LOG_WARN ("No mobility model found on node " << nodeId);
    }

  // Set up discovery cycle callbacks
  m_cycle->SetSlot0Callback (MakeCallback (&BleMeshNode::OnSlot0, this));
  m_cycle->SetForwardingSlotCallback (1, MakeCallback (&BleMeshNode::OnSlot1, this));
  m_cycle->SetForwardingSlotCallback (2, MakeCallback (&BleMeshNode::OnSlot2, this));
  m_cycle->SetForwardingSlotCallback (3, MakeCallback (&BleMeshNode::OnSlot3, this));
  m_cycle->SetCycleCompleteCallback (MakeCallback (&BleMeshNode::OnCycleComplete, this));

  // Configure forwarding logic
  m_forwarding->SetProximityThreshold (m_proximityThreshold);

  NS_LOG_INFO ("Node " << m_nodeId << " initialized");
}

void
BleMeshNode::Start ()
{
  NS_LOG_FUNCTION (this);

  if (m_nodeId == 0)
    {
      NS_LOG_ERROR ("Node not initialized - call Initialize() first");
      return;
    }

  NS_LOG_INFO ("Node " << m_nodeId << " starting discovery protocol");

  // Start discovery cycle
  m_cycle->Start ();
}

void
BleMeshNode::Stop ()
{
  NS_LOG_FUNCTION (this);

  NS_LOG_INFO ("Node " << m_nodeId << " stopping discovery protocol");

  // Stop discovery cycle
  m_cycle->Stop ();

  // Clear queue
  m_queue->Clear ();
}

uint32_t
BleMeshNode::GetNodeId () const
{
  return m_nodeId;
}

BleMeshNodeState
BleMeshNode::GetState () const
{
  return m_state;
}

void
BleMeshNode::SetState (BleMeshNodeState state)
{
  NS_LOG_FUNCTION (this << state);

  if (m_state != state)
    {
      BleMeshNodeState oldState = m_state;
      m_state = state;

      NS_LOG_INFO ("Node " << m_nodeId << " state changed: "
                   << oldState << " -> " << state);

      // Trigger trace
      m_stateChangeTrace (m_nodeId, state);
    }
}

Vector
BleMeshNode::GetLocation () const
{
  if (m_mobility)
    {
      return m_mobility->GetPosition ();
    }
  return Vector (0, 0, 0);
}

void
BleMeshNode::ReceiveMessage (Ptr<Packet> packet, int8_t rssi)
{
  NS_LOG_FUNCTION (this << packet << static_cast<int32_t> (rssi));

  m_messagesReceived++;

  // Store RSSI for crowding factor calculation (Phase 3)
  m_election->AddRssiSample (rssi);
  m_election->RecordMessageReceived ();

  // Parse header
  BleDiscoveryHeaderWrapper header;
  Ptr<Packet> packetCopy = packet->Copy ();
  packetCopy->RemoveHeader (header);

  NS_LOG_DEBUG ("Node " << m_nodeId << " received message from "
                << header.GetSenderId () << " (RSSI=" << static_cast<int32_t> (rssi)
                << " dBm, TTL=" << static_cast<uint32_t> (header.GetTtl ()) << ")");

  // Process the message
  ProcessDiscoveryMessage (header, rssi);

  // Try to enqueue for forwarding (queue handles deduplication and loop detection)
  bool enqueued = m_queue->Enqueue (packet, header, m_nodeId);

  if (enqueued)
    {
      NS_LOG_DEBUG ("Message enqueued for potential forwarding");
    }
  else
    {
      NS_LOG_DEBUG ("Message not enqueued (duplicate, loop, or overflow)");
      m_messagesDropped++;
    }
}

uint32_t
BleMeshNode::GetDirectNeighborCount () const
{
  return m_election->CountDirectConnections ();
}

double
BleMeshNode::GetCrowdingFactor () const
{
  return m_election->CalculateCrowding ();
}

void
BleMeshNode::SetTransmitCallback (TransmitCallback callback)
{
  NS_LOG_FUNCTION (this);
  m_transmitCallback = callback;
}

// ===== Discovery Cycle Callbacks =====

void
BleMeshNode::OnSlot0 ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("Node " << m_nodeId << " - Slot 0: Sending own discovery message");

  SendDiscoveryMessage ();
}

void
BleMeshNode::OnSlot1 ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("Node " << m_nodeId << " - Slot 1: Forwarding queued messages");

  ForwardQueuedMessage ();
}

void
BleMeshNode::OnSlot2 ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("Node " << m_nodeId << " - Slot 2: Forwarding queued messages");

  ForwardQueuedMessage ();
}

void
BleMeshNode::OnSlot3 ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("Node " << m_nodeId << " - Slot 3: Forwarding queued messages");

  ForwardQueuedMessage ();
}

void
BleMeshNode::OnCycleComplete ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("Node " << m_nodeId << " - Cycle complete");

  // Clean old RSSI samples
  // TODO: Implement time-based cleanup

  // Clean old seen messages from queue
  m_queue->CleanOldEntries (Seconds (60.0));

  // Check if should transition to candidate state
  if (m_state == BLE_STATE_DISCOVERY && ShouldBecomeCandidate ())
    {
      SetState (BLE_STATE_CLUSTERHEAD_CANDIDATE);
    }
}

// ===== Message Transmission =====

void
BleMeshNode::SendDiscoveryMessage ()
{
  NS_LOG_FUNCTION (this);

  // Create discovery message
  BleDiscoveryHeaderWrapper header;
  header.SetSenderId (m_nodeId);
  header.SetTtl (m_initialTtl);
  header.AddToPath (m_nodeId);

  // Add GPS if available
  if (m_mobility)
    {
      Vector location = m_mobility->GetPosition ();
      header.SetGpsLocation (location);
      NS_LOG_DEBUG ("Added GPS location: " << location);
    }

  // Create packet
  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (header);

  m_messagesSent++;

  NS_LOG_INFO ("Node " << m_nodeId << " sending discovery message (TTL="
               << static_cast<uint32_t> (m_initialTtl) << ")");

  // Transmit via callback to lower layer
  if (!m_transmitCallback.IsNull ())
    {
      m_transmitCallback (packet, m_nodeId);
    }
  else
    {
      NS_LOG_WARN ("No transmit callback set!");
    }
}

void
BleMeshNode::ForwardQueuedMessage ()
{
  NS_LOG_FUNCTION (this);

  if (m_queue->IsEmpty ())
    {
      NS_LOG_DEBUG ("Queue empty, nothing to forward");
      return;
    }

  // Peek at highest priority message
  BleDiscoveryHeaderWrapper header;
  if (!m_queue->Peek (header))
    {
      return;
    }

  // Get current location and crowding factor
  Vector currentLocation = GetLocation ();
  double crowdingFactor = GetCrowdingFactor ();

  // Check if should forward using all 3 metrics
  bool shouldForward = m_forwarding->ShouldForward (header,
                                                     currentLocation,
                                                     crowdingFactor,
                                                     m_proximityThreshold);

  if (!shouldForward)
    {
      NS_LOG_DEBUG ("Forwarding decision: DROP (failed forwarding criteria)");
      m_messagesDropped++;
      // Remove from queue
      BleDiscoveryHeaderWrapper discardedHeader;
      m_queue->Dequeue (discardedHeader);
      return;
    }

  // Dequeue and forward
  Ptr<Packet> packet = m_queue->Dequeue (header);
  if (!packet)
    {
      return;
    }

  // Decrement TTL and add self to path
  header.DecrementTtl ();
  header.AddToPath (m_nodeId);

  // Update GPS location
  if (m_mobility)
    {
      header.SetGpsLocation (GetLocation ());
    }

  // Recreate packet with updated header
  packet = Create<Packet> ();
  packet->AddHeader (header);

  m_messagesForwarded++;
  m_election->RecordMessageForwarded ();  // Phase 3: Track forwarding success

  NS_LOG_INFO ("Node " << m_nodeId << " forwarding message from "
               << header.GetSenderId () << " (TTL="
               << static_cast<uint32_t> (header.GetTtl ()) << ")");

  // Transmit
  if (!m_transmitCallback.IsNull ())
    {
      m_transmitCallback (packet, m_nodeId);
    }
}

// ===== Message Processing =====

void
BleMeshNode::ProcessDiscoveryMessage (const BleDiscoveryHeaderWrapper& header, int8_t rssi)
{
  NS_LOG_FUNCTION (this << header.GetSenderId () << static_cast<int32_t> (rssi));

  uint32_t senderId = header.GetSenderId ();

  // Update neighbor information
  if (header.IsGpsAvailable ())
    {
      UpdateNeighbor (senderId, header.GetGpsLocation (), rssi);
    }
  else
    {
      UpdateNeighbor (senderId, Vector (0, 0, 0), rssi);
    }

  // If this is an election message, process clusterhead announcement
  if (header.IsElectionMessage ())
    {
      NS_LOG_DEBUG ("Received election announcement from " << senderId);
      // TODO: Process clusterhead election (Phase 3)
    }

  // TODO: Extract connectivity information from path
  // TODO: Update routing tables
}

void
BleMeshNode::UpdateNeighbor (uint32_t nodeId, Vector location, int8_t rssi)
{
  NS_LOG_FUNCTION (this << nodeId << location << static_cast<int32_t> (rssi));

  // Use Phase 3 election module for neighbor tracking
  m_election->UpdateNeighbor (nodeId, location, rssi);

  NS_LOG_DEBUG ("Updated neighbor " << nodeId << " (RSSI=" << static_cast<int32_t> (rssi)
                << " dBm, total neighbors=" << m_election->CountDirectConnections () << ")");
}

bool
BleMeshNode::ShouldBecomeCandidate ()
{
  NS_LOG_FUNCTION (this);

  // Phase 3: Use election module for candidacy determination
  return m_election->ShouldBecomeCandidate ();
}

double
BleMeshNode::CalculateConnectivityScore ()
{
  NS_LOG_FUNCTION (this);

  // Phase 3: Use election module for score calculation
  return m_election->CalculateCandidacyScore ();
}

} // namespace ns3
