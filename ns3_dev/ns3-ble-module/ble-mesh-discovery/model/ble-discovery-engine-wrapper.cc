#include "ble-discovery-engine-wrapper.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/double.h"
#include "ns3/integer.h"
#include "ns3/uinteger.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BleDiscoveryEngineWrapper");
NS_OBJECT_ENSURE_REGISTERED (BleDiscoveryEngineWrapper);

TypeId
BleDiscoveryEngineWrapper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BleDiscoveryEngineWrapper")
    .SetParent<Object> ()
    .SetGroupName ("BleMeshDiscovery")
    .AddConstructor<BleDiscoveryEngineWrapper> ()
    .AddAttribute ("SlotDuration",
                   "Discovery slot duration",
                   TimeValue (MilliSeconds (BLE_DISCOVERY_DEFAULT_SLOT_DURATION_MS)),
                   MakeTimeAccessor (&BleDiscoveryEngineWrapper::m_slotDuration),
                   MakeTimeChecker ())
    .AddAttribute ("InitialTtl",
                   "TTL used for locally-originated messages",
                   UintegerValue (BLE_DISCOVERY_DEFAULT_TTL),
                   MakeUintegerAccessor (&BleDiscoveryEngineWrapper::m_initialTtl),
                   MakeUintegerChecker<uint8_t> (1))
    .AddAttribute ("ProximityThreshold",
                   "GPS proximity threshold (meters)",
                   DoubleValue (10.0),
                   MakeDoubleAccessor (&BleDiscoveryEngineWrapper::m_proximityThreshold),
                   MakeDoubleChecker<double> (0.0))
    .AddAttribute ("NodeId",
                   "Unique node identifier",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BleDiscoveryEngineWrapper::m_nodeId),
                   MakeUintegerChecker<uint32_t> (1))
  ;
  return tid;
}

BleDiscoveryEngineWrapper::BleDiscoveryEngineWrapper ()
  : m_slotDuration (MilliSeconds (BLE_DISCOVERY_DEFAULT_SLOT_DURATION_MS)),
    m_initialTtl (BLE_DISCOVERY_DEFAULT_TTL),
    m_proximityThreshold (10.0),
    m_nodeId (0),
    m_initialized (false),
    m_running (false)
{
  NS_LOG_FUNCTION (this);
  ble_engine_config_init (&m_config);
}

BleDiscoveryEngineWrapper::~BleDiscoveryEngineWrapper ()
{
  NS_LOG_FUNCTION (this);
  Stop ();
}

bool
BleDiscoveryEngineWrapper::Initialize (void)
{
  NS_LOG_FUNCTION (this);

  if (m_initialized)
    {
      return true;
    }

  if (m_nodeId == 0)
    {
      NS_LOG_ERROR ("NodeId attribute must be set before Initialize()");
      return false;
    }

  ble_engine_config_init (&m_config);
  m_config.node_id = m_nodeId;
  m_config.slot_duration_ms = static_cast<uint32_t> (m_slotDuration.GetMilliSeconds ());
  m_config.initial_ttl = m_initialTtl;
  m_config.proximity_threshold = m_proximityThreshold;
  m_config.send_cb = &BleDiscoveryEngineWrapper::EngineSendHook;
  m_config.log_cb = &BleDiscoveryEngineWrapper::EngineLogHook;
  m_config.user_context = this;

  if (!ble_engine_init (&m_engine, &m_config))
    {
      NS_LOG_ERROR ("Failed to initialize discovery engine");
      return false;
    }

  m_initialized = true;
  return true;
}

void
BleDiscoveryEngineWrapper::Start (void)
{
  NS_LOG_FUNCTION (this);

  if (!m_initialized && !Initialize ())
    {
      return;
    }

  if (m_running)
    {
      return;
    }

  m_running = true;
  m_tickEvent = Simulator::ScheduleNow (&BleDiscoveryEngineWrapper::RunTick, this);
}

void
BleDiscoveryEngineWrapper::Stop (void)
{
  NS_LOG_FUNCTION (this);
  if (m_running)
    {
      Simulator::Cancel (m_tickEvent);
      m_running = false;
    }
}

void
BleDiscoveryEngineWrapper::SetSendCallback (TxCallback cb)
{
  m_txCallback = cb;
}

void
BleDiscoveryEngineWrapper::Receive (const BleDiscoveryHeaderWrapper& header, int8_t rssi)
{
  if (!m_initialized && !Initialize ())
    {
      return;
    }

  const ble_discovery_packet_t& cPacket = header.GetCPacket ();
  ble_engine_receive_packet (&m_engine,
                             &cPacket,
                             rssi,
                             static_cast<uint32_t> (Simulator::Now ().GetMilliSeconds ()));
}

void
BleDiscoveryEngineWrapper::SetCrowdingFactor (double crowdingFactor)
{
  ble_engine_set_crowding_factor (&m_engine, crowdingFactor);
}

void
BleDiscoveryEngineWrapper::SetNoiseLevel (double noiseLevel)
{
  ble_engine_set_noise_level (&m_engine, noiseLevel);
}

void
BleDiscoveryEngineWrapper::MarkCandidateHeard (void)
{
  ble_engine_mark_candidate_heard (&m_engine);
}

void
BleDiscoveryEngineWrapper::SetGpsLocation (Vector location, bool valid)
{
  ble_engine_set_gps (&m_engine, location.x, location.y, location.z, valid);
}

void
BleDiscoveryEngineWrapper::SeedRandom (uint32_t seed)
{
  ble_engine_seed_random (seed);
}

const ble_mesh_node_t*
BleDiscoveryEngineWrapper::GetNode (void) const
{
  return ble_engine_get_node (&m_engine);
}

void
BleDiscoveryEngineWrapper::DoDispose ()
{
  Stop ();
  Object::DoDispose ();
}

void
BleDiscoveryEngineWrapper::ScheduleNextTick (void)
{
  if (!m_running)
    {
      return;
    }

  m_tickEvent = Simulator::Schedule (m_slotDuration,
                                     &BleDiscoveryEngineWrapper::RunTick,
                                     this);
}

void
BleDiscoveryEngineWrapper::RunTick (void)
{
  ble_engine_tick (&m_engine,
                   static_cast<uint32_t> (Simulator::Now ().GetMilliSeconds ()));
  ScheduleNextTick ();
}

void
BleDiscoveryEngineWrapper::EngineSendHook (const ble_discovery_packet_t *packet, void *context)
{
  BleDiscoveryEngineWrapper *self = static_cast<BleDiscoveryEngineWrapper *> (context);
  if (self)
    {
      self->HandleEngineSend (packet);
    }
}

void
BleDiscoveryEngineWrapper::EngineLogHook (const char *level, const char *message, void *context)
{
  NS_UNUSED (level);
  BleDiscoveryEngineWrapper *self = static_cast<BleDiscoveryEngineWrapper *> (context);
  if (!self || !message)
    {
      return;
    }
  NS_LOG_DEBUG ("Engine: " << message);
}

void
BleDiscoveryEngineWrapper::HandleEngineSend (const ble_discovery_packet_t *packet)
{
  if (!packet)
    {
      return;
    }

  if (m_txCallback.IsNull ())
    {
      NS_LOG_WARN ("No transmission callback registered");
      return;
    }

  BleDiscoveryHeaderWrapper header;
  header.SetSenderId (packet->sender_id);
  header.SetTtl (packet->ttl);
  for (uint16_t i = 0; i < packet->path_length; ++i)
    {
      header.AddToPath (packet->path[i]);
    }
  header.SetGpsAvailable (packet->gps_available);
  if (packet->gps_available)
    {
      header.SetGpsLocation (Vector (packet->gps_location.x,
                                     packet->gps_location.y,
                                     packet->gps_location.z));
    }

  Ptr<Packet> pkt = Create<Packet> ();
  pkt->AddHeader (header);
  m_txCallback (pkt);
}

}
