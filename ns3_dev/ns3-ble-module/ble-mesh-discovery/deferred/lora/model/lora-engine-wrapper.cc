/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * LoRA Engine NS-3 Wrapper (placeholder)
 */

#include "lora-engine-wrapper.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/packet.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LoraEngineWrapper");
NS_OBJECT_ENSURE_REGISTERED (LoraEngineWrapper);

TypeId
LoraEngineWrapper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LoraEngineWrapper")
    .SetParent<Object> ()
    .SetGroupName ("LoRa")
    .AddConstructor<LoraEngineWrapper> ()
    .AddAttribute ("SlotDuration",
                   "Engine tick duration",
                   TimeValue (MilliSeconds (100)),
                   MakeTimeAccessor (&LoraEngineWrapper::m_slotDuration),
                   MakeTimeChecker ())
    .AddAttribute ("NodeId",
                   "Unique node identifier",
                   UintegerValue (0),
                   MakeUintegerAccessor (&LoraEngineWrapper::m_nodeId),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("InitialChannel",
                   "Initial LoRA channel",
                   UintegerValue (1),
                   MakeUintegerAccessor (&LoraEngineWrapper::m_initialChannel),
                   MakeUintegerChecker<uint8_t> (1))
    .AddAttribute ("MaxChannels",
                   "Maximum LoRA channels",
                   UintegerValue (1),
                   MakeUintegerAccessor (&LoraEngineWrapper::m_maxChannels),
                   MakeUintegerChecker<uint8_t> (1));
  return tid;
}

LoraEngineWrapper::LoraEngineWrapper ()
  : m_slotDuration (MilliSeconds (100)),
    m_nodeId (0),
    m_initialChannel (1),
    m_maxChannels (1),
    m_initialized (false),
    m_running (false)
{
  NS_LOG_FUNCTION (this);
  lora_engine_config_init (&m_config);
}

LoraEngineWrapper::~LoraEngineWrapper ()
{
  NS_LOG_FUNCTION (this);
  Stop ();
}

bool
LoraEngineWrapper::Initialize (void)
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
  lora_engine_config_init (&m_config);
  m_config.node_id = m_nodeId;
  m_config.initial_channel = m_initialChannel;
  m_config.max_channels = m_maxChannels;
  m_config.slot_duration_ms = static_cast<uint32_t> (m_slotDuration.GetMilliSeconds ());
  m_config.send_cb = &LoraEngineWrapper::EngineSendHook;
  m_config.log_cb = &LoraEngineWrapper::EngineLogHook;
  m_config.user_context = this;

  if (!lora_engine_init (&m_engine, &m_config))
    {
      NS_LOG_ERROR ("Failed to initialize LoRA engine");
      return false;
    }

  m_initialized = true;
  return true;
}

void
LoraEngineWrapper::Start (void)
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
  m_tickEvent = Simulator::ScheduleNow (&LoraEngineWrapper::RunTick, this);
}

void
LoraEngineWrapper::Stop (void)
{
  NS_LOG_FUNCTION (this);
  if (m_running)
    {
      Simulator::Cancel (m_tickEvent);
      m_running = false;
    }
}

void
LoraEngineWrapper::SetSendCallback (TxCallback cb)
{
  m_txCallback = cb;
}

void
LoraEngineWrapper::Receive (const lora_discovery_packet_t& packet, int8_t rssi)
{
  if (!m_initialized && !Initialize ())
    {
      return;
    }
  uint32_t now_ms = static_cast<uint32_t> (Simulator::Now ().GetMilliSeconds ());
  lora_engine_receive_packet (&m_engine, &packet, rssi, now_ms);
}

void
LoraEngineWrapper::SetCrowdingFactor (double crowding)
{
  lora_engine_set_crowding (&m_engine, crowding);
}

void
LoraEngineWrapper::ScheduleNextTick (void)
{
  if (!m_running)
    {
      return;
    }
  m_tickEvent = Simulator::Schedule (m_slotDuration,
                                     &LoraEngineWrapper::RunTick,
                                     this);
}

void
LoraEngineWrapper::RunTick (void)
{
  lora_engine_tick (&m_engine,
                    static_cast<uint32_t> (Simulator::Now ().GetMilliSeconds ()));
  ScheduleNextTick ();
}

void
LoraEngineWrapper::EngineSendHook (const lora_discovery_packet_t *packet, void *context)
{
  LoraEngineWrapper *self = static_cast<LoraEngineWrapper *> (context);
  if (self)
    {
      self->HandleEngineSend (packet);
    }
}

void
LoraEngineWrapper::EngineLogHook (const char *level, const char *message, void *context)
{
  NS_UNUSED (level);
  LoraEngineWrapper *self = static_cast<LoraEngineWrapper *> (context);
  if (!self || !message)
    {
      return;
    }
  NS_LOG_DEBUG ("LoRA Engine: " << message);
}

void
LoraEngineWrapper::HandleEngineSend (const lora_discovery_packet_t *packet)
{
  if (!packet)
    {
      return;
    }
  if (m_txCallback.IsNull ())
    {
      NS_LOG_WARN ("No LoRA TX callback registered");
      return;
    }
  uint8_t buf[256];
  uint32_t len = lora_discovery_serialize (packet, buf, sizeof(buf));
  if (len == 0)
    {
      NS_LOG_WARN ("LoRA serialize failed");
      return;
    }
  Ptr<Packet> pkt = Create<Packet> (buf, len);
  m_txCallback (pkt);
}

} // namespace ns3
