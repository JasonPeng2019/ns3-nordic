/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef BLE_CLUSTER_MANAGER_H
#define BLE_CLUSTER_MANAGER_H

#include "ns3/object.h"
#include <map>
#include <vector>

namespace ns3 {

class BleClusterManager : public Object
{
public:
  static TypeId GetTypeId (void);
  BleClusterManager ();
  ~BleClusterManager () override;

  void SetClusterhead (uint32_t nodeId, uint32_t clusterheadId, uint16_t hops, uint32_t directCount);
  bool GetClusterhead (uint32_t nodeId, uint32_t &clusterheadId) const;
  void Clear ();

private:
  std::map<uint32_t, uint32_t> m_assignment;
};

} // namespace ns3

#endif /* BLE_CLUSTER_MANAGER_H */
