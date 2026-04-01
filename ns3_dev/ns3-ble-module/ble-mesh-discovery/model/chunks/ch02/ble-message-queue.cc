/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025 Your Institution
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ble-message-queue.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/vector.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BleMessageQueue");
NS_OBJECT_ENSURE_REGISTERED (BleMessageQueue);

TypeId
BleMessageQueue::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BleMessageQueue")
    .SetParent<Object> ()
    .SetGroupName ("BLE")
    .AddConstructor<BleMessageQueue> ()
  ;
  return tid;
}

BleMessageQueue::BleMessageQueue ()
{
  NS_LOG_FUNCTION (this);
  ble_queue_init (&m_queue);
}

BleMessageQueue::~BleMessageQueue ()
{
  NS_LOG_FUNCTION (this);
}

bool
BleMessageQueue::Enqueue (Ptr<Packet> packet, const BleDiscoveryHeaderWrapper& header, uint32_t nodeId)
{
  NS_LOG_FUNCTION (this << packet << nodeId);

  const ble_discovery_packet_t *cPacketPtr = nullptr;
  ble_discovery_packet_t temp_packet;

  if (header.IsElectionMessage ())
    {
      const ble_election_packet_t& election = header.GetCElectionPacket ();
      cPacketPtr = reinterpret_cast<const ble_discovery_packet_t *> (&election);
    }
  else
    {
      temp_packet = header.GetCPacket ();
      cPacketPtr = &temp_packet;
    }

  // Get current time in milliseconds
  uint32_t current_time_ms = static_cast<uint32_t> (Simulator::Now ().GetMilliSeconds ());

  // Call C core function
  bool result = ble_queue_enqueue (&m_queue, cPacketPtr, nodeId, current_time_ms);

  if (result)
    {
      NS_LOG_DEBUG ("Message enqueued (sender=" << cPacketPtr->sender_id
                    << ", TTL=" << static_cast<uint32_t> (cPacketPtr->ttl)
                    << ", queueSize=" << m_queue.size << ")");
    }
  else
    {
      NS_LOG_DEBUG ("Message rejected");
    }

  return result;
}

Ptr<Packet>
BleMessageQueue::Dequeue (BleDiscoveryHeaderWrapper& header)
{
  NS_LOG_FUNCTION (this);

  ble_election_packet_t c_packet;

  // Call C core function
  if (!ble_queue_dequeue (&m_queue, &c_packet))
    {
      NS_LOG_DEBUG ("Queue empty, nothing to dequeue");
      return nullptr;
    }

  // Create new wrapper with C packet data
  // We need to manually set all fields since there's no SetFromCPacket method
  BleDiscoveryHeaderWrapper newHeader;
  if (c_packet.base.message_type == BLE_MSG_ELECTION_ANNOUNCEMENT)
    {
      newHeader.SetAsElectionMessage ();
      ble_election_packet_t &dst = newHeader.GetCElectionPacketMutable ();
      dst = c_packet;
      newHeader.GetCPacketMutable () = dst.base;
    }
  else
    {
      newHeader.GetCPacketMutable () = c_packet.base;
    }
  header = newHeader;

  // Create packet
  Ptr<Packet> packet = Create<Packet> ();

  NS_LOG_DEBUG ("Message dequeued (sender=" << c_packet.base.sender_id
                << ", TTL=" << static_cast<uint32_t> (c_packet.base.ttl)
                << ", queueSize=" << m_queue.size << ")");

  return packet;
}

bool
BleMessageQueue::Peek (BleDiscoveryHeaderWrapper& header) const
{
  NS_LOG_FUNCTION (this);

  ble_election_packet_t c_packet;

  // Call C core function
  if (!ble_queue_peek (&m_queue, &c_packet))
    {
      return false;
    }

  // Create new wrapper with C packet data
  BleDiscoveryHeaderWrapper newHeader;
  if (c_packet.base.message_type == BLE_MSG_ELECTION_ANNOUNCEMENT)
    {
      newHeader.SetAsElectionMessage ();
      ble_election_packet_t &dst = newHeader.GetCElectionPacketMutable ();
      dst = c_packet;
      newHeader.GetCPacketMutable () = dst.base;
    }
  else
    {
      newHeader.GetCPacketMutable () = c_packet.base;
    }
  header = newHeader;

  return true;
}

bool
BleMessageQueue::IsEmpty () const
{
  return ble_queue_is_empty (&m_queue);
}

uint32_t
BleMessageQueue::GetSize () const
{
  return ble_queue_get_size (&m_queue);
}

void
BleMessageQueue::Clear ()
{
  NS_LOG_FUNCTION (this);
  ble_queue_clear (&m_queue);
  NS_LOG_INFO ("Queue cleared");
}

void
BleMessageQueue::CleanOldEntries (Time maxAge)
{
  NS_LOG_FUNCTION (this << maxAge);

  uint32_t current_time_ms = static_cast<uint32_t> (Simulator::Now ().GetMilliSeconds ());
  uint32_t max_age_ms = static_cast<uint32_t> (maxAge.GetMilliSeconds ());

  ble_queue_clean_old_entries (&m_queue, current_time_ms, max_age_ms);
}

void
BleMessageQueue::GetStatistics (uint32_t& totalEnqueued, uint32_t& totalDequeued,
                                uint32_t& totalDuplicates, uint32_t& totalLoops,
                                uint32_t& totalOverflows) const
{
  ble_queue_get_statistics (&m_queue, &totalEnqueued, &totalDequeued,
                             &totalDuplicates, &totalLoops, &totalOverflows);
}

} // namespace ns3
