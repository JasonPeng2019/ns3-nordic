/**
 * @file ble_forwarding_logic.h
 * @brief Pure C implementation of BLE mesh discovery forwarding logic
 *
 * This module implements the 3-metric forwarding algorithm:
 * 1. Picky Forwarding (crowding factor based filtering)
 * 2. GPS Proximity Filtering
 * 3. TTL-Based Prioritization
 *
 * NO dependencies on NS-3 or C++ standard library - portable to embedded systems.
 */

#ifndef BLE_FORWARDING_LOGIC_H
#define BLE_FORWARDING_LOGIC_H

#include <stdint.h>
#include <stdbool.h>
#include "ble_discovery_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Seed the internal forwarding RNG
 * @param seed New seed (0 selects default)
 */
void ble_forwarding_set_random_seed(uint32_t seed);

/**
 * @brief Calculate crowding factor from RSSI measurements
 * @param rssi_samples Array of RSSI samples (dBm)
 * @param num_samples Number of samples
 * @return Crowding factor (0.0 to 1.0, higher = more crowded)
 */
double ble_forwarding_calculate_crowding_factor(const int8_t *rssi_samples,
                                                  uint32_t num_samples);

/**
 * @brief Convert a single RSSI reading to a normalized noise level (0-100)
 * @param rssi_samples Array of RSSI samples (dBm)
 * @param num_samples Number of samples
 * @return Noise level scaled 0 (quiet) to 100 (very noisy)
 */
double ble_forwarding_calculate_noise_level(const int8_t *rssi_samples,
                                              uint32_t num_samples);

/**
 * @brief Determine if message should be forwarded based on crowding factor
 *
 * Uses picky forwarding algorithm with direct-neighbor awareness.
 *
 * @param crowding_factor Crowding factor (0.0 to 1.0)
 * @param direct_neighbors Number of direct neighbors observed
 * @return true if message should be forwarded
 */
bool ble_forwarding_should_forward_crowding(double crowding_factor,
                                              uint32_t direct_neighbors);

/**
 * @brief Calculate distance between two GPS coordinates (meters)
 * @param loc1 First GPS location
 * @param loc2 Second GPS location
 * @return Distance in meters
 */
double ble_forwarding_calculate_distance(const ble_gps_location_t *loc1,
                                           const ble_gps_location_t *loc2);

/**
 * @brief Determine if message should be forwarded based on GPS proximity
 *
 * Forwards if distance to last hop exceeds proximity threshold
 *
 * @param current_location This node's GPS location
 * @param last_hop_location Last hop GPS location (LHGPS)
 * @param proximity_threshold Minimum distance for forwarding (meters)
 * @return true if message should be forwarded (distance > threshold)
 */
bool ble_forwarding_should_forward_proximity(const ble_gps_location_t *current_location,
                                               const ble_gps_location_t *last_hop_location,
                                               double proximity_threshold);

/**
 * @brief Determine if message should be forwarded based on all three metrics
 *
 * Combines:
 * - Picky forwarding (crowding factor)
 * - GPS proximity filtering
 * - TTL check (TTL must be > 0)
 *
 * @param packet Discovery packet
 * @param current_location This node's GPS location
 * @param crowding_factor Local crowding factor
 * @param proximity_threshold GPS proximity threshold (meters)
 * @param direct_neighbors Number of direct neighbors observed
 * @return true if message should be forwarded
 */
bool ble_forwarding_should_forward(const ble_discovery_packet_t *packet,
                                     const ble_gps_location_t *current_location,
                                     double crowding_factor,
                                     double proximity_threshold,
                                     uint32_t direct_neighbors);

/**
 * @brief Calculate forwarding priority (lower = higher priority)
 *
 * Priority is primarily based on TTL (higher TTL = higher priority)
 *
 * @param ttl Time To Live value
 * @return Priority value (0 = highest, 255 = lowest)
 */
uint8_t ble_forwarding_calculate_priority(uint8_t ttl);

#ifdef __cplusplus
}
#endif

#endif /* BLE_FORWARDING_LOGIC_H */
