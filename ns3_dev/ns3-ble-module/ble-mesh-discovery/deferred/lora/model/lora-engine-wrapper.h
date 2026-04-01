/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * LoRA Engine NS-3 Wrapper (placeholder)
 */

#ifndef LORA_ENGINE_WRAPPER_H
#define LORA_ENGINE_WRAPPER_H

#include "ns3/object.h"
#include "ns3/callback.h"
#include "ns3/event-id.h"
#include "ns3/packet.h"
#include "ns3/nstime.h"

extern "C" {
#include "ns3/lora_engine.h"
}

namespace ns3 {

class LoraEngineWrapper : public Object
{
public:
  typedef Callback<void, Ptr<Packet> > TxCallback;

  static TypeId GetTypeId (void);

  LoraEngineWrapper ();
  ~LoraEngineWrapper () override;

  bool Initialize (void);
  void Start (void);
  void Stop (void);
  void SetSendCallback (TxCallback cb);
  void Receive (const lora_discovery_packet_t& packet, int8_t rssi);
  void SetCrowdingFactor (double crowding);

private:
  void ScheduleNextTick (void);
  void RunTick (void);
  static void EngineSendHook (const lora_discovery_packet_t *packet, void *context);
  static void EngineLogHook (const char *level, const char *message, void *context);
  void HandleEngineSend (const lora_discovery_packet_t *packet);

  Time m_slotDuration;
  uint32_t m_nodeId;
  uint8_t m_initialChannel;
  uint8_t m_maxChannels;

  bool m_initialized;
  bool m_running;
  EventId m_tickEvent;

  lora_engine_config_t m_config;
  lora_engine_t m_engine;
  TxCallback m_txCallback;
};

} // namespace ns3

#endif /* LORA_ENGINE_WRAPPER_H */
