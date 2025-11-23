#ifndef BLE_ELECTION_HEADER_H
#define BLE_ELECTION_HEADER_H

#include "ble-discovery-header-wrapper.h"

namespace ns3 {

/**
 * \ingroup ble-mesh-discovery
 * \brief Specialized header for clusterhead election announcements.
 *
 * This class guarantees that all serialized packets are election announcements.
 * It reuses the underlying C core serialization but enforces clusterhead flag
 * semantics required by Phase 4 Task 19.
 */
class BleElectionHeader : public BleDiscoveryHeaderWrapper
{
public:
  BleElectionHeader ();
  ~BleElectionHeader () override;

  static TypeId GetTypeId (void);
  TypeId GetInstanceTypeId (void) const override;

  uint32_t GetSerializedSize (void) const override;
  void Serialize (Buffer::Iterator start) const override;
  uint32_t Deserialize (Buffer::Iterator start) override;

  
};

} 

#endif 
