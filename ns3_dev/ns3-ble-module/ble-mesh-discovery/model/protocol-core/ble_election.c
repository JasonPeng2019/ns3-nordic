/**
 * @file ble_election.c
 * @brief Pure C implementation of BLE mesh clusterhead election
 */

#include "ble_election.h"
#include <string.h>
#include <math.h>

/* RSSI threshold for considering a connection "direct" (1-hop) */
#define DEFAULT_DIRECT_RSSI_THRESHOLD -70  /* dBm */

/* Default candidacy thresholds */
#define DEFAULT_MIN_NEIGHBORS 10
#define DEFAULT_MIN_CN_RATIO 5.0
#define DEFAULT_MIN_GEO_DIST 0.3

void
ble_election_init(ble_election_state_t *state)
{
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(ble_election_state_t));

    /* Set default thresholds */
    state->min_neighbors_for_candidacy = DEFAULT_MIN_NEIGHBORS;
    state->min_connection_noise_ratio = DEFAULT_MIN_CN_RATIO;
    state->min_geographic_distribution = DEFAULT_MIN_GEO_DIST;
    state->direct_connection_rssi_threshold = DEFAULT_DIRECT_RSSI_THRESHOLD;
}

void
ble_election_update_neighbor(ble_election_state_t *state,
                               uint32_t node_id,
                               const ble_gps_location_t *location,
                               int8_t rssi,
                               uint32_t current_time_ms)
{
    if (!state) {
        return;
    }

    /* Find existing neighbor or add new one */
    ble_neighbor_info_t *neighbor = NULL;
    for (uint32_t i = 0; i < state->neighbor_count; i++) {
        if (state->neighbors[i].node_id == node_id) {
            neighbor = &state->neighbors[i];
            break;
        }
    }

    if (!neighbor && state->neighbor_count < BLE_MAX_NEIGHBORS) {
        /* Add new neighbor */
        neighbor = &state->neighbors[state->neighbor_count++];
        neighbor->node_id = node_id;
        neighbor->message_count = 0;
    }

    if (neighbor) {
        /* Update neighbor info */
        if (location) {
            neighbor->location = *location;
        }
        neighbor->rssi = rssi;
        neighbor->message_count++;
        neighbor->last_seen_time_ms = current_time_ms;

        /* Determine if direct connection based on RSSI */
        neighbor->is_direct = (rssi >= state->direct_connection_rssi_threshold);
    }
}

void
ble_election_add_rssi_sample(ble_election_state_t *state, int8_t rssi)
{
    if (!state) {
        return;
    }

    if (state->rssi_sample_count < 100) {
        state->rssi_samples[state->rssi_sample_count++] = rssi;
    } else {
        /* Circular buffer - overwrite oldest */
        for (uint32_t i = 0; i < 99; i++) {
            state->rssi_samples[i] = state->rssi_samples[i + 1];
        }
        state->rssi_samples[99] = rssi;
    }
}

double
ble_election_calculate_crowding(const ble_election_state_t *state)
{
    if (!state || state->rssi_sample_count == 0) {
        return 0.0;
    }

    /* Calculate mean RSSI */
    double sum = 0.0;
    for (uint32_t i = 0; i < state->rssi_sample_count; i++) {
        sum += state->rssi_samples[i];
    }
    double mean_rssi = sum / state->rssi_sample_count;

    /* Convert RSSI to crowding factor
     * Higher (less negative) RSSI = stronger signals = more crowded
     * -40 dBm = very crowded (1.0)
     * -90 dBm = not crowded (0.0)
     */
    const double RSSI_MIN = -90.0;
    const double RSSI_MAX = -40.0;

    if (mean_rssi >= RSSI_MAX) {
        return 1.0;
    }
    if (mean_rssi <= RSSI_MIN) {
        return 0.0;
    }

    return (mean_rssi - RSSI_MIN) / (RSSI_MAX - RSSI_MIN);
}

uint32_t
ble_election_count_direct_connections(const ble_election_state_t *state)
{
    if (!state) {
        return 0;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < state->neighbor_count; i++) {
        if (state->neighbors[i].is_direct) {
            count++;
        }
    }

    return count;
}

double
ble_election_calculate_geographic_distribution(const ble_election_state_t *state)
{
    if (!state || state->neighbor_count < 2) {
        return 0.0; /* Not enough neighbors to determine distribution */
    }

    /* Calculate centroid */
    double centroid_x = 0.0, centroid_y = 0.0, centroid_z = 0.0;
    uint32_t valid_locations = 0;

    for (uint32_t i = 0; i < state->neighbor_count; i++) {
        /* Skip neighbors without valid GPS */
        if (state->neighbors[i].location.x != 0.0 ||
            state->neighbors[i].location.y != 0.0 ||
            state->neighbors[i].location.z != 0.0) {
            centroid_x += state->neighbors[i].location.x;
            centroid_y += state->neighbors[i].location.y;
            centroid_z += state->neighbors[i].location.z;
            valid_locations++;
        }
    }

    if (valid_locations < 2) {
        return 0.0; /* Not enough GPS data */
    }

    centroid_x /= valid_locations;
    centroid_y /= valid_locations;
    centroid_z /= valid_locations;

    /* Calculate variance of distances from centroid */
    double variance = 0.0;
    for (uint32_t i = 0; i < state->neighbor_count; i++) {
        const ble_gps_location_t *loc = &state->neighbors[i].location;

        if (loc->x != 0.0 || loc->y != 0.0 || loc->z != 0.0) {
            double dx = loc->x - centroid_x;
            double dy = loc->y - centroid_y;
            double dz = loc->z - centroid_z;
            double distance = sqrt(dx * dx + dy * dy + dz * dz);
            variance += distance * distance;
        }
    }

    variance /= valid_locations;

    /* Normalize variance to 0.0-1.0 range
     * Higher variance = better distribution
     * Assume max reasonable variance is 10000 (100m std dev)
     */
    double std_dev = sqrt(variance);
    double distribution = std_dev / 100.0;

    if (distribution > 1.0) {
        distribution = 1.0;
    }

    return distribution;
}

void
ble_election_update_metrics(ble_election_state_t *state)
{
    if (!state) {
        return;
    }

    /* Update connectivity metrics */
    state->metrics.direct_connections = ble_election_count_direct_connections(state);
    state->metrics.total_neighbors = state->neighbor_count;
    state->metrics.crowding_factor = ble_election_calculate_crowding(state);
    state->metrics.connection_noise_ratio =
        state->metrics.direct_connections / (1.0 + state->metrics.crowding_factor);
    state->metrics.geographic_distribution =
        ble_election_calculate_geographic_distribution(state);

    /* Calculate forwarding success rate */
    if (state->metrics.messages_received > 0) {
        state->metrics.forwarding_success_rate =
            (double)state->metrics.messages_forwarded / state->metrics.messages_received;
    } else {
        state->metrics.forwarding_success_rate = 0.0;
    }
}

double
ble_election_calculate_candidacy_score(const ble_election_state_t *state)
{
    if (!state) {
        return 0.0;
    }

    /* Score formula (weighted combination):
     * Score = w1 * direct_connections +
     *         w2 * connection_noise_ratio +
     *         w3 * geographic_distribution +
     *         w4 * forwarding_success_rate
     */

    const double w1 = 1.0;   /* Weight for direct connections */
    const double w2 = 2.0;   /* Weight for CN ratio (most important) */
    const double w3 = 1.5;   /* Weight for geographic distribution */
    const double w4 = 1.0;   /* Weight for forwarding success */

    double score = w1 * state->metrics.direct_connections +
                   w2 * state->metrics.connection_noise_ratio +
                   w3 * state->metrics.geographic_distribution * 10.0 +
                   w4 * state->metrics.forwarding_success_rate * 10.0;

    return score;
}

bool
ble_election_should_become_candidate(ble_election_state_t *state)
{
    if (!state) {
        return false;
    }

    /* Update metrics before checking */
    ble_election_update_metrics(state);

    /* Check minimum direct neighbors */
    if (state->metrics.direct_connections < state->min_neighbors_for_candidacy) {
        return false;
    }

    /* Check connection:noise ratio */
    if (state->metrics.connection_noise_ratio < state->min_connection_noise_ratio) {
        return false;
    }

    /* Check geographic distribution (only if GPS available) */
    if (state->neighbor_count >= 2) {
        if (state->metrics.geographic_distribution < state->min_geographic_distribution) {
            return false;
        }
    }

    /* All criteria met */
    state->is_candidate = true;
    state->candidacy_score = ble_election_calculate_candidacy_score(state);

    return true;
}

void
ble_election_set_thresholds(ble_election_state_t *state,
                              uint32_t min_neighbors,
                              double min_cn_ratio,
                              double min_geo_dist)
{
    if (!state) {
        return;
    }

    state->min_neighbors_for_candidacy = min_neighbors;
    state->min_connection_noise_ratio = min_cn_ratio;
    state->min_geographic_distribution = min_geo_dist;
}

const ble_neighbor_info_t*
ble_election_get_neighbor(const ble_election_state_t *state, uint32_t node_id)
{
    if (!state) {
        return NULL;
    }

    for (uint32_t i = 0; i < state->neighbor_count; i++) {
        if (state->neighbors[i].node_id == node_id) {
            return &state->neighbors[i];
        }
    }

    return NULL;
}

uint32_t
ble_election_clean_old_neighbors(ble_election_state_t *state,
                                   uint32_t current_time_ms,
                                   uint32_t timeout_ms)
{
    if (!state) {
        return 0;
    }

    uint32_t removed = 0;
    uint32_t new_count = 0;

    for (uint32_t i = 0; i < state->neighbor_count; i++) {
        uint32_t age = current_time_ms - state->neighbors[i].last_seen_time_ms;

        if (age <= timeout_ms) {
            /* Keep this neighbor */
            if (new_count != i) {
                state->neighbors[new_count] = state->neighbors[i];
            }
            new_count++;
        } else {
            removed++;
        }
    }

    state->neighbor_count = new_count;
    return removed;
}
