/**
 * @file ble_discovery_packet.c
 * @brief Pure C implementation of BLE Discovery Protocol packet operations
 * @author Benjamin Huh
 * @date 2025-11-20
 */

#include "ble_discovery_packet.h"
#include <stdlib.h>

#include <limits.h>

/* ===== Helper Functions for Serialization ===== */

/**
 * @brief Write uint8_t to buffer
 */
static inline void write_u8(uint8_t **buf, uint8_t value)
{
    **buf = value;
    (*buf)++;
}

/**
 * @brief Write uint16_t to buffer (big-endian/network byte order)
 */
static inline void write_u16(uint8_t **buf, uint16_t value)
{
    **buf = (value >> 8) & 0xFF;
    (*buf)++;
    **buf = value & 0xFF;
    (*buf)++;
}

/**
 * @brief Write uint32_t to buffer (big-endian/network byte order)
 */
static inline void write_u32(uint8_t **buf, uint32_t value)
{
    **buf = (value >> 24) & 0xFF;
    (*buf)++;
    **buf = (value >> 16) & 0xFF;
    (*buf)++;
    **buf = (value >> 8) & 0xFF;
    (*buf)++;
    **buf = value & 0xFF;
    (*buf)++;
}

/**
 * @brief Write double to buffer (IEEE 754)
 */
static inline void write_double(uint8_t **buf, double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(double));
    write_u32(buf, (bits >> 32) & 0xFFFFFFFF);
    write_u32(buf, bits & 0xFFFFFFFF);
}

/**
 * @brief Read uint8_t from buffer
 */
static inline uint8_t read_u8(const uint8_t **buf)
{
    uint8_t value = **buf;
    (*buf)++;
    return value;
}

/**
 * @brief Read uint16_t from buffer (big-endian/network byte order)
 */
static inline uint16_t read_u16(const uint8_t **buf)
{
    uint16_t value = ((uint16_t)(*buf)[0] << 8) | (*buf)[1];
    (*buf) += 2;
    return value;
}

/**
 * @brief Read uint32_t from buffer (big-endian/network byte order)
 */
static inline uint32_t read_u32(const uint8_t **buf)
{
    uint32_t value = ((uint32_t)(*buf)[0] << 24) |
                     ((uint32_t)(*buf)[1] << 16) |
                     ((uint32_t)(*buf)[2] << 8) |
                     (*buf)[3];
    (*buf) += 4;
    return value;
}

/**
 * @brief Read double from buffer (IEEE 754)
 */
static inline double read_double(const uint8_t **buf)
{
    uint64_t bits = ((uint64_t)read_u32(buf) << 32) | read_u32(buf);
    double value;
    memcpy(&value, &bits, sizeof(double));
    return value;
}

/* ===== Packet Initialization ===== */

void ble_discovery_packet_init(ble_discovery_packet_t *packet)
{
    if (!packet) return;

    packet->message_type = BLE_MSG_DISCOVERY;
    packet->is_clusterhead_message = false;
    packet->sender_id = 0;
    packet->ttl = BLE_DISCOVERY_DEFAULT_TTL;
    packet->path_length = 0;
    packet->gps_available = false;
    packet->gps_location.x = 0.0;
    packet->gps_location.y = 0.0;
    packet->gps_location.z = 0.0;
}

void ble_election_packet_init(ble_election_packet_t *packet)
{
    if (!packet) return;

    ble_discovery_packet_init(&packet->base);
    packet->base.message_type = BLE_MSG_ELECTION_ANNOUNCEMENT;
    packet->base.is_clusterhead_message = true;

    packet->election.class_id = 0;
    packet->election.direct_connections = 0;
    packet->election.pdsf = 0;
    packet->election.last_pi = 1;
    packet->election.score = 0.0;
    packet->election.hash = 0;
    ble_election_pdsf_history_reset(&packet->election.pdsf_history);
}

/* ===== TTL Operations ===== */

bool ble_discovery_decrement_ttl(ble_discovery_packet_t *packet)
{
    if (!packet) return false;

    if (packet->ttl > 0) {
        packet->ttl--;
        return true;
    }
    return false;
}

/* ===== Path Operations ===== */

bool ble_discovery_add_to_path(ble_discovery_packet_t *packet, uint32_t node_id)
{
    if (!packet) return false;
    if (packet->path_length >= BLE_DISCOVERY_MAX_PATH_LENGTH) return false;

    packet->path[packet->path_length] = node_id;
    packet->path_length++;
    return true;
}

bool ble_discovery_is_in_path(const ble_discovery_packet_t *packet, uint32_t node_id)
{
    if (!packet) return false;

    for (uint16_t i = 0; i < packet->path_length; i++) {
        if (packet->path[i] == node_id) {
            return true;
        }
    }
    return false;
}

/* ===== GPS Operations ===== */

void ble_discovery_set_gps(ble_discovery_packet_t *packet, double x, double y, double z)
{
    if (!packet) return;

    packet->gps_location.x = x;
    packet->gps_location.y = y;
    packet->gps_location.z = z;
    packet->gps_available = true;
}

/* ===== Size Calculations ===== */

uint32_t ble_discovery_get_size(const ble_discovery_packet_t *packet)
{
    if (!packet) return 0;

    // Message Type (1) + Clusterhead flag (1) + Sender ID (4) + TTL (1)
    uint32_t size = 1 + 1 + 4 + 1;

    // PSF: length (2) + node IDs (4 each)
    size += 2 + (packet->path_length * 4);

    // GPS: availability flag (1) + coordinates (24 if available)
    size += 1;
    if (packet->gps_available) {
        size += 3 * 8; // 3 doubles
    }

    return size;
}

uint32_t ble_election_get_size(const ble_election_packet_t *packet)
{
    if (!packet) return 0;

    // Base discovery size + election fields
    uint32_t size = ble_discovery_get_size(&packet->base);

    // Flags (1) + Class ID (2) + Direct Connections (4) + PDSF (4) + Last Π (4) + Score (8) + Hash (4)
    size += 1 + 2 + 4 + 4 + 4 + 8 + 4;
    // PDSF history: hop count (2) + per-hop counts (4 bytes each)
    size += 2 + (packet->election.pdsf_history.hop_count * 4);

    return size;
}

/* ===== Serialization ===== */

uint32_t ble_discovery_serialize(const ble_discovery_packet_t *packet,
                                   uint8_t *buffer,
                                   uint32_t buffer_size)
{
    if (!packet || !buffer) return 0;

    uint32_t required_size = ble_discovery_get_size(packet);
    if (buffer_size < required_size) return 0;

    uint8_t *ptr = buffer;

    // Write message type
    write_u8(&ptr, (uint8_t)packet->message_type);

    // Write clusterhead flag
    write_u8(&ptr, packet->is_clusterhead_message ? 1 : 0);

    // Write sender ID
    write_u32(&ptr, packet->sender_id);

    // Write TTL
    write_u8(&ptr, packet->ttl);

    // Write Path So Far
    write_u16(&ptr, packet->path_length);
    for (uint16_t i = 0; i < packet->path_length; i++) {
        write_u32(&ptr, packet->path[i]);
    }

    // Write GPS availability
    write_u8(&ptr, packet->gps_available ? 1 : 0);
    if (packet->gps_available) {
        write_double(&ptr, packet->gps_location.x);
        write_double(&ptr, packet->gps_location.y);
        write_double(&ptr, packet->gps_location.z);
    }

    return (uint32_t)(ptr - buffer);
}

uint32_t ble_discovery_deserialize(ble_discovery_packet_t *packet,
                                     const uint8_t *buffer,
                                     uint32_t buffer_size)
{
    if (!packet || !buffer || buffer_size < 7) return 0;

    const uint8_t *ptr = buffer;

    // Read message type
    packet->message_type = (ble_message_type_t)read_u8(&ptr);

    // Read clusterhead flag
    packet->is_clusterhead_message = (read_u8(&ptr) == 1);

    // Read sender ID
    packet->sender_id = read_u32(&ptr);

    // Read TTL
    packet->ttl = read_u8(&ptr);

    // Read Path So Far
    packet->path_length = read_u16(&ptr);
    if (packet->path_length > BLE_DISCOVERY_MAX_PATH_LENGTH) {
        return 0; // Invalid path length
    }

    for (uint16_t i = 0; i < packet->path_length; i++) {
        packet->path[i] = read_u32(&ptr);
    }

    // Read GPS availability
    packet->gps_available = (read_u8(&ptr) == 1);
    if (packet->gps_available) {
        packet->gps_location.x = read_double(&ptr);
        packet->gps_location.y = read_double(&ptr);
        packet->gps_location.z = read_double(&ptr);
    }

    return (uint32_t)(ptr - buffer);
}

uint32_t ble_election_serialize(const ble_election_packet_t *packet,
                                  uint8_t *buffer,
                                  uint32_t buffer_size)
{
    if (!packet || !buffer) return 0;

    uint32_t required_size = ble_election_get_size(packet);
    if (buffer_size < required_size) return 0;

    // Serialize base discovery packet
    uint32_t bytes_written = ble_discovery_serialize(&packet->base, buffer, buffer_size);
    if (bytes_written == 0) return 0;

    uint8_t *ptr = buffer + bytes_written;

    // Write election-specific fields
    uint8_t flags = packet->election.is_renouncement ? 0x1 : 0x0;
    write_u8(&ptr, flags);
    write_u16(&ptr, packet->election.class_id);
    write_u32(&ptr, packet->election.direct_connections);
    write_u32(&ptr, packet->election.pdsf);
    write_u32(&ptr, packet->election.last_pi);
    write_double(&ptr, packet->election.score);
    write_u32(&ptr, packet->election.hash);
    write_u16(&ptr, packet->election.pdsf_history.hop_count);
    for (uint16_t i = 0; i < packet->election.pdsf_history.hop_count; i++) {
        write_u32(&ptr, packet->election.pdsf_history.direct_counts[i]);
    }

    return (uint32_t)(ptr - buffer);
}

uint32_t ble_election_deserialize(ble_election_packet_t *packet,
                                    const uint8_t *buffer,
                                    uint32_t buffer_size)
{
    if (!packet || !buffer) return 0;

    // Deserialize base discovery packet
    uint32_t bytes_read = ble_discovery_deserialize(&packet->base, buffer, buffer_size);
    if (bytes_read == 0) return 0;

    const uint8_t *ptr = buffer + bytes_read;
    ble_election_pdsf_history_reset(&packet->election.pdsf_history);

    // Read election-specific fields
    uint8_t flags = read_u8(&ptr);
    packet->election.is_renouncement = (flags & 0x1) != 0;
    packet->election.class_id = read_u16(&ptr);
    packet->election.direct_connections = read_u32(&ptr);
    packet->election.pdsf = read_u32(&ptr);
    packet->election.last_pi = read_u32(&ptr);
    packet->election.score = read_double(&ptr);
    packet->election.hash = read_u32(&ptr);
    packet->election.pdsf_history.hop_count = read_u16(&ptr);
    if (packet->election.pdsf_history.hop_count > BLE_PDSF_MAX_HOPS) {
        return 0;
    }
    for (uint16_t i = 0; i < packet->election.pdsf_history.hop_count; i++) {
        packet->election.pdsf_history.direct_counts[i] = read_u32(&ptr);
    }

    return (uint32_t)(ptr - buffer);
}

/* ===== Election Calculations ===== */

void ble_election_pdsf_history_reset(ble_pdsf_history_t *history)
{
    if (!history) return;
    history->hop_count = 0;
    memset(history->direct_counts, 0, sizeof(history->direct_counts));
}

bool ble_election_pdsf_history_add(ble_pdsf_history_t *history, uint32_t direct_connections)
{
    if (!history) return false;
    if (history->hop_count >= BLE_PDSF_MAX_HOPS) {
        return false;
    }
    history->direct_counts[history->hop_count++] = direct_connections;
    return true;
}

uint32_t ble_election_update_pdsf(ble_election_packet_t *packet,
                                    uint32_t direct_connections,
                                    uint32_t already_reached)
{
    if (!packet) return 0;

    if (already_reached > direct_connections) {
        already_reached = direct_connections;
    }
    uint32_t unique_connections = direct_connections - already_reached;

    if (!ble_election_pdsf_history_add(&packet->election.pdsf_history, unique_connections)) {
        return packet->election.pdsf;
    }

    uint32_t new_pi = 0;
    packet->election.pdsf = ble_election_calculate_pdsf(
        packet->election.pdsf,
        packet->election.last_pi,
        unique_connections,
        &new_pi);
    packet->election.last_pi = new_pi;

    return packet->election.pdsf;
}

uint32_t ble_election_calculate_pdsf(uint32_t previous_pdsf,
                                     uint32_t previous_pi,
                                     uint32_t direct_neighbors,
                                     uint32_t *new_pi_out)
{
    /* Treat zeroed history as baseline 1 so the first hop contributes its direct count */
    uint64_t baseline_pi = (previous_pi == 0) ? 1ULL : (uint64_t)previous_pi;
    uint64_t hop_neighbors = (uint64_t)direct_neighbors;
    uint64_t pi_term = baseline_pi * hop_neighbors;

    if (pi_term > UINT32_MAX) {
        pi_term = UINT32_MAX;
    }

    uint64_t updated_sum = (uint64_t)previous_pdsf + pi_term;
    if (updated_sum > UINT32_MAX) {
        updated_sum = UINT32_MAX;
    }

    if (new_pi_out) {
        *new_pi_out = (uint32_t)pi_term;
    }

    return (uint32_t)updated_sum;
}

static double clamp_unit(double value)
{
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

#define BLE_SCORE_DIRECT_NORMALIZER 30.0
#define BLE_SCORE_CN_RATIO_NORMALIZER 10.0

double ble_election_calculate_score(uint32_t direct_connections,
                                      double noise_level)
{
    double base_score = (double)direct_connections;

    double neighbor_ratio = 0.0;
    if (BLE_DISCOVERY_MAX_CLUSTER_SIZE > 0) {
        neighbor_ratio = (double)direct_connections / (double)BLE_DISCOVERY_MAX_CLUSTER_SIZE;
    }

    double noise_modifier = 1.0 / (noise_level + 1.0);

    double ratio_bonus = neighbor_ratio * noise_modifier;

    return base_score + ratio_bonus;
}

uint32_t ble_election_generate_hash(uint32_t node_id)
{
    //JASON TODO: Update hash function for when you talk back to edges to factor in 
    //node distribution

    // Simple hash function for FDMA/TDMA slot assignment
    // FNV-1a hash variant
    uint32_t hash = 2166136261u;
    hash ^= (node_id & 0xFF);
    hash *= 16777619;
    hash ^= ((node_id >> 8) & 0xFF);
    hash *= 16777619;
    hash ^= ((node_id >> 16) & 0xFF);
    hash *= 16777619;
    hash ^= ((node_id >> 24) & 0xFF);
    hash *= 16777619;

    return hash;
}
