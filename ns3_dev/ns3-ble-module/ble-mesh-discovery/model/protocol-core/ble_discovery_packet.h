/**
 * @file ble_discovery_packet.h
 * @brief Pure C implementation of BLE Discovery Protocol packet format
 * @author jason peng
 * @date 2025-11-20
 *
 * This is the core protocol implementation in C for portability to embedded systems.
 * Can be compiled without NS-3 or any C++ dependencies.
 *
 * Based on: "Clusterhead & BLE Mesh discovery process" by jason.peng (November 2025)
 */

#ifndef BLE_DISCOVERY_PACKET_H
#define BLE_DISCOVERY_PACKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>




#define BLE_DISCOVERY_MAX_PATH_LENGTH 50    /**< Maximum nodes in path */
#define BLE_DISCOVERY_DEFAULT_TTL 10        /**< Default Time To Live */
#define BLE_DISCOVERY_MAX_CLUSTER_SIZE 150  /**< Maximum devices per cluster */
#define BLE_PDSF_MAX_HOPS BLE_DISCOVERY_MAX_PATH_LENGTH /**< Maximum hops tracked for PDSF */



/**
 * @brief Weights for clusterhead score calculation
 */
typedef struct {
    double direct_weight;            /**< Weight for direct connections */
    double connection_noise_weight;  /**< Weight for connection:noise ratio */
    double geographic_weight;        /**< Weight for geographic distribution */
    double forwarding_weight;        /**< Weight for forwarding success */
} ble_score_weights_t;

extern const ble_score_weights_t BLE_DEFAULT_SCORE_WEIGHTS;



/**
 * @brief Message type enumeration
 */
typedef enum {
    BLE_MSG_DISCOVERY = 0,           /**< Basic discovery message */
    BLE_MSG_ELECTION_ANNOUNCEMENT = 1 /**< Clusterhead election announcement */
} ble_message_type_t;



/**
 * @brief GPS coordinates structure
 */
typedef struct {
    double x;  /**< X coordinate (latitude) */
    double y;  /**< Y coordinate (longitude) */
    double z;  /**< Z coordinate (altitude) */
} ble_gps_location_t;



/**
 * @brief BLE Discovery packet (common fields)
 */
typedef struct {
    ble_message_type_t message_type;  /**< Message type */
    bool is_clusterhead_message;      /**< Clusterhead flag (true for election announcements) */
    uint32_t sender_id;               /**< Unique identifier of sender */
    uint8_t ttl;                      /**< Time To Live (hops remaining) */

    
    uint16_t path_length;             /**< Number of nodes in path */
    uint32_t path[BLE_DISCOVERY_MAX_PATH_LENGTH]; /**< Array of node IDs */

    
    bool gps_available;               /**< GPS availability flag */
    ble_gps_location_t gps_location;  /**< GPS coordinates (if available) */
} ble_discovery_packet_t;



/**
 * @brief Tracks direct connection counts per hop for PDSF calculation
 */
typedef struct {
    uint16_t hop_count;                              /**< Number of hops recorded */
    uint32_t direct_counts[BLE_PDSF_MAX_HOPS];       /**< Direct connection counts per hop */
} ble_pdsf_history_t;

/**
 * @brief Election announcement specific fields
 *
 * Fields required for Phase 4 election:
 * - class_id: Identifies the clusterhead's class/group
 * - direct_connections: Number of 1-hop neighbors (used for conflict resolution)
 * - pdsf: Predicted Devices So Far (for cluster capacity limiting)
 * - score: Candidacy score (weighted metric combination)
 * - hash: FDMA/TDMA slot assignment hash
 */
typedef struct {
    uint16_t class_id;           /**< Clusterhead class identifier */
    uint32_t direct_connections; /**< Number of direct (1-hop) neighbors - for conflict resolution */
    uint32_t pdsf;               /**< Predicted Devices So Far */
    double score;                /**< Clusterhead candidacy score (0.0-1.0) */
    uint32_t hash;               /**< FDMA/TDMA hash function value */
    ble_pdsf_history_t pdsf_history; /**< Hop-by-hop direct connection history */
} ble_election_data_t;

/**
 * @brief Complete election announcement packet
 */
typedef struct {
    ble_discovery_packet_t base;  /**< Base discovery fields */
    ble_election_data_t election; /**< Election-specific fields */
} ble_election_packet_t;



/**
 * @brief Initialize a discovery packet with default values
 * @param packet Pointer to packet structure
 */
void ble_discovery_packet_init(ble_discovery_packet_t *packet);

/**
 * @brief Initialize an election packet with default values
 * @param packet Pointer to election packet structure
 */
void ble_election_packet_init(ble_election_packet_t *packet);

/**
 * @brief Decrement TTL by 1
 * @param packet Pointer to packet structure
 * @return true if TTL > 0 after decrement, false otherwise
 */
bool ble_discovery_decrement_ttl(ble_discovery_packet_t *packet);

/**
 * @brief Add a node ID to the path
 * @param packet Pointer to packet structure
 * @param node_id Node ID to add
 * @return true if added successfully, false if path full
 */
bool ble_discovery_add_to_path(ble_discovery_packet_t *packet, uint32_t node_id);

/**
 * @brief Check if a node is in the path (loop detection)
 * @param packet Pointer to packet structure
 * @param node_id Node ID to check
 * @return true if node is in path, false otherwise
 */
bool ble_discovery_is_in_path(const ble_discovery_packet_t *packet, uint32_t node_id);

/**
 * @brief Set GPS location
 * @param packet Pointer to packet structure
 * @param x X coordinate (latitude)
 * @param y Y coordinate (longitude)
 * @param z Z coordinate (altitude)
 */
void ble_discovery_set_gps(ble_discovery_packet_t *packet, double x, double y, double z);

/**
 * @brief Calculate serialized size of discovery packet
 * @param packet Pointer to packet structure
 * @return Size in bytes
 */
uint32_t ble_discovery_get_size(const ble_discovery_packet_t *packet);

/**
 * @brief Calculate serialized size of election packet
 * @param packet Pointer to election packet structure
 * @return Size in bytes
 */
uint32_t ble_election_get_size(const ble_election_packet_t *packet);

/**
 * @brief Serialize discovery packet to buffer
 * @param packet Pointer to packet structure
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of bytes written, or 0 on error
 */
uint32_t ble_discovery_serialize(const ble_discovery_packet_t *packet,
                                   uint8_t *buffer,
                                   uint32_t buffer_size);

/**
 * @brief Deserialize discovery packet from buffer
 * @param packet Pointer to packet structure to fill
 * @param buffer Input buffer
 * @param buffer_size Size of input buffer
 * @return Number of bytes read, or 0 on error
 */
uint32_t ble_discovery_deserialize(ble_discovery_packet_t *packet,
                                     const uint8_t *buffer,
                                     uint32_t buffer_size);

/**
 * @brief Serialize election packet to buffer
 * @param packet Pointer to election packet structure
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of bytes written, or 0 on error
 */
uint32_t ble_election_serialize(const ble_election_packet_t *packet,
                                  uint8_t *buffer,
                                  uint32_t buffer_size);

/**
 * @brief Deserialize election packet from buffer
 * @param packet Pointer to election packet structure to fill
 * @param buffer Input buffer
 * @param buffer_size Size of input buffer
 * @return Number of bytes read, or 0 on error
 */
uint32_t ble_election_deserialize(ble_election_packet_t *packet,
                                    const uint8_t *buffer,
                                    uint32_t buffer_size);

/**
 * @brief Update PDSF (Predicted Devices So Far) based on local neighbors
 * @param previous_pdsf PDSF value carried with the incoming packet (use 1 for first hop)
 * @param direct_neighbors Number of direct neighbors observed during the crowding measurement
 * @return Updated PDSF including this hop's predicted reach
 */
uint32_t ble_election_calculate_pdsf(uint32_t previous_pdsf, uint32_t direct_neighbors);

/**
 * @brief Calculate clusterhead candidacy score
 * @param direct_connections Number of direct connections
 * @param noise_level Measured noise level (RSSI-based)
 * @return Candidacy score (higher = better)
 */
double ble_election_calculate_score(uint32_t direct_connections,
                                      double noise_level);

/**
 * @brief Reset PDSF history accumulator
 * @param history History structure to reset
 */
void ble_election_pdsf_history_reset(ble_pdsf_history_t *history);

/**
 * @brief Append a direct-connection count to the PDSF history
 * @param history History structure
 * @param direct_connections Count recorded at this hop
 * @return true if appended successfully
 */
bool ble_election_pdsf_history_add(ble_pdsf_history_t *history,
                                   uint32_t direct_connections);

/**
 * @brief Recalculate running PDSF after observing new neighbors
 * @param packet Election packet being updated (history consumed)
 * @param direct_connections New neighbor count at this hop
 * @param already_reached Nodes already covered by previous hops
 * @return Updated PDSF value
 */
uint32_t ble_election_update_pdsf(ble_election_packet_t *packet,
                                  uint32_t direct_connections,
                                  uint32_t already_reached);

/**
 * @brief Generate FDMA/TDMA hash from node ID
 * @param node_id Node identifier
 * @return Hash value for time/frequency slot assignment
 */
uint32_t ble_election_generate_hash(uint32_t node_id);

#ifdef __cplusplus
}
#endif

#endif 
