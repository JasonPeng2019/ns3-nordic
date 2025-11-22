/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025 Your Institution
 *
 * Author: Benjamin Huh <buh07@github>
 *
 * BLE Mesh Node - Main protocol coordinator
 */

#ifndef BLE_MESH_NODE_H
#define BLE_MESH_NODE_H

#include "ns3/object.h"
#include "ns3/node.h"
#include "ns3/vector.h"
#include "ns3/traced-callback.h"
#include "ns3/mobility-model.h"
#include "ble-discovery-cycle.h"
#include "ble-message-queue.h"
#include "ble-forwarding-logic.h"
#include "ble-discovery-header-wrapper.h"
#include <map>
#include <vector>

namespace ns3 {

/**
 * \ingroup ble-mesh-discovery
 * \brief Node state in the BLE mesh discovery protocol
 */
enum BleMeshNodeState
{
  BLE_STATE_DISCOVERY,              //!< Discovery phase - learning network topology
  BLE_STATE_EDGE,                   //!< Edge node - assigned to a clusterhead
  BLE_STATE_CLUSTERHEAD_CANDIDATE,  //!< Candidate to become clusterhead
  BLE_STATE_CLUSTERHEAD             //!< Clusterhead - managing a cluster
};

/**
 * \ingroup ble-mesh-discovery
 * \brief Neighbor information for connectivity tracking
 */
struct NeighborInfo
{
  uint32_t nodeId;                  //!< Neighbor node ID
  Vector location;                  //!< Last known GPS location
  int8_t lastRssi;                  //!< Last RSSI measurement (dBm)
  Time lastSeen;                    //!< Last time we heard from this neighbor
  uint32_t messageCount;            //!< Number of messages received from neighbor
};

/**
 * \ingroup ble-mesh-discovery
 * \brief Main BLE Mesh Discovery Node
 *
 * This is the central component that coordinates the entire protocol:
 * - Manages node state machine
 * - Runs discovery cycle (4-slot timing)
 * - Sends discovery messages
 * - Receives and forwards messages using the queue and forwarding logic
 * - Tracks neighbors and connectivity
 * - Performs clusterhead election
 *
 * Integrates:
 * - BleDiscoveryCycle (timing)
 * - BleMessageQueue (message buffering)
 * - BleForwardingLogic (forwarding decisions)
 */
class BleMeshNode : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Constructor
   */
  BleMeshNode ();

  /**
   * \brief Destructor
   */
  virtual ~BleMeshNode ();

  /**
   * \brief Initialize the node and start protocol
   * \param nodeId This node's unique ID
   * \param node The NS-3 Node object (for mobility, etc.)
   */
  void Initialize (uint32_t nodeId, Ptr<Node> node);

  /**
   * \brief Start the discovery protocol
   */
  void Start ();

  /**
   * \brief Stop the discovery protocol
   */
  void Stop ();

  /**
   * \brief Get node ID
   * \return Node ID
   */
  uint32_t GetNodeId () const;

  /**
   * \brief Get current state
   * \return Current node state
   */
  BleMeshNodeState GetState () const;

  /**
   * \brief Set node state
   * \param state New state
   */
  void SetState (BleMeshNodeState state);

  /**
   * \brief Get current GPS location from mobility model
   * \return Current position
   */
  Vector GetLocation () const;

  /**
   * \brief Receive a discovery message (called by lower layer)
   * \param packet The received packet
   * \param rssi Received signal strength indicator (dBm)
   */
  void ReceiveMessage (Ptr<Packet> packet, int8_t rssi);

  /**
   * \brief Get number of direct neighbors (1-hop)
   * \return Number of neighbors
   */
  uint32_t GetDirectNeighborCount () const;

  /**
   * \brief Get crowding factor based on recent RSSI measurements
   * \return Crowding factor (0.0 to 1.0)
   */
  double GetCrowdingFactor () const;

  /**
   * \brief Set callback for message transmission (to lower layer)
   * \param callback Function to call when ready to transmit
   */
  typedef Callback<void, Ptr<Packet>, uint32_t> TransmitCallback;
  void SetTransmitCallback (TransmitCallback callback);

  // Statistics and debugging
  uint32_t GetMessagesSent () const { return m_messagesSent; }
  uint32_t GetMessagesReceived () const { return m_messagesReceived; }
  uint32_t GetMessagesForwarded () const { return m_messagesForwarded; }
  uint32_t GetMessagesDropped () const { return m_messagesDropped; }

private:
  // Discovery cycle callbacks (called at each slot)
  void OnSlot0 ();  //!< Send own discovery message
  void OnSlot1 ();  //!< Forward queued messages (slot 1)
  void OnSlot2 ();  //!< Forward queued messages (slot 2)
  void OnSlot3 ();  //!< Forward queued messages (slot 3)
  void OnCycleComplete (); //!< End of 4-slot cycle

  /**
   * \brief Send a discovery message
   */
  void SendDiscoveryMessage ();

  /**
   * \brief Forward a message from the queue
   */
  void ForwardQueuedMessage ();

  /**
   * \brief Process a received discovery message
   * \param header The discovery message header
   * \param rssi Received signal strength
   */
  void ProcessDiscoveryMessage (const BleDiscoveryHeaderWrapper& header, int8_t rssi);

  /**
   * \brief Update neighbor information
   * \param nodeId Neighbor node ID
   * \param location Neighbor location
   * \param rssi RSSI measurement
   */
  void UpdateNeighbor (uint32_t nodeId, Vector location, int8_t rssi);

  /**
   * \brief Check if node should become clusterhead candidate
   * \return true if candidacy criteria met
   */
  bool ShouldBecomeCandidate ();

  /**
   * \brief Calculate connectivity score for clusterhead election
   * \return Connectivity score
   */
  double CalculateConnectivityScore ();

  // Node identity and state
  uint32_t m_nodeId;                          //!< This node's ID
  Ptr<Node> m_node;                           //!< NS-3 Node object
  BleMeshNodeState m_state;                   //!< Current protocol state
  Ptr<MobilityModel> m_mobility;              //!< Mobility model for GPS

  // Protocol components
  Ptr<BleDiscoveryCycle> m_cycle;             //!< Discovery cycle manager
  Ptr<BleMessageQueue> m_queue;               //!< Message forwarding queue
  Ptr<BleForwardingLogic> m_forwarding;       //!< Forwarding logic

  // Neighbor tracking
  std::map<uint32_t, NeighborInfo> m_neighbors; //!< Neighbor database
  std::vector<int8_t> m_recentRssi;           //!< Recent RSSI samples for crowding
  Time m_rssiWindow;                          //!< Time window for RSSI samples

  // Clusterhead information
  uint32_t m_clusterheadId;                   //!< Assigned clusterhead (if edge node)
  std::vector<uint32_t> m_pathToClusterhead; //!< Path to assigned clusterhead

  // Transmission callback
  TransmitCallback m_transmitCallback;        //!< Callback to lower layer

  // Configuration parameters
  uint8_t m_initialTtl;                       //!< Initial TTL for discovery messages
  double m_proximityThreshold;                //!< GPS proximity threshold (meters)
  uint32_t m_candidacyThreshold;              //!< Min neighbors for candidacy

  // Statistics
  uint32_t m_messagesSent;                    //!< Total messages sent
  uint32_t m_messagesReceived;                //!< Total messages received
  uint32_t m_messagesForwarded;               //!< Total messages forwarded
  uint32_t m_messagesDropped;                 //!< Total messages dropped

  // Traced callbacks for monitoring
  TracedCallback<uint32_t, BleMeshNodeState> m_stateChangeTrace; //!< State change trace
};

} // namespace ns3

#endif /* BLE_MESH_NODE_H */
