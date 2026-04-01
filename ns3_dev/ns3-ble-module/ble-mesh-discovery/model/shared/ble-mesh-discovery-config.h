/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef BLE_MESH_DISCOVERY_CONFIG_H
#define BLE_MESH_DISCOVERY_CONFIG_H

#include <cstdint>

namespace ns3 {

struct BleMeshDiscoveryConfig
{
  uint8_t initialTtl = 10;
  double proximityThresholdMeters = 10.0;
  double crowdingThreshold = 0.5;
  uint32_t clusterCapacity = 150;
  uint32_t seed = 1;
  uint32_t slotDurationMs = 100;
};

enum BleMeshNodeState
{
  BLE_STATE_DISCOVERY = 0,
  BLE_STATE_EDGE = 1,
  BLE_STATE_CLUSTERHEAD_CANDIDATE = 2,
  BLE_STATE_CLUSTERHEAD = 3
};

} // namespace ns3

#endif /* BLE_MESH_DISCOVERY_CONFIG_H */
