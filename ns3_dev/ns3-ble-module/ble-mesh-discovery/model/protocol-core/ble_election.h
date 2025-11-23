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

/* Maximum number of neighbors to track */
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
    bool is_direct;                      /**< True if heard during direct-neighbor phase */
} ble_election_neighbor_info_t;

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

#define BLE_RSSI_BUFFER_SIZE 100         /**< Max RSSI samples stored per measurement */

/**
 * @brief Clusterhead election state
 */
typedef struct {
    ble_election_neighbor_info_t neighbors[BLE_MAX_NEIGHBORS]; /**< Neighbor database */
    uint32_t neighbor_count;             /**< Current number of neighbors */

    ble_connectivity_metrics_t metrics;  /**< Connectivity metrics */

    int8_t rssi_samples[BLE_RSSI_BUFFER_SIZE]; /**< Circular buffer of RSSI samples */
    uint32_t rssi_head;                  /**< Index of oldest sample */
    uint32_t rssi_tail;                  /**< Index where next sample goes */
    uint32_t rssi_count;                 /**< Current number of samples */
    bool crowding_measurement_active;    /**< True while noisy window is active */
    double last_crowding_factor;         /**< Most recent finalized crowding value */

    bool is_candidate;                   /**< Whether node is candidate */
    double candidacy_score;              /**< Candidacy score */

    /* Thresholds (configurable) */
    uint32_t min_neighbors_for_candidacy; /**< Minimum direct neighbors */
    double min_connection_noise_ratio;   /**< Minimum ratio for candidacy */
    double min_geographic_distribution;  /**< Reserved for future use (distribution stored but not gated) */
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
 * @param current_time_ms Current time in milliseconds
 */
void ble_election_add_rssi_sample(ble_election_state_t *state, int8_t rssi, uint32_t current_time_ms);

/**
 * @brief Calculate crowding factor from RSSI samples
 * @param state Election state
 * @return Crowding factor (0.0 = not crowded, 1.0 = very crowded)
 */
double ble_election_calculate_crowding(const ble_election_state_t *state);

/**
 * @brief Reset RSSI sample buffer (clears current measurements)
 * @param state Election state
 */
void ble_election_reset_rssi_samples(ble_election_state_t *state);

/**
 * @brief Begin a noisy broadcast crowding measurement window
 * @param state Election state
 * @param window_ms Measurement duration (ms); sets max-age for samples
 */
void ble_election_begin_crowding_measurement(ble_election_state_t *state, uint32_t window_ms);

/**
 * @brief Finalize current crowding measurement and cache the result
 * @param state Election state
 * @return Final crowding factor (0.0-1.0)
 */
double ble_election_end_crowding_measurement(ble_election_state_t *state);

/**
 * @brief Check whether a crowding measurement window is active
 * @param state Election state
 * @return true if actively collecting RSSI samples
 */
bool ble_election_is_crowding_measurement_active(const ble_election_state_t *state);

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
const ble_election_neighbor_info_t* ble_election_get_neighbor(const ble_election_state_t *state,
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

#endif /* BLE_ELECTION_H */
