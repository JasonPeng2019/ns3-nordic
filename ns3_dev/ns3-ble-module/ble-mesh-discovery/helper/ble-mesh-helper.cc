/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ble-mesh-helper.h"

namespace ns3 {

BleMeshHelper::BleMeshHelper () = default;

void
BleMeshHelper::SetConfig (const BleMeshDiscoveryConfig &config)
{
  m_config = config;
}

NetDeviceContainer
BleMeshHelper::Install (NodeContainer) const
{
  return NetDeviceContainer ();
}

} // namespace ns3
