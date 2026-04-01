/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * BLE Discovery Engine NS-3 Wrapper
 */

#include "ble-discovery-engine.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/double.h"
#include "ns3/integer.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BleDiscoveryEngine");
NS_OBJECT_ENSURE_REGISTERED (BleDiscoveryEngine);

TypeId
BleDiscoveryEngine::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BleDiscoveryEngine")
    .SetParent<Object> ()
    .SetGroupName ("BleMeshDiscovery")
    .AddConstructor<BleDiscoveryEngine> ()
    .AddAttribute ("SlotDuration",
                   "Discovery slot duration",
                   TimeValue (MilliSeconds (BLE_DISCOVERY_DEFAULT_SLOT_DURATION_MS)),
                   MakeTimeAccessor (&BleDiscoveryEngine::m_slotDuration),
                   MakeTimeChecker ())
    .AddAttribute ("InitialTtl",
                   "TTL used for locally-originated messages",
                   UintegerValue (BLE_DISCOVERY_DEFAULT_TTL),
                   MakeUintegerAccessor (&BleDiscoveryEngine::m_initialTtl),
                   MakeUintegerChecker<uint8_t> (1))
    .AddAttribute ("ProximityThreshold",
                   "GPS proximity threshold (meters)",
                   DoubleValue (10.0),
                   MakeDoubleAccessor (&BleDiscoveryEngine::m_proximityThreshold),
                   MakeDoubleChecker<double> (0.0))
    .AddAttribute ("NoiseSlotCount",
                   "Number of micro-slots in the noisy measurement phase",
                   UintegerValue (BLE_ENGINE_DEFAULT_NOISE_SLOTS),
                   MakeUintegerAccessor (&BleDiscoveryEngine::m_noiseSlotCount),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("NoiseSlotDuration",
                   "Duration of each noisy micro-slot",
                   TimeValue (MilliSeconds (BLE_ENGINE_DEFAULT_NOISE_SLOT_DURATION_MS)),
                   MakeTimeAccessor (&BleDiscoveryEngine::m_noiseSlotDuration),
                   MakeTimeChecker ())
    .AddAttribute ("NeighborSlotCount",
                   "Number of micro-slots in the neighbor-discovery phase",
                   UintegerValue (BLE_ENGINE_DEFAULT_NEIGHBOR_SLOTS),
                   MakeUintegerAccessor (&BleDiscoveryEngine::m_neighborSlotCount),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("NeighborSlotDuration",
                   "Duration of each neighbor micro-slot",
                   TimeValue (MilliSeconds (BLE_ENGINE_DEFAULT_NEIGHBOR_SLOT_DURATION_MS)),
                   MakeTimeAccessor (&BleDiscoveryEngine::m_neighborSlotDuration),
                   MakeTimeChecker ())
    .AddAttribute ("NeighborTimeoutCycles",
                   "Discovery cycles before neighbors are considered stale",
                   UintegerValue (BLE_ENGINE_DEFAULT_NEIGHBOR_TIMEOUT_CYCLES),
                   MakeUintegerAccessor (&BleDiscoveryEngine::m_neighborTimeoutCycles),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("FdmaChannels",
                   "FDMA channels for data phase (placeholder wiring)",
                   UintegerValue (BLE_ENGINE_DEFAULT_FDMA_CHANNELS),
                   MakeUintegerAccessor (&BleDiscoveryEngine::m_fdmaChannels),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("TdmaSlots",
                   "TDMA slots per frame for data phase (placeholder wiring)",
                   UintegerValue (BLE_ENGINE_DEFAULT_TDMA_SLOTS),
                   MakeUintegerAccessor (&BleDiscoveryEngine::m_tdmaSlots),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("FrameDuration",
                   "TDMA frame duration (placeholder wiring)",
                   TimeValue (MilliSeconds (BLE_ENGINE_DEFAULT_FRAME_DURATION_MS)),
                   MakeTimeAccessor (&BleDiscoveryEngine::m_frameDuration),
                   MakeTimeChecker ())
    .AddAttribute ("Mode1Duration",
                   "Mode1 duration (placeholder, not enforced)",
                   TimeValue (MilliSeconds (BLE_ENGINE_DEFAULT_MODE_DURATION_MS)),
                   MakeTimeAccessor (&BleDiscoveryEngine::m_mode1Duration),
                   MakeTimeChecker ())
    .AddAttribute ("Mode2Duration",
                   "Mode2 duration (placeholder, not enforced)",
                   TimeValue (MilliSeconds (BLE_ENGINE_DEFAULT_MODE_DURATION_MS)),
                   MakeTimeAccessor (&BleDiscoveryEngine::m_mode2Duration),
                   MakeTimeChecker ())
    .AddAttribute ("EnableCollisionModel",
                   "Enable slot-level collision gating",
                   BooleanValue (true),
                   MakeBooleanAccessor (&BleDiscoveryEngine::m_enableCollisionModel),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableDataPhase",
                   "Enable data-phase scheduling hooks",
                   BooleanValue (true),
                   MakeBooleanAccessor (&BleDiscoveryEngine::m_enableDataPhase),
                   MakeBooleanChecker ())
    .AddAttribute ("NodeId",
                   "Unique node identifier",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BleDiscoveryEngine::m_nodeId),
                   MakeUintegerChecker<uint32_t> (1))
    .AddTraceSource ("MetricsUpdate",
                     "Fires when the engine publishes connectivity metrics",
                     MakeTraceSourceAccessor (&BleDiscoveryEngine::m_metricsTrace),
                     "ns3::BleDiscoveryEngine::MetricsTraceCallback")
    .AddTraceSource ("SlotOutcome",
                     "Fires when a data-phase slot outcome is recorded",
                     MakeTraceSourceAccessor (&BleDiscoveryEngine::m_slotOutcomeTrace),
                     "ns3::BleDiscoveryEngine::SlotOutcomeTraceCallback")
  ;
  return tid;
}

BleDiscoveryEngine::BleDiscoveryEngine ()
  : m_slotDuration (MilliSeconds (BLE_DISCOVERY_DEFAULT_SLOT_DURATION_MS)),
    m_initialTtl (BLE_DISCOVERY_DEFAULT_TTL),
    m_proximityThreshold (10.0),
    m_nodeId (0),
    m_noiseSlotCount (BLE_ENGINE_DEFAULT_NOISE_SLOTS),
    m_noiseSlotDuration (MilliSeconds (BLE_ENGINE_DEFAULT_NOISE_SLOT_DURATION_MS)),
    m_neighborSlotCount (BLE_ENGINE_DEFAULT_NEIGHBOR_SLOTS),
    m_neighborSlotDuration (MilliSeconds (BLE_ENGINE_DEFAULT_NEIGHBOR_SLOT_DURATION_MS)),
    m_neighborTimeoutCycles (BLE_ENGINE_DEFAULT_NEIGHBOR_TIMEOUT_CYCLES),
    m_fdmaChannels (BLE_ENGINE_DEFAULT_FDMA_CHANNELS),
    m_tdmaSlots (BLE_ENGINE_DEFAULT_TDMA_SLOTS),
  m_frameDuration (MilliSeconds (BLE_ENGINE_DEFAULT_FRAME_DURATION_MS)),
  m_mode1Duration (MilliSeconds (BLE_ENGINE_DEFAULT_MODE_DURATION_MS)),
  m_mode2Duration (MilliSeconds (BLE_ENGINE_DEFAULT_MODE_DURATION_MS)),
  m_enableCollisionModel (true),
  m_enableDataPhase (true),
  m_initialized (false),
  m_running (false)
{
  NS_LOG_FUNCTION (this);
  ble_engine_config_init (&m_config);
}

BleDiscoveryEngine::~BleDiscoveryEngine ()
{
  NS_LOG_FUNCTION (this);
  Stop ();
}

bool
BleDiscoveryEngine::Initialize (void)
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
  m_config.noise_slot_count = m_noiseSlotCount;
  m_config.noise_slot_duration_ms = static_cast<uint32_t> (m_noiseSlotDuration.GetMilliSeconds ());
  m_config.neighbor_slot_count = m_neighborSlotCount;
  m_config.neighbor_slot_duration_ms = static_cast<uint32_t> (m_neighborSlotDuration.GetMilliSeconds ());
  m_config.neighbor_timeout_cycles = m_neighborTimeoutCycles;
  m_config.fdma_channels = m_fdmaChannels;
  m_config.tdma_slots = m_tdmaSlots;
  m_config.frame_duration_ms = static_cast<uint32_t> (m_frameDuration.GetMilliSeconds ());
  m_config.mode1_duration_ms = static_cast<uint32_t> (m_mode1Duration.GetMilliSeconds ());
  m_config.mode2_duration_ms = static_cast<uint32_t> (m_mode2Duration.GetMilliSeconds ());
  m_config.enable_collision_model = m_enableCollisionModel;
  m_config.enable_data_phase = m_enableDataPhase;
  m_config.send_cb = &BleDiscoveryEngine::EngineSendHook;
  m_config.log_cb = &BleDiscoveryEngine::EngineLogHook;
  m_config.metrics_cb = &BleDiscoveryEngine::EngineMetricsHook;
  m_config.slot_cb = &BleDiscoveryEngine::EngineSlotHook;
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
BleDiscoveryEngine::Start (void)
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
  m_tickEvent = Simulator::ScheduleNow (&BleDiscoveryEngine::RunTick, this);
}

void
BleDiscoveryEngine::Stop (void)
{
  NS_LOG_FUNCTION (this);
  if (m_running)
    {
      Simulator::Cancel (m_tickEvent);
      m_running = false;
    }
}

void
BleDiscoveryEngine::SetSendCallback (TxCallback cb)
{
  m_txCallback = cb;
}

void
BleDiscoveryEngine::Receive (const BleDiscoveryHeaderWrapper& header, int8_t rssi)
{
  if (!m_initialized && !Initialize ())
    {
      return;
    }

  uint32_t now_ms = static_cast<uint32_t> (Simulator::Now ().GetMilliSeconds ());

  if (header.IsElectionMessage ())
    {
      const ble_election_packet_t& cElection = header.GetCElectionPacket ();
      const ble_discovery_packet_t *basePtr =
        reinterpret_cast<const ble_discovery_packet_t *> (&cElection);
      ble_engine_receive_packet (&m_engine, basePtr, rssi, now_ms);
    }
  else
    {
      const ble_discovery_packet_t& cPacket = header.GetCPacket ();
      ble_engine_receive_packet (&m_engine, &cPacket, rssi, now_ms);
    }
}

void
BleDiscoveryEngine::SetCrowdingFactor (double crowdingFactor)
{
  ble_engine_set_crowding_factor (&m_engine, crowdingFactor);
}

void
BleDiscoveryEngine::SetNoiseLevel (double noiseLevel)
{
  ble_engine_set_noise_level (&m_engine, noiseLevel);
}

void
BleDiscoveryEngine::MarkCandidateHeard (void)
{
  ble_engine_mark_candidate_heard (&m_engine);
}

void
BleDiscoveryEngine::SetGpsLocation (Vector location, bool valid)
{
  ble_engine_set_gps (&m_engine, location.x, location.y, location.z, valid);
}

void
BleDiscoveryEngine::SeedRandom (uint32_t seed)
{
  ble_engine_seed_random (seed);
}

const ble_mesh_node_t*
BleDiscoveryEngine::GetNode (void) const
{
  return ble_engine_get_node (&m_engine);
}

void
BleDiscoveryEngine::DoDispose ()
{
  Stop ();
  Object::DoDispose ();
}

void
BleDiscoveryEngine::ScheduleNextTick (void)
{
  if (!m_running)
    {
      return;
    }

  m_tickEvent = Simulator::Schedule (m_slotDuration,
                                     &BleDiscoveryEngine::RunTick,
                                     this);
}

void
BleDiscoveryEngine::RunTick (void)
{
  ble_engine_tick (&m_engine,
                   static_cast<uint32_t> (Simulator::Now ().GetMilliSeconds ()));
  ScheduleNextTick ();
}

void
BleDiscoveryEngine::EngineSendHook (const ble_discovery_packet_t *packet, void *context)
{
  BleDiscoveryEngine *self = static_cast<BleDiscoveryEngine *> (context);
  if (self)
    {
      self->HandleEngineSend (packet);
    }
}

void
BleDiscoveryEngine::EngineLogHook (const char *level, const char *message, void *context)
{
  NS_UNUSED (level);
  BleDiscoveryEngine *self = static_cast<BleDiscoveryEngine *> (context);
  if (!self || !message)
    {
      return;
    }
  NS_LOG_DEBUG ("Engine: " << message);
}

void
BleDiscoveryEngine::EngineMetricsHook (const ble_connectivity_metrics_t *metrics,
                                              void *context)
{
  BleDiscoveryEngine *self = static_cast<BleDiscoveryEngine *> (context);
  if (self)
    {
      self->HandleMetricsUpdate (metrics);
    }
}

void
BleDiscoveryEngine::EngineSlotHook (const ble_engine_slot_event_t *evt,
                                           void *context)
{
  BleDiscoveryEngine *self = static_cast<BleDiscoveryEngine *> (context);
  if (!self || !evt)
    {
      return;
    }
  SlotOutcomeEvent out;
  out.nodeId = evt->node_id;
  out.isClusterSlot = evt->is_cluster_slot;
  out.frameIndex = evt->frame_index;
  out.slotIndex = evt->slot_index;
  out.channelIndex = evt->channel_index;
  out.iteration = evt->iteration;
  out.outcome = static_cast<uint8_t> (evt->outcome);
  self->m_slotOutcomeTrace (out);
}

void
BleDiscoveryEngine::HandleEngineSend (const ble_discovery_packet_t *packet)
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

  if (packet->message_type == BLE_MSG_ELECTION_ANNOUNCEMENT)
    {
      header.SetAsElectionMessage ();
      const ble_election_packet_t *src =
        reinterpret_cast<const ble_election_packet_t *> (packet);
      ble_election_packet_t &dst = header.GetCElectionPacketMutable ();
      dst = *src;
      header.GetCPacketMutable () = dst.base;
    }
  else
    {
      header.GetCPacketMutable () = *packet;
    }

  Ptr<Packet> pkt = Create<Packet> ();
  pkt->AddHeader (header);
  m_txCallback (pkt);
}

void
BleDiscoveryEngine::HandleMetricsUpdate (const ble_connectivity_metrics_t *metrics)
{
  if (!metrics)
    {
      return;
    }

  m_metricsTrace (*metrics);
}

} // namespace ns3
