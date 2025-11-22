/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025
 *
 * Author: Benjamin Huh <buh07@github>
 *
 * C++ Wrapper Implementation - Thin layer over C protocol core
 */

#include "ble-discovery-header-wrapper.h"
#include "ns3/log.h"
#include "ns3/assert.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BleDiscoveryHeaderWrapper");

NS_OBJECT_ENSURE_REGISTERED (BleDiscoveryHeaderWrapper);

BleDiscoveryHeaderWrapper::BleDiscoveryHeaderWrapper ()
  : m_isElection (false)
{
  NS_LOG_FUNCTION (this);
  ble_discovery_packet_init (&m_packet);
}

BleDiscoveryHeaderWrapper::~BleDiscoveryHeaderWrapper ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
BleDiscoveryHeaderWrapper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BleDiscoveryHeaderWrapper")
    .SetParent<Header> ()
    .SetGroupName ("BleMeshDiscovery")
    .AddConstructor<BleDiscoveryHeaderWrapper> ()
  ;
  return tid;
}

TypeId
BleDiscoveryHeaderWrapper::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void
BleDiscoveryHeaderWrapper::Print (std::ostream &os) const
{
  os << "BleDiscoveryHeader (C-core): ";
  os << "Type=" << (m_isElection ? "ELECTION" : "DISCOVERY") << ", ";
  os << "ID=" << m_packet.sender_id << ", ";
  os << "TTL=" << (uint32_t)m_packet.ttl << ", ";
  os << "ClusterheadFlag=" << (m_packet.is_clusterhead_message ? "true" : "false") << ", ";
  os << "Path=[";
  for (uint16_t i = 0; i < m_packet.path_length; ++i)
    {
      os << m_packet.path[i];
      if (i < m_packet.path_length - 1)
        os << ",";
    }
  os << "], GPS=" << (m_packet.gps_available ? "available" : "unavailable");

  if (m_isElection)
    {
      os << ", ClassID=" << m_election.election.class_id;
      os << ", PDSF=" << m_election.election.pdsf;
      os << ", Score=" << m_election.election.score;
    }
}

uint32_t
BleDiscoveryHeaderWrapper::GetSerializedSize (void) const
{
  if (m_isElection)
    {
      return ble_election_get_size (&m_election);
    }
  else
    {
      return ble_discovery_get_size (&m_packet);
    }
}

void
BleDiscoveryHeaderWrapper::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);

  uint32_t size = GetSerializedSize ();
  uint8_t *buffer = new uint8_t[size];

  uint32_t bytes_written;
  if (m_isElection)
    {
      NS_ABORT_MSG_IF (!m_election.base.is_clusterhead_message,
                       "Election packet missing clusterhead flag");
      bytes_written = ble_election_serialize (&m_election, buffer, size);
    }
  else
    {
      bytes_written = ble_discovery_serialize (&m_packet, buffer, size);
    }

  // Write to NS-3 buffer
  start.Write (buffer, bytes_written);

  delete[] buffer;
}

uint32_t
BleDiscoveryHeaderWrapper::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);

  // Read first byte to determine message type
  uint8_t msg_type = start.PeekU8 ();
  m_isElection = (msg_type == BLE_MSG_ELECTION_ANNOUNCEMENT);

  // Read into temporary buffer
  uint32_t available = start.GetRemainingSize ();
  uint8_t *buffer = new uint8_t[available];
  start.Read (buffer, available);

  uint32_t bytes_read;
  if (m_isElection)
    {
      bytes_read = ble_election_deserialize (&m_election, buffer, available);
      // Sync the base packet reference
      m_packet = m_election.base;
      m_isElection = m_packet.is_clusterhead_message;
    }
  else
    {
      bytes_read = ble_discovery_deserialize (&m_packet, buffer, available);
      m_isElection = m_packet.is_clusterhead_message;
    }

  delete[] buffer;
  return bytes_read;
}

// ===== Discovery Packet Methods =====

bool
BleDiscoveryHeaderWrapper::IsElectionMessage (void) const
{
  return m_isElection;
}

void
BleDiscoveryHeaderWrapper::SetClusterheadFlag (bool isClusterhead)
{
  if (isClusterhead && !m_isElection)
    {
      SetAsElectionMessage ();
      return;
    }

  m_packet.is_clusterhead_message = isClusterhead;
  if (m_isElection)
    {
      m_election.base.is_clusterhead_message = isClusterhead;
      if (!isClusterhead)
        {
          m_isElection = false;
        }
    }
  else
    {
      m_isElection = isClusterhead;
    }
}

bool
BleDiscoveryHeaderWrapper::HasClusterheadFlag (void) const
{
  return m_packet.is_clusterhead_message;
}

void
BleDiscoveryHeaderWrapper::SetSenderId (uint32_t id)
{
  m_packet.sender_id = id;
  if (m_isElection)
    {
      m_election.base.sender_id = id;
    }
}

uint32_t
BleDiscoveryHeaderWrapper::GetSenderId (void) const
{
  return m_packet.sender_id;
}

void
BleDiscoveryHeaderWrapper::SetTtl (uint8_t ttl)
{
  m_packet.ttl = ttl;
  if (m_isElection)
    {
      m_election.base.ttl = ttl;
    }
}

uint8_t
BleDiscoveryHeaderWrapper::GetTtl (void) const
{
  return m_packet.ttl;
}

bool
BleDiscoveryHeaderWrapper::DecrementTtl (void)
{
  bool result = ble_discovery_decrement_ttl (&m_packet);
  if (m_isElection)
    {
      m_election.base.ttl = m_packet.ttl;
    }
  return result;
}

bool
BleDiscoveryHeaderWrapper::AddToPath (uint32_t nodeId)
{
  bool result = ble_discovery_add_to_path (&m_packet, nodeId);
  if (m_isElection && result)
    {
      // Sync election packet
      m_election.base.path[m_election.base.path_length++] = nodeId;
    }
  return result;
}

bool
BleDiscoveryHeaderWrapper::IsInPath (uint32_t nodeId) const
{
  return ble_discovery_is_in_path (&m_packet, nodeId);
}

std::vector<uint32_t>
BleDiscoveryHeaderWrapper::GetPath (void) const
{
  std::vector<uint32_t> path;
  for (uint16_t i = 0; i < m_packet.path_length; ++i)
    {
      path.push_back (m_packet.path[i]);
    }
  return path;
}

void
BleDiscoveryHeaderWrapper::SetGpsLocation (Vector position)
{
  ble_discovery_set_gps (&m_packet, position.x, position.y, position.z);
  if (m_isElection)
    {
      m_election.base.gps_location = m_packet.gps_location;
      m_election.base.gps_available = true;
    }
}

Vector
BleDiscoveryHeaderWrapper::GetGpsLocation (void) const
{
  return Vector (m_packet.gps_location.x,
                 m_packet.gps_location.y,
                 m_packet.gps_location.z);
}

void
BleDiscoveryHeaderWrapper::SetGpsAvailable (bool available)
{
  m_packet.gps_available = available;
  if (m_isElection)
    {
      m_election.base.gps_available = available;
    }
}

bool
BleDiscoveryHeaderWrapper::IsGpsAvailable (void) const
{
  return m_packet.gps_available;
}

// ===== Election Methods =====

void
BleDiscoveryHeaderWrapper::SetAsElectionMessage (void)
{
  m_isElection = true;
  // Copy current packet state first
  ble_discovery_packet_t temp = m_packet;
  // Initialize election packet (sets message type and zeros election fields)
  ble_election_packet_init (&m_election);
  // Restore the base packet fields except message_type
  m_election.base.sender_id = temp.sender_id;
  m_election.base.ttl = temp.ttl;
  m_election.base.path_length = temp.path_length;
  for (uint16_t i = 0; i < temp.path_length; ++i)
    {
      m_election.base.path[i] = temp.path[i];
    }
  m_election.base.gps_available = temp.gps_available;
  m_election.base.gps_location = temp.gps_location;
  m_election.base.is_clusterhead_message = true;
  // Sync m_packet with election base
  m_packet = m_election.base;
  m_packet.is_clusterhead_message = true;
}

void
BleDiscoveryHeaderWrapper::SetClassId (uint16_t classId)
{
  if (!m_isElection) SetAsElectionMessage ();
  m_election.election.class_id = classId;
}

uint16_t
BleDiscoveryHeaderWrapper::GetClassId (void) const
{
  return m_isElection ? m_election.election.class_id : 0;
}

void
BleDiscoveryHeaderWrapper::SetPdsf (uint32_t pdsf)
{
  if (!m_isElection) SetAsElectionMessage ();
  m_election.election.pdsf = pdsf;
}

uint32_t
BleDiscoveryHeaderWrapper::GetPdsf (void) const
{
  return m_isElection ? m_election.election.pdsf : 0;
}

void
BleDiscoveryHeaderWrapper::ResetPdsfHistory (void)
{
  if (!m_isElection)
    {
      return;
    }
  ble_election_pdsf_history_reset (&m_election.election.pdsf_history);
  m_election.election.pdsf = 0;
}

uint32_t
BleDiscoveryHeaderWrapper::UpdatePdsf (uint32_t directConnections, uint32_t alreadyReached)
{
  if (!m_isElection)
    {
      SetAsElectionMessage ();
    }
  return ble_election_update_pdsf (&m_election, directConnections, alreadyReached);
}

std::vector<uint32_t>
BleDiscoveryHeaderWrapper::GetPdsfHopHistory (void) const
{
  std::vector<uint32_t> history;
  if (!m_isElection)
    {
      return history;
    }

  for (uint16_t i = 0; i < m_election.election.pdsf_history.hop_count; ++i)
    {
      history.push_back (m_election.election.pdsf_history.direct_counts[i]);
    }
  return history;
}

void
BleDiscoveryHeaderWrapper::SetScore (double score)
{
  if (!m_isElection) SetAsElectionMessage ();
  m_election.election.score = score;
}

double
BleDiscoveryHeaderWrapper::GetScore (void) const
{
  return m_isElection ? m_election.election.score : 0.0;
}

void
BleDiscoveryHeaderWrapper::SetHash (uint32_t hash)
{
  if (!m_isElection) SetAsElectionMessage ();
  m_election.election.hash = hash;
}

uint32_t
BleDiscoveryHeaderWrapper::GetHash (void) const
{
  return m_isElection ? m_election.election.hash : 0;
}

} // namespace ns3
