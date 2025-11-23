#ifndef BLE_MESH_NODE_WRAPPER_H
#define BLE_MESH_NODE_WRAPPER_H

#include "ns3/object.h"
#include "ns3/vector.h"
#include "ns3/nstime.h"
#include "ns3/callback.h"
#include "ns3/traced-callback.h"
#include "ns3/mobility-model.h"
#include "ns3/ble_mesh_node.h"
#include <vector>

namespace ns3 {

/**
 * \ingroup ble-mesh-discovery
 * \brief NS-3 C++ Wrapper for BLE Mesh Node (Pure C Implementation)
 *
 * This class provides a thin C++ wrapper around the pure C node implementation,
 * allowing it to integrate with NS-3 while keeping the core logic portable.
 */
class BleMeshNodeWrapper : public Object
{
public:
  /**
   * \brief Get the type ID
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Constructor
   */
  BleMeshNodeWrapper ();

  /**
   * \brief Destructor
   */
  virtual ~BleMeshNodeWrapper ();

  /**
   * \brief Initialize node with ID
   * \param nodeId Unique node identifier
   */
  void Initialize (uint32_t nodeId);

  

  /**
   * \brief Set node GPS location
   * \param position NS-3 Vector (x=lat, y=lon, z=alt)
   */
  void SetGpsLocation (Vector position);

  /**
   * \brief Get node GPS location
   * \return NS-3 Vector
   */
  Vector GetGpsLocation (void) const;

  /**
   * \brief Mark GPS as unavailable
   */
  void ClearGps (void);

  /**
   * \brief Check if GPS is available
   * \return true if GPS available
   */
  bool IsGpsAvailable (void) const;

  /**
   * \brief Set GPS cache time-to-live
   * \param ttl Time-to-live in discovery cycles (0=no expiration)
   */
  void SetGpsCacheTtl (uint32_t ttl);

  /**
   * \brief Check if GPS cache is still valid
   * \return true if GPS is available and cache hasn't expired
   */
  bool IsGpsCacheValid (void) const;

  /**
   * \brief Invalidate GPS cache (force refresh on next update)
   */
  void InvalidateGpsCache (void);

  /**
   * \brief Get age of GPS data in cycles
   * \return Number of cycles since last GPS update
   */
  uint32_t GetGpsAge (void) const;

  /**
   * \brief Update GPS location from NS-3 mobility model
   * \param mobilityModel Pointer to mobility model
   * \return true if GPS was updated successfully
   */
  bool UpdateGpsFromMobilityModel (Ptr<MobilityModel> mobilityModel);



  /**
   * \brief Get current node state
   * \return Current state
   */
  ble_node_state_t GetState (void) const;

  /**
   * \brief Get previous node state
   * \return Previous state
   */
  ble_node_state_t GetPreviousState (void) const;

  /**
   * \brief Set node state
   * \param newState New state to transition to
   * \return true if transition was valid
   */
  bool SetState (ble_node_state_t newState);

  /**
   * \brief Get state name as string
   * \param state The state
   * \return String representation
   */
  static std::string GetStateName (ble_node_state_t state);

  /**
   * \brief Get current state name
   * \return String representation of current state
   */
  std::string GetCurrentStateName (void) const;



  /**
   * \brief Advance to next discovery cycle
   */
  void AdvanceCycle (void);

  /**
   * \brief Get current cycle number
   * \return Current cycle
   */
  uint32_t GetCurrentCycle (void) const;



  /**
   * \brief Add or update neighbor
   * \param neighborId Neighbor's node ID
   * \param rssi RSSI value (dBm)
   * \param hopCount Hop count to neighbor
   * \return true if added/updated successfully
   */
  bool AddNeighbor (uint32_t neighborId, int8_t rssi, uint8_t hopCount);

  /**
   * \brief Update neighbor's GPS location
   * \param neighborId Neighbor's node ID
   * \param gps GPS location
   * \return true if neighbor found and updated
   */
  bool UpdateNeighborGps (uint32_t neighborId, Vector gps);

  /**
   * \brief Get number of neighbors
   * \return Neighbor count
   */
  uint16_t GetNeighborCount (void) const;

  /**
   * \brief Get number of direct (1-hop) neighbors
   * \return Direct neighbor count
   */
  uint16_t GetDirectNeighborCount (void) const;

  /**
   * \brief Get average RSSI of all neighbors
   * \return Average RSSI in dBm
   */
  int8_t GetAverageRssi (void) const;

  /**
   * \brief Remove stale neighbors
   * \param maxAge Maximum age in cycles
   * \return Number of neighbors removed
   */
  uint16_t PruneStaleNeighbors (uint32_t maxAge);

  

  /**
   * \brief Calculate candidacy score
   * \param noiseLevel Measured noise level
   * \return Candidacy score (higher = better)
   */
  double CalculateCandidacyScore (double noiseLevel);

  /**
   * \brief Get candidacy score
   * \return Score (0.0-1.0)
   */
  double GetCandidacyScore (void) const;

  /**
   * \brief Set candidacy score
   * \param score Score value
   */
  void SetCandidacyScore (double score);

  /**
   * \brief Get PDSF value
   * \return PDSF
   */
  uint32_t GetPdsf (void) const;

  /**
   * \brief Set PDSF value
   * \param pdsf PDSF value
   */
  void SetPdsf (uint32_t pdsf);

  /**
   * \brief Set the most recent noise level measurement
   * \param noiseLevel Noise value (>=0)
   */
  void SetNoiseLevel (double noiseLevel);

  /**
   * \brief Mark that another clusterhead candidate was heard
   */
  void MarkCandidateHeard (void);

  /**
   * \brief Get election hash
   * \return Hash value
   */
  uint32_t GetElectionHash (void) const;

  /**
   * \brief Check if node should become edge node
   * \return true if should become edge
   */
  bool ShouldBecomeEdge (void) const;

  /**
   * \brief Check if node should become candidate
   * \return true if should become candidate
   */
  bool ShouldBecomeCandidate (void) const;

  /**
   * \brief Set clusterhead ID (for members)
   * \param clusterId Clusterhead node ID
   */
  void SetClusterheadId (uint32_t clusterId);

  /**
   * \brief Get clusterhead ID
   * \return Clusterhead node ID
   */
  uint32_t GetClusterheadId (void) const;

  /**
   * \brief Set cluster class (for clusterheads)
   * \param classId Cluster class ID
   */
  void SetClusterClass (uint16_t classId);

  /**
   * \brief Get cluster class
   * \return Cluster class ID
   */
  uint16_t GetClusterClass (void) const;

  

  /**
   * \brief Update statistics
   */
  void UpdateStatistics (void);

  /**
   * \brief Get messages sent count
   * \return Count
   */
  uint32_t GetMessagesSent (void) const;

  /**
   * \brief Get messages received count
   * \return Count
   */
  uint32_t GetMessagesReceived (void) const;

  /**
   * \brief Get messages forwarded count
   * \return Count
   */
  uint32_t GetMessagesForwarded (void) const;

  /**
   * \brief Get messages dropped count
   * \return Count
   */
  uint32_t GetMessagesDropped (void) const;

  /**
   * \brief Get discovery cycles count
   * \return Count
   */
  uint32_t GetDiscoveryCycles (void) const;

  /**
   * \brief Increment sent counter
   */
  void IncrementSent (void);

  /**
   * \brief Increment received counter
   */
  void IncrementReceived (void);

  /**
   * \brief Increment forwarded counter
   */
  void IncrementForwarded (void);

  /**
   * \brief Increment dropped counter
   */
  void IncrementDropped (void);

  

  /**
   * \brief Get node ID
   * \return Node ID
   */
  uint32_t GetNodeId (void) const;

  

  /**
   * \brief Get reference to underlying C node structure
   * \return Reference to C node
   */
  const ble_mesh_node_t& GetCNode (void) const { return m_node; }

  

  /**
   * \brief TracedCallback for state changes
   */
  typedef void (*StateChangeCallback)(uint32_t nodeId,
                                       ble_node_state_t oldState,
                                       ble_node_state_t newState);

private:
  ble_mesh_node_t m_node;  //!< C node structure

  /**
   * \brief State change traced callback
   */
  TracedCallback<uint32_t, ble_node_state_t, ble_node_state_t> m_stateChangeTrace;

  /**
   * \brief GPS enabled configuration flag
   */
  bool m_gpsEnabled;
};

} 

#endif 
