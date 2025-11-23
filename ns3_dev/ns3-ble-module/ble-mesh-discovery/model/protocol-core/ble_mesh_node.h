/**
 * @file ble_mesh_node.h
 * @brief Pure C implementation of BLE Mesh Node state machine
 * @author Benjamin Huh
 * @date 2025-11-21
 *
 * This is the core node state machine implementation in C for portability.
 * Can be compiled without NS-3 or any C++ dependencies.
 *
 * Based on: "Clusterhead & BLE Mesh discovery process" by jason.peng (November 2025)
 */

#ifndef BLE_MESH_NODE_H
#define BLE_MESH_NODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "ble_discovery_packet.h"

/* ===== Constants ===== */

#define BLE_MESH_MAX_NEIGHBORS 150      /**< Maximum neighbors per node */
#define BLE_MESH_INVALID_NODE_ID 0      /**< Invalid/unassigned node ID */
#define BLE_MESH_DISCOVERY_TIMEOUT 90   /**< Discovery phase timeout in cycles */
/* ===== Node State Enumeration ===== */

/**
 * @brief Node state in the BLE mesh network
 *
 * State transitions:
 * INIT → DISCOVERY → EDGE/CLUSTERHEAD_CANDIDATE → CLUSTERHEAD/MEMBER
 */
typedef enum {
    BLE_NODE_STATE_INIT = 0,              /**< Initial state, not yet started */
    BLE_NODE_STATE_DISCOVERY = 1,         /**< Active discovery phase */
    BLE_NODE_STATE_EDGE = 2,              /**< Edge node (low connectivity) */
    BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE = 3, /**< Candidate for clusterhead */
    BLE_NODE_STATE_CLUSTERHEAD = 4,       /**< Elected clusterhead */
    BLE_NODE_STATE_CLUSTER_MEMBER = 5     /**< Member of a cluster */
} ble_node_state_t;

/* ===== Neighbor Information Structure ===== */

/**
 * @brief Information about a discovered neighbor
 */
typedef struct {
    uint32_t node_id;           /**< Neighbor's node ID */
    int8_t rssi;                /**< RSSI value (dBm) */
    uint8_t hop_count;          /**< Hop count to this neighbor */
    uint32_t last_seen_cycle;   /**< Last discovery cycle when heard from */
    bool is_clusterhead;        /**< Whether neighbor is a clusterhead */
    uint16_t clusterhead_class; /**< Clusterhead class if applicable */
    ble_gps_location_t gps;     /**< Neighbor's GPS location */
    bool gps_valid;             /**< Whether GPS location is valid */
} ble_neighbor_info_t;

/* ===== Neighbor Table Structure ===== */

/**
 * @brief Neighbor tracking table
 */
typedef struct {
    ble_neighbor_info_t neighbors[BLE_MESH_MAX_NEIGHBORS];
    uint16_t count;             /**< Current number of neighbors */
} ble_neighbor_table_t;

/* ===== Node Statistics Structure ===== */

/**
 * @brief Node statistics for election and monitoring
 */
typedef struct {
    uint32_t messages_sent;         /**< Total messages transmitted */
    uint32_t messages_received;     /**< Total messages received */
    uint32_t messages_forwarded;    /**< Total messages forwarded */
    uint32_t messages_dropped;      /**< Total messages dropped */
    uint32_t discovery_cycles;      /**< Number of discovery cycles completed */
    int8_t avg_rssi;                /**< Average RSSI of neighbors */
    uint16_t direct_connections;    /**< Number of direct (1-hop) connections */
} ble_node_statistics_t;

/* ===== Main Node Structure ===== */

/**
 * @brief BLE Mesh Node state and data
 */
typedef struct {
    /* Identity */
    uint32_t node_id;               /**< Unique node identifier */

    /* State */
    ble_node_state_t state;         /**< Current node state */
    ble_node_state_t prev_state;    /**< Previous state (for transitions) */
    uint32_t state_entry_cycle;     /**< Cycle number when entered current state */

    /* Location */
    ble_gps_location_t gps_location; /**< Node's GPS coordinates */
    bool gps_available;             /**< Whether GPS is available */
    uint32_t gps_last_update_cycle; /**< Cycle when GPS was last updated */
    uint32_t gps_cache_ttl;         /**< GPS cache time-to-live in cycles (0=disabled) */

    /* Discovery & Clustering */
    ble_neighbor_table_t neighbors; /**< Known neighbors */
    uint32_t clusterhead_id;        /**< ID of clusterhead (if member) */
    uint16_t cluster_class;         /**< Cluster class ID (if clusterhead) */

    /* Election metrics */
    uint32_t pdsf;                  /**< Predicted Devices So Far */
    double candidacy_score;         /**< Clusterhead candidacy score (0.0-1.0) */
    uint32_t election_hash;         /**< FDMA/TDMA hash value */
    double noise_level;             /**< Last measured noise/crowding level */
    uint32_t last_candidate_heard_cycle; /**< Discovery cycle when another candidate was last heard */

    /* Timing */
    uint32_t current_cycle;         /**< Current discovery cycle number */

    /* Statistics */
    ble_node_statistics_t stats;    /**< Node statistics */

} ble_mesh_node_t;

/* ===== Function Prototypes ===== */

/**
 * @brief Initialize a mesh node with default values
 * @param node Pointer to node structure
 * @param node_id Unique node identifier
 */
void ble_mesh_node_init(ble_mesh_node_t *node, uint32_t node_id);

/**
 * @brief Set node GPS location
 * @param node Pointer to node structure
 * @param x GPS X coordinate (latitude)
 * @param y GPS Y coordinate (longitude)
 * @param z GPS Z coordinate (altitude)
 */
void ble_mesh_node_set_gps(ble_mesh_node_t *node, double x, double y, double z);

/**
 * @brief Mark GPS as unavailable
 * @param node Pointer to node structure
 */
void ble_mesh_node_clear_gps(ble_mesh_node_t *node);

/**
 * @brief Set GPS cache time-to-live
 * @param node Pointer to node structure
 * @param ttl_cycles Cache TTL in discovery cycles (0=no expiration)
 */
void ble_mesh_node_set_gps_cache_ttl(ble_mesh_node_t *node, uint32_t ttl_cycles);

/**
 * @brief Check if GPS cache is still valid
 * @param node Pointer to node structure
 * @return true if GPS is available and cache hasn't expired
 */
bool ble_mesh_node_is_gps_cache_valid(const ble_mesh_node_t *node);

/**
 * @brief Invalidate GPS cache (marks as stale)
 * @param node Pointer to node structure
 */
void ble_mesh_node_invalidate_gps_cache(ble_mesh_node_t *node);

/**
 * @brief Get cycles since last GPS update
 * @param node Pointer to node structure
 * @return Number of cycles since GPS was last updated
 */
uint32_t ble_mesh_node_get_gps_age(const ble_mesh_node_t *node);

/**
 * @brief Get current node state
 * @param node Pointer to node structure
 * @return Current state
 */
ble_node_state_t ble_mesh_node_get_state(const ble_mesh_node_t *node);

/**
 * @brief Transition to a new state
 * @param node Pointer to node structure
 * @param new_state New state to transition to
 * @return true if transition was valid, false otherwise
 */
bool ble_mesh_node_set_state(ble_mesh_node_t *node, ble_node_state_t new_state);

/**
 * @brief Check if a state transition is valid
 * @param current Current state
 * @param new_state Proposed new state
 * @return true if transition is allowed, false otherwise
 */
bool ble_mesh_node_is_valid_transition(ble_node_state_t current, ble_node_state_t new_state);

/**
 * @brief Get state name as string (for debugging)
 * @param state The state
 * @return String representation of state
 */
const char* ble_mesh_node_state_name(ble_node_state_t state);

/**
 * @brief Advance to next discovery cycle
 * @param node Pointer to node structure
 */
void ble_mesh_node_advance_cycle(ble_mesh_node_t *node);

/**
 * @brief Add or update a neighbor in the table
 * @param node Pointer to node structure
 * @param neighbor_id Neighbor's node ID
 * @param rssi RSSI value
 * @param hop_count Hop count to neighbor
 * @return true if added/updated successfully, false if table full
 */
bool ble_mesh_node_add_neighbor(ble_mesh_node_t *node,
                                  uint32_t neighbor_id,
                                  int8_t rssi,
                                  uint8_t hop_count);

/**
 * @brief Update neighbor's GPS location
 * @param node Pointer to node structure
 * @param neighbor_id Neighbor's node ID
 * @param gps GPS location
 * @return true if neighbor found and updated, false otherwise
 */
bool ble_mesh_node_update_neighbor_gps(ble_mesh_node_t *node,
                                         uint32_t neighbor_id,
                                         const ble_gps_location_t *gps);

/**
 * @brief Find a neighbor by ID
 * @param node Pointer to node structure
 * @param neighbor_id Neighbor's node ID
 * @return Pointer to neighbor info, or NULL if not found
 */
ble_neighbor_info_t* ble_mesh_node_find_neighbor(ble_mesh_node_t *node,
                                                   uint32_t neighbor_id);

/**
 * @brief Count direct (1-hop) neighbors
 * @param node Pointer to node structure
 * @return Number of 1-hop neighbors
 */
uint16_t ble_mesh_node_count_direct_neighbors(const ble_mesh_node_t *node);

/**
 * @brief Calculate average RSSI of all neighbors
 * @param node Pointer to node structure
 * @return Average RSSI in dBm, or 0 if no neighbors
 */
int8_t ble_mesh_node_calculate_avg_rssi(const ble_mesh_node_t *node);

/**
 * @brief Remove stale neighbors (not heard from in N cycles)
 * @param node Pointer to node structure
 * @param max_age Maximum age in cycles before removal
 * @return Number of neighbors removed
 */
uint16_t ble_mesh_node_prune_stale_neighbors(ble_mesh_node_t *node, uint32_t max_age);

/**
 * @brief Calculate candidacy score for clusterhead election
 * @param node Pointer to node structure
 * @param noise_level Measured noise level
 * @return Candidacy score (higher = better)
 */
double ble_mesh_node_calculate_candidacy_score(const ble_mesh_node_t *node,
                                                 double noise_level);

/**
 * @brief Update node statistics
 * @param node Pointer to node structure
 */
void ble_mesh_node_update_statistics(ble_mesh_node_t *node);

/**
 * @brief Check if node should transition to EDGE state
 * @param node Pointer to node structure
 * @return true if node should become edge node
 */
bool ble_mesh_node_should_become_edge(const ble_mesh_node_t *node);

/**
 * @brief Check if node should become clusterhead candidate
 * @param node Pointer to node structure
 * @return true if node should become candidate
 */
bool ble_mesh_node_should_become_candidate(const ble_mesh_node_t *node);

/**
 * @brief Set the last measured noise/crowding level
 * @param node Pointer to node structure
 * @param noise_level Noise level value (>= 0)
 */
void ble_mesh_node_set_noise_level(ble_mesh_node_t *node, double noise_level);

/**
 * @brief Mark that another clusterhead candidate was heard
 * @param node Pointer to node structure
 */
void ble_mesh_node_mark_candidate_heard(ble_mesh_node_t *node);

/**
 * @brief Increment message sent counter
 * @param node Pointer to node structure
 */
void ble_mesh_node_inc_sent(ble_mesh_node_t *node);

/**
 * @brief Increment message received counter
 * @param node Pointer to node structure
 */
void ble_mesh_node_inc_received(ble_mesh_node_t *node);

/**
 * @brief Increment message forwarded counter
 * @param node Pointer to node structure
 */
void ble_mesh_node_inc_forwarded(ble_mesh_node_t *node);

/**
 * @brief Increment message dropped counter
 * @param node Pointer to node structure
 */
void ble_mesh_node_inc_dropped(ble_mesh_node_t *node);

#ifdef __cplusplus
}
#endif

#endif /* BLE_MESH_NODE_H */
