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

  
  const ble_discovery_packet_t& c_packet = header.GetCPacket ();

  
  uint32_t current_time_ms = static_cast<uint32_t> (Simulator::Now ().GetMilliSeconds ());

  
  bool result = ble_queue_enqueue (&m_queue, &c_packet, nodeId, current_time_ms);

  if (result)
    {
      NS_LOG_DEBUG ("Message enqueued (sender=" << c_packet.sender_id
                    << ", TTL=" << static_cast<uint32_t> (c_packet.ttl)
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

  ble_discovery_packet_t c_packet;

  
  if (!ble_queue_dequeue (&m_queue, &c_packet))
    {
      NS_LOG_DEBUG ("Queue empty, nothing to dequeue");
      return nullptr;
    }

  
  
  BleDiscoveryHeaderWrapper newHeader;
  newHeader.SetSenderId (c_packet.sender_id);
  newHeader.SetTtl (c_packet.ttl);
  for (uint16_t i = 0; i < c_packet.path_length; ++i)
    {
      newHeader.AddToPath (c_packet.path[i]);
    }
  if (c_packet.gps_available)
    {
      newHeader.SetGpsLocation (Vector (c_packet.gps_location.x,
                                         c_packet.gps_location.y,
                                         c_packet.gps_location.z));
    }
  header = newHeader;

  
  Ptr<Packet> packet = Create<Packet> ();

  NS_LOG_DEBUG ("Message dequeued (sender=" << c_packet.sender_id
                << ", TTL=" << static_cast<uint32_t> (c_packet.ttl)
                << ", queueSize=" << m_queue.size << ")");

  return packet;
}

bool
BleMessageQueue::Peek (BleDiscoveryHeaderWrapper& header) const
{
  NS_LOG_FUNCTION (this);

  ble_discovery_packet_t c_packet;

  
  if (!ble_queue_peek (&m_queue, &c_packet))
    {
      return false;
    }

  
  BleDiscoveryHeaderWrapper newHeader;
  newHeader.SetSenderId (c_packet.sender_id);
  newHeader.SetTtl (c_packet.ttl);
  for (uint16_t i = 0; i < c_packet.path_length; ++i)
    {
      newHeader.AddToPath (c_packet.path[i]);
    }
  if (c_packet.gps_available)
    {
      newHeader.SetGpsLocation (Vector (c_packet.gps_location.x,
                                         c_packet.gps_location.y,
                                         c_packet.gps_location.z));
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

} 
