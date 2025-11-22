/**
 * @file ble_forwarding_logic.c
 * @brief Pure C implementation of BLE mesh discovery forwarding logic
 */

#include "ble_forwarding_logic.h"
#include <math.h>

/* Helper function: Calculate mean of RSSI samples */
static double
calculate_mean_rssi(const int8_t *samples, uint32_t count)
{
    if (!samples || count == 0) {
        return 0.0;
    }

    double sum = 0.0;
    for (uint32_t i = 0; i < count; i++) {
        sum += samples[i];
    }

    return sum / count;
}

double
ble_forwarding_calculate_crowding_factor(const int8_t *rssi_samples,
                                           uint32_t num_samples)
{
    if (!rssi_samples || num_samples == 0) {
        return 0.0; /* No crowding if no samples */
    }

    /* Calculate mean RSSI */
    double mean_rssi = calculate_mean_rssi(rssi_samples, num_samples);

    /* Convert RSSI to crowding factor
     * Higher (less negative) RSSI = stronger signals = more crowded
     * Example: -40 dBm = very crowded, -90 dBm = not crowded
     *
     * Normalize to 0.0 - 1.0 range:
     * - RSSI >= -40 dBm: crowding = 1.0 (very crowded)
     * - RSSI <= -90 dBm: crowding = 0.0 (not crowded)
     */

    const double RSSI_MIN = -90.0;  /* Weakest signal considered */
    const double RSSI_MAX = -40.0;  /* Strongest signal considered */

    if (mean_rssi >= RSSI_MAX) {
        return 1.0;
    }
    if (mean_rssi <= RSSI_MIN) {
        return 0.0;
    }

    /* Linear interpolation */
    double crowding = (mean_rssi - RSSI_MIN) / (RSSI_MAX - RSSI_MIN);

    return crowding;
}

bool
ble_forwarding_should_forward_crowding(double crowding_factor,
                                         double random_value)
{
    /* Picky forwarding algorithm:
     * - High crowding (e.g., 0.9) → forward only 10% of messages
     * - Low crowding (e.g., 0.1) → forward 90% of messages
     * - Forwarding probability = 1.0 - crowding_factor
     */

    double forward_probability = 1.0 - crowding_factor;

    return random_value < forward_probability;
}

double
ble_forwarding_calculate_distance(const ble_gps_location_t *loc1,
                                    const ble_gps_location_t *loc2)
{
    if (!loc1 || !loc2) {
        return 0.0;
    }

    /* Simple Euclidean distance in 3D space (assuming x, y, z in meters) */
    double dx = loc2->x - loc1->x;
    double dy = loc2->y - loc1->y;
    double dz = loc2->z - loc1->z;

    return sqrt(dx * dx + dy * dy + dz * dz);
}

bool
ble_forwarding_should_forward_proximity(const ble_gps_location_t *current_location,
                                          const ble_gps_location_t *last_hop_location,
                                          double proximity_threshold)
{
    if (!current_location || !last_hop_location) {
        /* If GPS not available, skip proximity check */
        return true;
    }

    /* Calculate distance */
    double distance = ble_forwarding_calculate_distance(current_location,
                                                         last_hop_location);

    /* Forward only if distance exceeds threshold */
    return distance > proximity_threshold;
}

bool
ble_forwarding_should_forward(const ble_discovery_packet_t *packet,
                                const ble_gps_location_t *current_location,
                                double crowding_factor,
                                double proximity_threshold,
                                double random_value)
{
    if (!packet) {
        return false;
    }

    /* Check TTL (must be > 0) */
    if (packet->ttl == 0) {
        return false;
    }

    /* Check crowding factor (picky forwarding) */
    if (!ble_forwarding_should_forward_crowding(crowding_factor, random_value)) {
        return false;
    }

    /* Check GPS proximity (if GPS available) */
    if (packet->gps_available && current_location) {
        if (!ble_forwarding_should_forward_proximity(current_location,
                                                       &packet->gps_location,
                                                       proximity_threshold)) {
            return false;
        }
    }

    /* All checks passed */
    return true;
}

uint8_t
ble_forwarding_calculate_priority(uint8_t ttl)
{
    /* Higher TTL = higher priority (lower priority number)
     * Priority range: 0 (highest) to 255 (lowest)
     */

    if (ttl == 0) {
        return 255; /* Lowest priority for expired messages */
    }

    /* Invert TTL: higher TTL gets lower priority number */
    return 255 - ttl;
}
