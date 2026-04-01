/**
 * @file ble_forwarding_logic.c
 * @brief Pure C implementation of BLE mesh discovery forwarding logic
 */

#include "ble_forwarding_logic.h"
#include <math.h>
#include <limits.h>

/* Internal RNG state for probabilistic decisions */
static uint32_t g_forwarding_rng_state = 0x6d2b79f5u;

static double
ble_forwarding_random_value(void)
{
    /* xorshift32 */
    g_forwarding_rng_state ^= g_forwarding_rng_state << 13;
    g_forwarding_rng_state ^= g_forwarding_rng_state >> 17;
    g_forwarding_rng_state ^= g_forwarding_rng_state << 5;

    return (double)g_forwarding_rng_state / (double)UINT32_MAX;
}

void
ble_forwarding_set_random_seed(uint32_t seed)
{
    if (seed == 0) {
        g_forwarding_rng_state = 0x6d2b79f5u;
    } else {
        g_forwarding_rng_state = seed;
    }
}

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
    const double RSSI_MIN = -90.0;  /* Weakest signal considered */
    const double RSSI_MAX = -40.0;  /* Strongest signal considered */

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

double
ble_forwarding_calculate_noise_level(const int8_t *rssi_samples,
                                       uint32_t num_samples)
{
    double crowding = ble_forwarding_calculate_crowding_factor(rssi_samples, num_samples);
    return crowding * 100.0;
}

bool
ble_forwarding_should_forward_crowding(double crowding_factor,
                                         uint32_t direct_neighbors)
{
    const double CROWDING_LOW = 0.1;
    const double CROWDING_HIGH = 0.9;

    double clamped_crowding = crowding_factor;
    if (clamped_crowding < 0.0) {
        clamped_crowding = 0.0;
    } else if (clamped_crowding > 1.0) {
        clamped_crowding = 1.0;
    }

    uint32_t neighbors = (direct_neighbors == 0) ? 1 : direct_neighbors;
    double base_probability = 2.0 / (double)neighbors;
    if (base_probability > 1.0) {
        base_probability = 1.0;
    }

    double forward_probability;
    if (clamped_crowding <= CROWDING_LOW) {
        forward_probability = 1.0;
    } else if (clamped_crowding >= CROWDING_HIGH) {
        forward_probability = base_probability;
    } else {
        double t = (clamped_crowding - CROWDING_LOW) / (CROWDING_HIGH - CROWDING_LOW);
        forward_probability = 1.0 + t * (base_probability - 1.0);
    }

    double random_value = ble_forwarding_random_value();
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
                                uint32_t direct_neighbors)
{
    if (!packet) {
        return false;
    }

    /* Check TTL (must be > 0) */
    if (packet->ttl == 0) {
        return false;
    }

    /* Check crowding factor (picky forwarding) */
    if (!ble_forwarding_should_forward_crowding(crowding_factor, direct_neighbors)) {
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
