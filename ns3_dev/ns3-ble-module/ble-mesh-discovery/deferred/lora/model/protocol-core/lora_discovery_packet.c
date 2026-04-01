/**
 * @file lora_discovery_packet.c
 * @brief Pure C scaffolding for LoRA discovery packets (placeholder)
 */

#include "lora_discovery_packet.h"
#include <string.h>

void
lora_discovery_packet_init(lora_discovery_packet_t *packet)
{
    if (!packet) {
        return;
    }
    memset(packet, 0, sizeof(*packet));
    packet->message_type = LORA_MSG_DISCOVERY;
    packet->ttl = 6;
    packet->channel = 1;
    packet->home_channel = 1;
    packet->target_channel = 1;
    packet->crowding = 0;
    packet->is_clusterhead = false;
}

bool
lora_discovery_add_to_path(lora_discovery_packet_t *packet, uint32_t node_id)
{
    if (!packet) {
        return false;
    }
    if (packet->path_length >= (sizeof(packet->path) / sizeof(packet->path[0]))) {
        return false;
    }
    packet->path[packet->path_length++] = node_id;
    return true;
}

void
lora_discovery_set_gps(lora_discovery_packet_t *packet, double x, double y, double z)
{
    if (!packet) {
        return;
    }
    packet->gps.x = x;
    packet->gps.y = y;
    packet->gps.z = z;
    packet->gps_available = true;
}

uint32_t
lora_discovery_serialize(const lora_discovery_packet_t *packet,
                         uint8_t *buffer,
                         uint32_t buffer_size)
{
    if (!packet || !buffer) {
        return 0;
    }
    uint32_t needed = 1 + 4 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + (packet->path_length * 4);
    if (packet->gps_available) {
        needed += 3 * sizeof(double);
    }
    if (buffer_size < needed) {
        return 0;
    }
    uint8_t *ptr = buffer;
    *ptr++ = (uint8_t)packet->message_type;
    memcpy(ptr, &packet->sender_id, sizeof(uint32_t)); ptr += 4;
    *ptr++ = packet->ttl;
    *ptr++ = packet->channel;
    *ptr++ = packet->home_channel;
    *ptr++ = packet->target_channel;
    *ptr++ = packet->crowding;
    *ptr++ = packet->is_clusterhead ? 1 : 0;
    *ptr++ = packet->path_length;
    for (uint8_t i = 0; i < packet->path_length; i++) {
        memcpy(ptr, &packet->path[i], sizeof(uint32_t)); ptr += 4;
    }
    *ptr++ = packet->gps_available ? 1 : 0;
    if (packet->gps_available) {
        memcpy(ptr, &packet->gps.x, sizeof(double)); ptr += sizeof(double);
        memcpy(ptr, &packet->gps.y, sizeof(double)); ptr += sizeof(double);
        memcpy(ptr, &packet->gps.z, sizeof(double)); ptr += sizeof(double);
    }
    return (uint32_t)(ptr - buffer);
}

uint32_t
lora_discovery_deserialize(lora_discovery_packet_t *packet,
                           const uint8_t *buffer,
                           uint32_t buffer_size)
{
    if (!packet || !buffer || buffer_size < 9) {
        return 0;
    }
    const uint8_t *ptr = buffer;
    packet->message_type = (lora_message_type_t)(*ptr++);
    memcpy(&packet->sender_id, ptr, sizeof(uint32_t)); ptr += 4;
    packet->ttl = *ptr++;
    packet->channel = *ptr++;
    packet->home_channel = *ptr++;
    packet->target_channel = *ptr++;
    packet->crowding = *ptr++;
    packet->is_clusterhead = (*ptr++ != 0);
    packet->path_length = *ptr++;
    if (packet->path_length > (sizeof(packet->path) / sizeof(packet->path[0]))) {
        return 0;
    }
    for (uint8_t i = 0; i < packet->path_length; i++) {
        memcpy(&packet->path[i], ptr, sizeof(uint32_t)); ptr += 4;
    }
    packet->gps_available = (*ptr++ != 0);
    if (packet->gps_available) {
        if ((uint32_t)(ptr - buffer + 3 * sizeof(double)) > buffer_size) {
            return 0;
        }
        memcpy(&packet->gps.x, ptr, sizeof(double)); ptr += sizeof(double);
        memcpy(&packet->gps.y, ptr, sizeof(double)); ptr += sizeof(double);
        memcpy(&packet->gps.z, ptr, sizeof(double)); ptr += sizeof(double);
    }
    return (uint32_t)(ptr - buffer);
}
