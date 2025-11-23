/**
 * @file ble_election.h
 * @brief Pure C implementation of BLE mesh clusterhead election
 *
 * This module implements Tasks 12-18 (Phase 3):
 * - Noisy broadcast phase
 * - RSSI-based crowding factor
 * - Direct connection counting
 * - Connectivity metric tracking
 * - Candidacy determination
 * - Geographic distribution analysis
 *
 * NO dependencies on NS-3 or C++ - portable to embedded systems.
 */

#ifndef BLE_ELECTION_H
#define BLE_ELECTION_H

#include <stdint.h>
#include <stdbool.h>
#include "ble_discovery_packet.h"

#ifdef __cplusplus
extern "C" {
#endif


#define BLE_MAX_NEIGHBORS 150

/**
 * @brief Neighbor information structure
 */
typedef struct {
    uint32_t node_id;                    /**< Neighbor node ID */
    ble_gps_location_t location;         /**< Last known GPS location */
    int8_t rssi;                         /**< Last RSSI measurement (dBm) */
    uint32_t message_count;              /**< Messages received from this neighbor */
    uint32_t last_seen_time_ms;          /**< Last time we heard from neighbor */
    bool is_direct;                      /**< True if 1-hop neighbor (strong signal) */
} ble_neighbor_info_t;

/**
 * @brief Connectivity metrics for candidacy determination
 */
typedef struct {
    uint32_t direct_connections;         /**< Number of direct (1-hop) neighbors */
    uint32_t total_neighbors;            /**< Total unique neighbors */
    double crowding_factor;              /**< Local crowding (0.0-1.0) */
    double connection_noise_ratio;       /**< Direct connections / (1 + crowding) */
    double geographic_distribution;      /**< Spatial distribution score (0.0-1.0) */
    uint32_t messages_forwarded;         /**< Successfully forwarded messages */
    uint32_t messages_received;          /**< Total messages received */
    double forwarding_success_rate;      /**< Forwarding success ratio */
} ble_connectivity_metrics_t;

/**
 * @brief Clusterhead election state
 */
typedef struct {
    ble_neighbor_info_t neighbors[BLE_MAX_NEIGHBORS]; /**< Neighbor database */
    uint32_t neighbor_count;             /**< Current number of neighbors */

    ble_connectivity_metrics_t metrics;  /**< Connectivity metrics */

    int8_t rssi_samples[100];            /**< Recent RSSI samples for crowding */
    uint32_t rssi_sample_count;          /**< Number of RSSI samples */

    bool is_candidate;                   /**< Whether node is candidate */
    double candidacy_score;              /**< Candidacy score */
    ble_score_weights_t score_weights;   /**< Configurable score weights */

    
    uint32_t min_neighbors_for_candidacy; /**< Minimum direct neighbors */
    double min_connection_noise_ratio;   /**< Minimum ratio for candidacy */
    double min_geographic_distribution;  /**< Minimum distribution score */
    int8_t direct_connection_rssi_threshold; /**< RSSI threshold for direct connection (dBm) */
} ble_election_state_t;

/**
 * @brief Initialize election state
 * @param state Pointer to election state structure
 */
void ble_election_init(ble_election_state_t *state);

/**
 * @brief Add or update neighbor information
 * @param state Election state
 * @param node_id Neighbor node ID
 * @param location GPS location (can be NULL if unavailable)
 * @param rssi RSSI measurement (dBm)
 * @param current_time_ms Current time in milliseconds
 */
void ble_election_update_neighbor(ble_election_state_t *state,
                                    uint32_t node_id,
                                    const ble_gps_location_t *location,
                                    int8_t rssi,
                                    uint32_t current_time_ms);

/**
 * @brief Add RSSI sample for crowding factor calculation
 * @param state Election state
 * @param rssi RSSI sample (dBm)
 */
void ble_election_add_rssi_sample(ble_election_state_t *state, int8_t rssi);

/**
 * @brief Calculate crowding factor from RSSI samples
 * @param state Election state
 * @return Crowding factor (0.0 = not crowded, 1.0 = very crowded)
 */
double ble_election_calculate_crowding(const ble_election_state_t *state);

/**
 * @brief Count direct connections (1-hop neighbors with strong signal)
 * @param state Election state
 * @return Number of direct connections
 */
uint32_t ble_election_count_direct_connections(const ble_election_state_t *state);

/**
 * @brief Calculate geographic distribution of neighbors
 *
 * Returns a score from 0.0 (all clustered) to 1.0 (well distributed)
 * Uses variance of distances from centroid.
 *
 * @param state Election state
 * @return Distribution score (0.0-1.0)
 */
double ble_election_calculate_geographic_distribution(const ble_election_state_t *state);

/**
 * @brief Update connectivity metrics
 * @param state Election state
 */
void ble_election_update_metrics(ble_election_state_t *state);

/**
 * @brief Set weights for candidacy score calculation
 * @param state Election state
 * @param weights Optional weights (NULL resets to defaults)
 */
void ble_election_set_score_weights(ble_election_state_t *state,
                                      const ble_score_weights_t *weights);

/**
 * @brief Calculate candidacy score
 *
 * Score is based on:
 * - Direct connection count
 * - Connection:noise ratio
 * - Geographic distribution
 * - Forwarding success rate
 *
 * @param state Election state
 * @return Candidacy score (higher = better candidate)
 */
double ble_election_calculate_candidacy_score(const ble_election_state_t *state);

/**
 * @brief Determine if node should become clusterhead candidate
 *
 * Checks:
 * - Minimum direct neighbors threshold
 * - Connection:noise ratio threshold
 * - Geographic distribution threshold
 * - Successful forwarding requirement
 *
 * @param state Election state
 * @return true if node should become candidate
 */
bool ble_election_should_become_candidate(ble_election_state_t *state);

/**
 * @brief Set candidacy thresholds
 * @param state Election state
 * @param min_neighbors Minimum direct neighbors
 * @param min_cn_ratio Minimum connection:noise ratio
 * @param min_geo_dist Minimum geographic distribution
 */
void ble_election_set_thresholds(ble_election_state_t *state,
                                   uint32_t min_neighbors,
                                   double min_cn_ratio,
                                   double min_geo_dist);

/**
 * @brief Get neighbor by ID
 * @param state Election state
 * @param node_id Node ID to find
 * @return Pointer to neighbor info, or NULL if not found
 */
const ble_neighbor_info_t* ble_election_get_neighbor(const ble_election_state_t *state,
                                                       uint32_t node_id);

/**
 * @brief Clean old neighbors (not heard from recently)
 * @param state Election state
 * @param current_time_ms Current time in milliseconds
 * @param timeout_ms Timeout in milliseconds
 * @return Number of neighbors removed
 */
uint32_t ble_election_clean_old_neighbors(ble_election_state_t *state,
                                            uint32_t current_time_ms,
                                            uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif 
