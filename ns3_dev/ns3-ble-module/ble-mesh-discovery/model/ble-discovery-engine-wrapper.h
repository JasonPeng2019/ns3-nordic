#ifndef BLE_DISCOVERY_ENGINE_WRAPPER_H
#define BLE_DISCOVERY_ENGINE_WRAPPER_H

#include "ns3/object.h"
#include "ns3/callback.h"
#include "ns3/event-id.h"
#include "ns3/packet.h"
#include "ns3/vector.h"
#include "ns3/nstime.h"
#include "ble-discovery-header-wrapper.h"

extern "C" {
#include "ns3/ble_discovery_engine.h"
}

namespace ns3 {

/**
 * \brief NS-3 wrapper for the pure C discovery engine
 */
class BleDiscoveryEngineWrapper : public Object
{
public:
  typedef Callback<void, Ptr<Packet> > TxCallback;

  static TypeId GetTypeId (void);

  BleDiscoveryEngineWrapper ();
  ~BleDiscoveryEngineWrapper () override;

  /**
   * \brief Configure and initialize the engine with current attributes
   */
  bool Initialize (void);

  /**
   * \brief Start periodic ticks
   */
  void Start (void);

  /**
   * \brief Stop periodic ticks
   */
  void Stop (void);

  /**
   * \brief Install callback invoked when engine transmits a packet
   */
  void SetSendCallback (TxCallback cb);

  /**
   * \brief Feed a received discovery header into the engine
   */
  void Receive (const BleDiscoveryHeaderWrapper& header, int8_t rssi);

  /**
   * \brief Update crowding factor (0-1)
   */
  void SetCrowdingFactor (double crowdingFactor);

  /**
   * \brief Update measured noise level
   */
  void SetNoiseLevel (double noiseLevel);

  /**
   * \brief Mark that another candidate announcement was heard
   */
  void MarkCandidateHeard (void);

  /**
   * \brief Update GPS location/state
   */
  void SetGpsLocation (Vector location, bool valid);

  /**
   * \brief Seed forwarding RNG (for deterministic tests)
   */
  void SeedRandom (uint32_t seed);

  /**
   * \brief Access underlying node state
   */
  const ble_mesh_node_t* GetNode (void) const;

protected:
  void DoDispose () override;

private:
  void ScheduleNextTick (void);
  void RunTick (void);
  static void EngineSendHook (const ble_discovery_packet_t *packet, void *context);
  static void EngineLogHook (const char *level, const char *message, void *context);
  void HandleEngineSend (const ble_discovery_packet_t *packet);

  Time m_slotDuration;
  uint8_t m_initialTtl;
  double m_proximityThreshold;
  uint32_t m_nodeId;

  bool m_initialized;
  bool m_running;
  EventId m_tickEvent;

  ble_engine_config_t m_config;
  ble_engine_t m_engine;
  TxCallback m_txCallback;
};

}

#endif
