/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ble-cluster-manager.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (BleClusterManager);

TypeId
BleClusterManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BleClusterManager")
    .SetParent<Object> ()
    .SetGroupName ("BleMeshDiscovery")
    .AddConstructor<BleClusterManager> ();
  return tid;
}

BleClusterManager::BleClusterManager () = default;
BleClusterManager::~BleClusterManager () = default;

void
BleClusterManager::SetClusterhead (uint32_t nodeId, uint32_t clusterheadId, uint16_t, uint32_t)
{
  m_assignment[nodeId] = clusterheadId;
}

bool
BleClusterManager::GetClusterhead (uint32_t nodeId, uint32_t &clusterheadId) const
{
  auto it = m_assignment.find (nodeId);
  if (it == m_assignment.end ())
    {
      return false;
    }
  clusterheadId = it->second;
  return true;
}

void
BleClusterManager::Clear ()
{
  m_assignment.clear ();
}

} // namespace ns3
