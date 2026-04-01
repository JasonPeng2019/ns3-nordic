/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef BLE_MESH_HELPER_H
#define BLE_MESH_HELPER_H

#include "ns3/object.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"
#include "ns3/ble-mesh-discovery-config.h"

namespace ns3 {

class BleMeshHelper
{
public:
  BleMeshHelper ();
  void SetConfig (const BleMeshDiscoveryConfig &config);
  NetDeviceContainer Install (NodeContainer nodes) const;

private:
  BleMeshDiscoveryConfig m_config;
};

} // namespace ns3

#endif /* BLE_MESH_HELPER_H */
