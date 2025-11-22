/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025
 *
 * Author: Benjamin Huh <buh07@github>
 *
 * C++ Wrapper Implementation - Thin layer over C node core
 */

#include "ble-mesh-node-wrapper.h"
#include "ns3/log.h"
#include "ns3/boolean.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BleMeshNodeWrapper");

NS_OBJECT_ENSURE_REGISTERED (BleMeshNodeWrapper);

TypeId
BleMeshNodeWrapper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BleMeshNodeWrapper")
    .SetParent<Object> ()
    .SetGroupName ("BleMeshDiscovery")
    .AddConstructor<BleMeshNodeWrapper> ()
    .AddAttribute ("GpsEnabled",
                   "Enable or disable GPS functionality",
                   BooleanValue (true),
                   MakeBooleanAccessor (&BleMeshNodeWrapper::m_gpsEnabled),
                   MakeBooleanChecker ())
    .AddTraceSource ("StateChange",
                     "Trace fired when node state changes",
                     MakeTraceSourceAccessor (&BleMeshNodeWrapper::m_stateChangeTrace),
                     "ns3::BleMeshNodeWrapper::StateChangeCallback")
  ;
  return tid;
}

BleMeshNodeWrapper::BleMeshNodeWrapper ()
  : m_gpsEnabled (true)
{
  NS_LOG_FUNCTION (this);
  // Initialize with invalid ID (will be set by Initialize())
  ble_mesh_node_init (&m_node, BLE_MESH_INVALID_NODE_ID);
}

BleMeshNodeWrapper::~BleMeshNodeWrapper ()
{
  NS_LOG_FUNCTION (this);
}

void
BleMeshNodeWrapper::Initialize (uint32_t nodeId)
{
  NS_LOG_FUNCTION (this << nodeId);
  ble_mesh_node_init (&m_node, nodeId);
}

// ===== GPS Management =====

void
BleMeshNodeWrapper::SetGpsLocation (Vector position)
{
  ble_mesh_node_set_gps (&m_node, position.x, position.y, position.z);
}

Vector
BleMeshNodeWrapper::GetGpsLocation (void) const
{
  return Vector (m_node.gps_location.x,
                 m_node.gps_location.y,
                 m_node.gps_location.z);
}

void
BleMeshNodeWrapper::ClearGps (void)
{
  ble_mesh_node_clear_gps (&m_node);
}

bool
BleMeshNodeWrapper::IsGpsAvailable (void) const
{
  return m_node.gps_available;
}

void
BleMeshNodeWrapper::SetGpsCacheTtl (uint32_t ttl)
{
  ble_mesh_node_set_gps_cache_ttl (&m_node, ttl);
}

bool
BleMeshNodeWrapper::IsGpsCacheValid (void) const
{
  return ble_mesh_node_is_gps_cache_valid (&m_node);
}

void
BleMeshNodeWrapper::InvalidateGpsCache (void)
{
  ble_mesh_node_invalidate_gps_cache (&m_node);
}

uint32_t
BleMeshNodeWrapper::GetGpsAge (void) const
{
  return ble_mesh_node_get_gps_age (&m_node);
}

bool
BleMeshNodeWrapper::UpdateGpsFromMobilityModel (Ptr<MobilityModel> mobilityModel)
{
  if (!m_gpsEnabled || mobilityModel == nullptr)
    {
      return false;
    }

  Vector position = mobilityModel->GetPosition ();
  SetGpsLocation (position);
  return true;
}

// ===== State Management =====

ble_node_state_t
BleMeshNodeWrapper::GetState (void) const
{
  return ble_mesh_node_get_state (&m_node);
}

ble_node_state_t
BleMeshNodeWrapper::GetPreviousState (void) const
{
  return m_node.prev_state;
}

bool
BleMeshNodeWrapper::SetState (ble_node_state_t newState)
{
  ble_node_state_t oldState = m_node.state;
  bool success = ble_mesh_node_set_state (&m_node, newState);

  if (success && oldState != newState)
    {
      NS_LOG_INFO ("Node " << m_node.node_id << " state: "
                   << ble_mesh_node_state_name (oldState) << " -> "
                   << ble_mesh_node_state_name (newState));

      // Fire traced callback
      m_stateChangeTrace (m_node.node_id, oldState, newState);
    }

  return success;
}

std::string
BleMeshNodeWrapper::GetStateName (ble_node_state_t state)
{
  return std::string (ble_mesh_node_state_name (state));
}

std::string
BleMeshNodeWrapper::GetCurrentStateName (void) const
{
  return GetStateName (m_node.state);
}

// ===== Cycle Management =====

void
BleMeshNodeWrapper::AdvanceCycle (void)
{
  ble_mesh_node_advance_cycle (&m_node);
}

uint32_t
BleMeshNodeWrapper::GetCurrentCycle (void) const
{
  return m_node.current_cycle;
}

// ===== Neighbor Management =====

bool
BleMeshNodeWrapper::AddNeighbor (uint32_t neighborId, int8_t rssi, uint8_t hopCount)
{
  return ble_mesh_node_add_neighbor (&m_node, neighborId, rssi, hopCount);
}

bool
BleMeshNodeWrapper::UpdateNeighborGps (uint32_t neighborId, Vector gps)
{
  ble_gps_location_t gps_c;
  gps_c.x = gps.x;
  gps_c.y = gps.y;
  gps_c.z = gps.z;

  return ble_mesh_node_update_neighbor_gps (&m_node, neighborId, &gps_c);
}

uint16_t
BleMeshNodeWrapper::GetNeighborCount (void) const
{
  return m_node.neighbors.count;
}

uint16_t
BleMeshNodeWrapper::GetDirectNeighborCount (void) const
{
  return ble_mesh_node_count_direct_neighbors (&m_node);
}

int8_t
BleMeshNodeWrapper::GetAverageRssi (void) const
{
  return ble_mesh_node_calculate_avg_rssi (&m_node);
}

uint16_t
BleMeshNodeWrapper::PruneStaleNeighbors (uint32_t maxAge)
{
  return ble_mesh_node_prune_stale_neighbors (&m_node, maxAge);
}

// ===== Election & Clustering =====

double
BleMeshNodeWrapper::CalculateCandidacyScore (double noiseLevel)
{
  double score = ble_mesh_node_calculate_candidacy_score (&m_node,
                                                            noiseLevel);
  m_node.candidacy_score = score;
  return score;
}

double
BleMeshNodeWrapper::GetCandidacyScore (void) const
{
  return m_node.candidacy_score;
}

void
BleMeshNodeWrapper::SetCandidacyScore (double score)
{
  m_node.candidacy_score = score;
}

uint32_t
BleMeshNodeWrapper::GetPdsf (void) const
{
  return m_node.pdsf;
}

void
BleMeshNodeWrapper::SetPdsf (uint32_t pdsf)
{
  m_node.pdsf = pdsf;
}

uint32_t
BleMeshNodeWrapper::GetElectionHash (void) const
{
  return m_node.election_hash;
}

bool
BleMeshNodeWrapper::ShouldBecomeEdge (void) const
{
  return ble_mesh_node_should_become_edge (&m_node);
}

bool
BleMeshNodeWrapper::ShouldBecomeCandidate (void) const
{
  return ble_mesh_node_should_become_candidate (&m_node);
}

void
BleMeshNodeWrapper::SetClusterheadId (uint32_t clusterId)
{
  m_node.clusterhead_id = clusterId;
}

uint32_t
BleMeshNodeWrapper::GetClusterheadId (void) const
{
  return m_node.clusterhead_id;
}

void
BleMeshNodeWrapper::SetClusterClass (uint16_t classId)
{
  m_node.cluster_class = classId;
}

uint16_t
BleMeshNodeWrapper::GetClusterClass (void) const
{
  return m_node.cluster_class;
}

// ===== Statistics =====

void
BleMeshNodeWrapper::UpdateStatistics (void)
{
  ble_mesh_node_update_statistics (&m_node);
}

uint32_t
BleMeshNodeWrapper::GetMessagesSent (void) const
{
  return m_node.stats.messages_sent;
}

uint32_t
BleMeshNodeWrapper::GetMessagesReceived (void) const
{
  return m_node.stats.messages_received;
}

uint32_t
BleMeshNodeWrapper::GetMessagesForwarded (void) const
{
  return m_node.stats.messages_forwarded;
}

uint32_t
BleMeshNodeWrapper::GetMessagesDropped (void) const
{
  return m_node.stats.messages_dropped;
}

uint32_t
BleMeshNodeWrapper::GetDiscoveryCycles (void) const
{
  return m_node.stats.discovery_cycles;
}

void
BleMeshNodeWrapper::IncrementSent (void)
{
  ble_mesh_node_inc_sent (&m_node);
}

void
BleMeshNodeWrapper::IncrementReceived (void)
{
  ble_mesh_node_inc_received (&m_node);
}

void
BleMeshNodeWrapper::IncrementForwarded (void)
{
  ble_mesh_node_inc_forwarded (&m_node);
}

void
BleMeshNodeWrapper::IncrementDropped (void)
{
  ble_mesh_node_inc_dropped (&m_node);
}

// ===== Identity =====

uint32_t
BleMeshNodeWrapper::GetNodeId (void) const
{
  return m_node.node_id;
}

} // namespace ns3
