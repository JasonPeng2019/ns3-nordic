/**
 * @file ble_discovery_packet.c
 * @brief Pure C implementation of BLE Discovery Protocol packet operations
 * @author jason peng
 * @date 2025-11-20
 */

#include "ble_discovery_packet.h"
#include <stdlib.h>

const ble_score_weights_t BLE_DEFAULT_SCORE_WEIGHTS = {
    0.35,
    0.30,
    0.20,
    0.15
};
#include <limits.h>



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
    packet->election.score = 0.0;
    packet->election.hash = 0;
    ble_election_pdsf_history_reset(&packet->election.pdsf_history);
}



bool ble_discovery_decrement_ttl(ble_discovery_packet_t *packet)
{
    if (!packet) return false;

    if (packet->ttl > 0) {
        packet->ttl--;
        return true;
    }
    return false;
}



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



void ble_discovery_set_gps(ble_discovery_packet_t *packet, double x, double y, double z)
{
    if (!packet) return;

    packet->gps_location.x = x;
    packet->gps_location.y = y;
    packet->gps_location.z = z;
    packet->gps_available = true;
}



uint32_t ble_discovery_get_size(const ble_discovery_packet_t *packet)
{
    if (!packet) return 0;

    
    uint32_t size = 1 + 1 + 4 + 1;

    
    size += 2 + (packet->path_length * 4);

    
    size += 1;
    if (packet->gps_available) {
        size += 3 * 8; 
    }

    return size;
}

uint32_t ble_election_get_size(const ble_election_packet_t *packet)
{
    if (!packet) return 0;

    
    uint32_t size = ble_discovery_get_size(&packet->base);

    
    size += 2 + 4 + 4 + 8 + 4;
    
    size += 2 + (packet->election.pdsf_history.hop_count * 4);

    return size;
}



uint32_t ble_discovery_serialize(const ble_discovery_packet_t *packet,
                                   uint8_t *buffer,
                                   uint32_t buffer_size)
{
    if (!packet || !buffer) return 0;

    uint32_t required_size = ble_discovery_get_size(packet);
    if (buffer_size < required_size) return 0;

    uint8_t *ptr = buffer;

    
    write_u8(&ptr, (uint8_t)packet->message_type);

    
    write_u8(&ptr, packet->is_clusterhead_message ? 1 : 0);

    
    write_u32(&ptr, packet->sender_id);

    
    write_u8(&ptr, packet->ttl);

    
    write_u16(&ptr, packet->path_length);
    for (uint16_t i = 0; i < packet->path_length; i++) {
        write_u32(&ptr, packet->path[i]);
    }

    
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

    
    packet->message_type = (ble_message_type_t)read_u8(&ptr);

    
    packet->is_clusterhead_message = (read_u8(&ptr) == 1);

    
    packet->sender_id = read_u32(&ptr);

    
    packet->ttl = read_u8(&ptr);

    
    packet->path_length = read_u16(&ptr);
    if (packet->path_length > BLE_DISCOVERY_MAX_PATH_LENGTH) {
        return 0; 
    }

    for (uint16_t i = 0; i < packet->path_length; i++) {
        packet->path[i] = read_u32(&ptr);
    }

    
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

    
    uint32_t bytes_written = ble_discovery_serialize(&packet->base, buffer, buffer_size);
    if (bytes_written == 0) return 0;

    uint8_t *ptr = buffer + bytes_written;

    
    write_u16(&ptr, packet->election.class_id);
    write_u32(&ptr, packet->election.direct_connections);
    write_u32(&ptr, packet->election.pdsf);
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

    
    uint32_t bytes_read = ble_discovery_deserialize(&packet->base, buffer, buffer_size);
    if (bytes_read == 0) return 0;

    const uint8_t *ptr = buffer + bytes_read;
    ble_election_pdsf_history_reset(&packet->election.pdsf_history);

    
    packet->election.class_id = read_u16(&ptr);
    packet->election.direct_connections = read_u32(&ptr);
    packet->election.pdsf = read_u32(&ptr);
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

    packet->election.pdsf = ble_election_calculate_pdsf(
        packet->election.pdsf,
        unique_connections);

    return packet->election.pdsf;
}

uint32_t ble_election_calculate_pdsf(uint32_t previous_pdsf, uint32_t direct_neighbors)
{
    
    uint32_t baseline = (previous_pdsf == 0) ? 1 : previous_pdsf;

    
    uint64_t increment = (uint64_t)baseline * (uint64_t)direct_neighbors;
    uint64_t updated = (uint64_t)baseline + increment;

    if (updated > UINT32_MAX) {
        return UINT32_MAX;
    }

    return (uint32_t)updated;
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
