/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025 Your Institution
 *
 * Author: Benjamin Huh <buh07@github>
 *
 * C++ Wrapper for BLE Forwarding Logic - Thin layer over C protocol core
 */

#ifndef BLE_FORWARDING_LOGIC_WRAPPER_H
#define BLE_FORWARDING_LOGIC_WRAPPER_H

#include "ns3/object.h"
#include "ns3/vector.h"
#include "ble-discovery-header-wrapper.h"

extern "C" {
#include "ns3/ble_forwarding_logic.h"
}

namespace ns3 {

/**
 * \ingroup ble-mesh-discovery
 * \brief C++ wrapper for BLE forwarding logic (wraps pure C implementation)
 *
 * Implements the 3-metric forwarding algorithm:
 * - Task 9: Picky Forwarding (crowding factor)
 * - Task 10: GPS Proximity Filtering
 * - Task 11: TTL-Based Prioritization
 */
class BleForwardingLogic : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Constructor
   */
  BleForwardingLogic ();

  /**
   * \brief Destructor
   */
  virtual ~BleForwardingLogic ();

  /**
   * \brief Calculate crowding factor from RSSI measurements
   * \param rssiSamples Vector of RSSI values (dBm)
   * \return Crowding factor (0.0 to 1.0, higher = more crowded)
   */
  double CalculateCrowdingFactor (const std::vector<int8_t>& rssiSamples) const;

  /**
   * \brief Determine if message should be forwarded based on crowding
   * \param crowdingFactor The crowding factor
   * \param directNeighbors Number of direct neighbors observed
   * \return true if message should be forwarded
   */
  bool ShouldForwardCrowding (double crowdingFactor, uint32_t directNeighbors);

  /**
   * \brief Calculate distance between two GPS locations
   * \param loc1 First location
   * \param loc2 Second location
   * \return Distance in meters
   */
  double CalculateDistance (Vector loc1, Vector loc2) const;

  /**
   * \brief Determine if message should be forwarded based on GPS proximity
   * \param currentLocation This node's location
   * \param lastHopLocation Last hop's location (from message)
   * \param proximityThreshold Minimum distance for forwarding (meters)
   * \param directNeighbors Number of direct neighbors observed
   * \return true if message should be forwarded
   */
  bool ShouldForwardProximity (Vector currentLocation,
                                Vector lastHopLocation,
                                double proximityThreshold) const;

  /**
   * \brief Determine if message should be forwarded (all 3 metrics)
   * \param header The discovery message header
   * \param currentLocation This node's location
   * \param crowdingFactor Local crowding factor
   * \param proximityThreshold GPS proximity threshold (meters)
   * \return true if message should be forwarded
   */
  bool ShouldForward (const BleDiscoveryHeaderWrapper& header,
                      Vector currentLocation,
                      double crowdingFactor,
                      double proximityThreshold,
                      uint32_t directNeighbors);

  /**
   * \brief Calculate forwarding priority for a message
   * \param header The discovery message header
   * \return Priority value (lower = higher priority)
   */
  uint8_t CalculatePriority (const BleDiscoveryHeaderWrapper& header) const;

  /**
   * \brief Set proximity threshold
   * \param threshold Minimum distance for forwarding (meters)
   */
  void SetProximityThreshold (double threshold);

  /**
   * \brief Get proximity threshold
   * \return Proximity threshold in meters
   */
  double GetProximityThreshold () const;

  /**
   * \brief Seed the forwarding random generator (for deterministic testing)
   * \param seed Seed value
   */
  void SeedRandom (uint32_t seed);

private:
  double m_proximityThreshold;             //!< GPS proximity threshold (meters)
};

} // namespace ns3

#endif /* BLE_FORWARDING_LOGIC_WRAPPER_H */
