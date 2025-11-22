/**
 * @file ble_mesh_node.c
 * @brief Pure C implementation of BLE Mesh Node state machine
 * @author Benjamin Huh
 * @date 2025-11-21
 */

#include "ble_mesh_node.h"
#include <string.h>

/* ===== Node Initialization ===== */

void ble_mesh_node_init(ble_mesh_node_t *node, uint32_t node_id)
{
    if (!node) return;

    memset(node, 0, sizeof(ble_mesh_node_t));

    node->node_id = node_id;
    node->state = BLE_NODE_STATE_INIT;
    node->prev_state = BLE_NODE_STATE_INIT;
    node->state_entry_cycle = 0;

    node->gps_available = false;
    node->gps_location.x = 0.0;
    node->gps_location.y = 0.0;
    node->gps_location.z = 0.0;
    node->gps_last_update_cycle = 0;
    node->gps_cache_ttl = 0; // No expiration by default

    node->clusterhead_id = BLE_MESH_INVALID_NODE_ID;
    node->cluster_class = 0;

    node->pdsf = 0;
    node->candidacy_score = 0.0;
    node->election_hash = ble_election_generate_hash(node_id);

    node->current_cycle = 0;

    node->neighbors.count = 0;

    memset(&node->stats, 0, sizeof(ble_node_statistics_t));
}

/* ===== GPS Management ===== */

void ble_mesh_node_set_gps(ble_mesh_node_t *node, double x, double y, double z)
{
    if (!node) return;

    node->gps_location.x = x;
    node->gps_location.y = y;
    node->gps_location.z = z;
    node->gps_available = true;
    node->gps_last_update_cycle = node->current_cycle;
}

void ble_mesh_node_clear_gps(ble_mesh_node_t *node)
{
    if (!node) return;
    node->gps_available = false;
}

void ble_mesh_node_set_gps_cache_ttl(ble_mesh_node_t *node, uint32_t ttl_cycles)
{
    if (!node) return;
    node->gps_cache_ttl = ttl_cycles;
}

bool ble_mesh_node_is_gps_cache_valid(const ble_mesh_node_t *node)
{
    if (!node || !node->gps_available) {
        return false;
    }

    // If TTL is 0, cache never expires
    if (node->gps_cache_ttl == 0) {
        return true;
    }

    // Check if cache has expired
    uint32_t age = node->current_cycle - node->gps_last_update_cycle;
    return (age < node->gps_cache_ttl);
}

void ble_mesh_node_invalidate_gps_cache(ble_mesh_node_t *node)
{
    if (!node) return;

    // Set last update cycle to force expiration
    // Make age >= TTL to invalidate cache
    if (node->gps_cache_ttl > 0) {
        // Set to current cycle - TTL to make age exactly TTL (expired)
        if (node->current_cycle >= node->gps_cache_ttl) {
            node->gps_last_update_cycle = node->current_cycle - node->gps_cache_ttl;
        } else {
            node->gps_last_update_cycle = 0;
        }
        // Also mark GPS as unavailable to force refresh
        node->gps_available = false;
    }
}

uint32_t ble_mesh_node_get_gps_age(const ble_mesh_node_t *node)
{
    if (!node) return 0;

    if (node->current_cycle >= node->gps_last_update_cycle) {
        return node->current_cycle - node->gps_last_update_cycle;
    }
    return 0;
}

/* ===== State Management ===== */

ble_node_state_t ble_mesh_node_get_state(const ble_mesh_node_t *node)
{
    if (!node) return BLE_NODE_STATE_INIT;
    return node->state;
}

bool ble_mesh_node_is_valid_transition(ble_node_state_t current, ble_node_state_t new_state)
{
    // Allow staying in same state
    if (current == new_state) {
        return true;
    }

    switch (current) {
        case BLE_NODE_STATE_INIT:
            // From INIT, can only go to DISCOVERY
            return (new_state == BLE_NODE_STATE_DISCOVERY);

        case BLE_NODE_STATE_DISCOVERY:
            // From DISCOVERY, can go to EDGE, CANDIDATE, or stay
            return (new_state == BLE_NODE_STATE_EDGE ||
                    new_state == BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE);

        case BLE_NODE_STATE_EDGE:
            // Edge nodes can become candidates if conditions change
            return (new_state == BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE ||
                    new_state == BLE_NODE_STATE_CLUSTER_MEMBER);

        case BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE:
            // Candidates can become clusterheads or members
            return (new_state == BLE_NODE_STATE_CLUSTERHEAD ||
                    new_state == BLE_NODE_STATE_CLUSTER_MEMBER ||
                    new_state == BLE_NODE_STATE_EDGE);

        case BLE_NODE_STATE_CLUSTERHEAD:
            // Clusterheads typically stay as clusterheads
            // (but could transition if needed)
            return (new_state == BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE);

        case BLE_NODE_STATE_CLUSTER_MEMBER:
            // Members can become candidates if clusterhead fails
            return (new_state == BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE ||
                    new_state == BLE_NODE_STATE_EDGE);

        default:
            return false;
    }
}

bool ble_mesh_node_set_state(ble_mesh_node_t *node, ble_node_state_t new_state)
{
    if (!node) return false;

    if (!ble_mesh_node_is_valid_transition(node->state, new_state)) {
        return false;
    }

    node->prev_state = node->state;
    node->state = new_state;
    node->state_entry_cycle = node->current_cycle;

    return true;
}

const char* ble_mesh_node_state_name(ble_node_state_t state)
{
    switch (state) {
        case BLE_NODE_STATE_INIT:
            return "INIT";
        case BLE_NODE_STATE_DISCOVERY:
            return "DISCOVERY";
        case BLE_NODE_STATE_EDGE:
            return "EDGE";
        case BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE:
            return "CLUSTERHEAD_CANDIDATE";
        case BLE_NODE_STATE_CLUSTERHEAD:
            return "CLUSTERHEAD";
        case BLE_NODE_STATE_CLUSTER_MEMBER:
            return "CLUSTER_MEMBER";
        default:
            return "UNKNOWN";
    }
}

/* ===== Cycle Management ===== */

void ble_mesh_node_advance_cycle(ble_mesh_node_t *node)
{
    if (!node) return;
    node->current_cycle++;
    node->stats.discovery_cycles++;
}

/* ===== Neighbor Management ===== */

ble_neighbor_info_t* ble_mesh_node_find_neighbor(ble_mesh_node_t *node, uint32_t neighbor_id)
{
    if (!node) return NULL;

    for (uint16_t i = 0; i < node->neighbors.count; i++) {
        if (node->neighbors.neighbors[i].node_id == neighbor_id) {
            return &node->neighbors.neighbors[i];
        }
    }
    return NULL;
}

bool ble_mesh_node_add_neighbor(ble_mesh_node_t *node,
                                  uint32_t neighbor_id,
                                  int8_t rssi,
                                  uint8_t hop_count)
{
    if (!node) return false;

    // Check if neighbor already exists
    ble_neighbor_info_t *existing = ble_mesh_node_find_neighbor(node, neighbor_id);
    if (existing) {
        // Update existing neighbor
        existing->rssi = rssi;
        existing->hop_count = hop_count;
        existing->last_seen_cycle = node->current_cycle;
        return true;
    }

    // Add new neighbor if space available
    if (node->neighbors.count >= BLE_MESH_MAX_NEIGHBORS) {
        return false; // Table full
    }

    ble_neighbor_info_t *new_neighbor = &node->neighbors.neighbors[node->neighbors.count];
    new_neighbor->node_id = neighbor_id;
    new_neighbor->rssi = rssi;
    new_neighbor->hop_count = hop_count;
    new_neighbor->last_seen_cycle = node->current_cycle;
    new_neighbor->is_clusterhead = false;
    new_neighbor->clusterhead_class = 0;
    new_neighbor->gps_valid = false;

    node->neighbors.count++;
    return true;
}

bool ble_mesh_node_update_neighbor_gps(ble_mesh_node_t *node,
                                         uint32_t neighbor_id,
                                         const ble_gps_location_t *gps)
{
    if (!node || !gps) return false;

    ble_neighbor_info_t *neighbor = ble_mesh_node_find_neighbor(node, neighbor_id);
    if (!neighbor) return false;

    neighbor->gps = *gps;
    neighbor->gps_valid = true;
    return true;
}

uint16_t ble_mesh_node_count_direct_neighbors(const ble_mesh_node_t *node)
{
    if (!node) return 0;

    uint16_t count = 0;
    for (uint16_t i = 0; i < node->neighbors.count; i++) {
        if (node->neighbors.neighbors[i].hop_count == 1) {
            count++;
        }
    }
    return count;
}

int8_t ble_mesh_node_calculate_avg_rssi(const ble_mesh_node_t *node)
{
    if (!node || node->neighbors.count == 0) return 0;

    int32_t sum = 0;
    for (uint16_t i = 0; i < node->neighbors.count; i++) {
        sum += node->neighbors.neighbors[i].rssi;
    }
    return (int8_t)(sum / node->neighbors.count);
}

uint16_t ble_mesh_node_prune_stale_neighbors(ble_mesh_node_t *node, uint32_t max_age)
{
    if (!node) return 0;

    uint16_t removed = 0;
    uint16_t write_idx = 0;

    for (uint16_t read_idx = 0; read_idx < node->neighbors.count; read_idx++) {
        uint32_t age = node->current_cycle - node->neighbors.neighbors[read_idx].last_seen_cycle;

        if (age <= max_age) {
            // Keep this neighbor
            if (write_idx != read_idx) {
                node->neighbors.neighbors[write_idx] = node->neighbors.neighbors[read_idx];
            }
            write_idx++;
        } else {
            // Remove (don't copy)
            removed++;
        }
    }

    node->neighbors.count = write_idx;
    return removed;
}

/* ===== Election & Decision Logic ===== */

double ble_mesh_node_calculate_candidacy_score(const ble_mesh_node_t *node,
                                                 double noise_level)
{
    if (!node) return 0.0;

    uint16_t direct_connections = ble_mesh_node_count_direct_neighbors(node);

    return ble_election_calculate_score(direct_connections,
                                          noise_level);
}

bool ble_mesh_node_should_become_edge(const ble_mesh_node_t *node)
{
    if (!node) return false;

    // Node becomes edge if:
    // 1. Very few direct neighbors (< 3)
    // 2. OR average RSSI is very weak
    uint16_t direct_neighbors = ble_mesh_node_count_direct_neighbors(node);
    int8_t avg_rssi = ble_mesh_node_calculate_avg_rssi(node);

    return (direct_neighbors < 3 || avg_rssi < BLE_MESH_EDGE_RSSI_THRESHOLD);
}

bool ble_mesh_node_should_become_candidate(const ble_mesh_node_t *node)
{
    if (!node) return false;

    // Node becomes candidate if:
    // 1. Has good connectivity (>= 5 direct neighbors)
    // 2. AND not already at capacity (< 150 total neighbors)
    // 3. AND average RSSI is reasonable
    uint16_t direct_neighbors = ble_mesh_node_count_direct_neighbors(node);
    int8_t avg_rssi = ble_mesh_node_calculate_avg_rssi(node);

    return (direct_neighbors >= 5 &&
            node->neighbors.count < BLE_MESH_MAX_NEIGHBORS &&
            avg_rssi >= BLE_MESH_EDGE_RSSI_THRESHOLD);
}

/* ===== Statistics ===== */

void ble_mesh_node_update_statistics(ble_mesh_node_t *node)
{
    if (!node) return;

    node->stats.avg_rssi = ble_mesh_node_calculate_avg_rssi(node);
    node->stats.direct_connections = ble_mesh_node_count_direct_neighbors(node);
}

void ble_mesh_node_inc_sent(ble_mesh_node_t *node)
{
    if (!node) return;
    node->stats.messages_sent++;
}

void ble_mesh_node_inc_received(ble_mesh_node_t *node)
{
    if (!node) return;
    node->stats.messages_received++;
}

void ble_mesh_node_inc_forwarded(ble_mesh_node_t *node)
{
    if (!node) return;
    node->stats.messages_forwarded++;
}

void ble_mesh_node_inc_dropped(ble_mesh_node_t *node)
{
    if (!node) return;
    node->stats.messages_dropped++;
}
