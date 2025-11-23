/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025 Your Institution
 *
 * Author: Benjamin Huh <buh07@github>
 *
 * C++ Wrapper for BLE Clusterhead Election - Thin layer over C protocol core
 */

#ifndef BLE_ELECTION_WRAPPER_H
#define BLE_ELECTION_WRAPPER_H

#include "ns3/object.h"
#include "ns3/vector.h"
#include "ns3/nstime.h"
#include <vector>
#include <map>

extern "C" {
#include "ns3/ble_election.h"
}

namespace ns3 {

/**
 * \ingroup ble-mesh-discovery
 * \brief Neighbor information (C++ wrapper)
 */
struct NeighborInfo
{
  uint32_t nodeId;                      //!< Neighbor node ID
  Vector location;                      //!< Last known GPS location
  int8_t rssi;                          //!< Last RSSI measurement (dBm)
  uint32_t messageCount;                //!< Messages received from neighbor
  Time lastSeen;                        //!< Last time we heard from neighbor
  bool isDirect;                        //!< True if 1-hop neighbor
};

/**
 * \ingroup ble-mesh-discovery
 * \brief Connectivity metrics (C++ wrapper)
 */
struct ConnectivityMetrics
{
  uint32_t directConnections;           //!< Number of direct (1-hop) neighbors
  uint32_t totalNeighbors;              //!< Total unique neighbors
  double crowdingFactor;                //!< Local crowding (0.0-1.0)
  double connectionNoiseRatio;          //!< Direct connections / (1 + crowding)
  double geographicDistribution;        //!< Spatial distribution score (0.0-1.0)
  uint32_t messagesForwarded;           //!< Successfully forwarded messages
  uint32_t messagesReceived;            //!< Total messages received
  double forwardingSuccessRate;         //!< Forwarding success ratio
};

/**
 * \ingroup ble-mesh-discovery
 * \brief C++ wrapper for BLE clusterhead election
 *
 * Wraps the pure C election implementation for NS-3 integration.
 * Implements Phase 3 Tasks 12-18.
 */
class BleElection : public Object
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
  BleElection ();

  /**
   * \brief Destructor
   */
  virtual ~BleElection ();

  /**
   * \brief Add or update neighbor information (Task 15)
   * \param nodeId Neighbor node ID
   * \param location GPS location
   * \param rssi RSSI measurement (dBm)
   */
  void UpdateNeighbor (uint32_t nodeId, Vector location, int8_t rssi);

  /**
   * \brief Add RSSI sample for crowding calculation (Task 13)
   * \param rssi RSSI sample (dBm)
   */
  void AddRssiSample (int8_t rssi);

  /**
   * \brief Begin noisy-window RSSI measurement (default 2s)
   * \param duration Length of window
   */
  void BeginNoiseWindow (Time duration = Seconds (2.0));

  /**
   * \brief End noisy-window measurement (captures snapshot)
   */
  void EndNoiseWindow ();

  /**
   * \brief Check if noisy window should auto-complete
   */
  void CheckNoiseWindow ();

  /**
   * \brief Query noisy window status / snapshot
   */
  bool IsNoiseWindowActive () const;
  bool IsNoiseWindowComplete () const;
  double GetCrowdingSnapshot () const;

  /**
   * \brief Calculate crowding factor (Task 13)
   * \return Crowding factor (0.0 = not crowded, 1.0 = very crowded)
   */
  double CalculateCrowding () const;

  /**
   * \brief Count direct connections (Task 15)
   * \return Number of direct (1-hop) neighbors
   */
  uint32_t CountDirectConnections () const;

  /**
   * \brief Calculate geographic distribution (Task 18)
   * \return Distribution score (0.0-1.0)
   */
  double CalculateGeographicDistribution () const;

  /**
   * \brief Update connectivity metrics (Task 16)
   */
  void UpdateMetrics ();

  /**
   * \brief Calculate candidacy score (Task 17)
   * \return Candidacy score (higher = better candidate)
   */
  double CalculateCandidacyScore () const;

  /**
   * \brief Determine if should become candidate (Task 17)
   * \return true if node should become clusterhead candidate
   */
  bool ShouldBecomeCandidate ();

  /**
   * \brief Get connectivity metrics (Task 16)
   * \return Current connectivity metrics
   */
  ConnectivityMetrics GetMetrics () const;

  /**
   * \brief Get all neighbors
   * \return Vector of neighbor information
   */
  std::vector<NeighborInfo> GetNeighbors () const;

  /**
   * \brief Get specific neighbor
   * \param nodeId Node ID to find
   * \param info Output parameter for neighbor info
   * \return true if neighbor found
   */
  bool GetNeighbor (uint32_t nodeId, NeighborInfo& info) const;

  /**
   * \brief Clean old neighbors
   * \param timeout Maximum age for neighbors
   * \return Number of neighbors removed
   */
  uint32_t CleanOldNeighbors (Time timeout);

  /**
   * \brief Set candidacy thresholds
   * \param minNeighbors Minimum direct neighbors
   * \param minCnRatio Minimum connection:noise ratio
   * \param minGeoDist Minimum geographic distribution
   */
  void SetThresholds (uint32_t minNeighbors, double minCnRatio, double minGeoDist);

  /**
   * \brief Set direct connection RSSI threshold
   * \param threshold RSSI threshold in dBm
   */
  void SetDirectRssiThreshold (int8_t threshold);

  /**
   * \brief Configure score weights used in candidacy calculation
   * \param directWeight Weight for direct connections
   * \param ratioWeight Weight for connection:noise ratio
   * \param geoWeight Weight for geographic distribution
   * \param forwardingWeight Weight for forwarding success
   */
  void SetScoreWeights (double directWeight,
                        double ratioWeight,
                        double geoWeight,
                        double forwardingWeight);

  /**
   * \brief Record a message was forwarded (for success rate)
   */
  void RecordMessageForwarded ();

  /**
   * \brief Record a message was received (for success rate)
   */
  void RecordMessageReceived ();

  /**
   * \brief Check if is candidate
   * \return true if node is candidate
   */
  bool IsCandidate () const;

  /**
   * \brief Get candidacy score
   * \return Current candidacy score
   */
  double GetCandidacyScore () const;

private:
  ble_election_state_t m_state;         //!< C core election state
};

} // namespace ns3

#endif /* BLE_ELECTION_WRAPPER_H */
