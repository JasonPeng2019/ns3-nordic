/**
 * @file lora_discovery_packet.h
 * @brief Pure C scaffolding for LoRA discovery packets (placeholder)
 *
 * This mirrors the BLE packet layout at a high level but will be specialized
 * for LoRA discovery/channel-voting in a future pass.
 */

#ifndef LORA_DISCOVERY_PACKET_H
#define LORA_DISCOVERY_PACKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LORA_MSG_DISCOVERY = 0,
    LORA_MSG_CLUSTERHEAD_ANNOUNCE = 1,
    LORA_MSG_CHANNEL_VOTE = 2
} lora_message_type_t;

typedef struct {
    double x;
    double y;
    double z;
} lora_gps_location_t;

typedef struct {
    lora_message_type_t message_type;
    uint32_t sender_id;
    uint8_t ttl;
    uint8_t channel;       /**< current channel k */
    uint8_t home_channel;  /**< home channel */
    uint8_t target_channel; /**< proposed/voted channel */
    uint8_t crowding;      /**< crowding factor scaled 0-255 */
    bool is_clusterhead;   /**< sender is LoRA clusterhead */
    bool gps_available;
    lora_gps_location_t gps;
    uint8_t path_length;
    uint32_t path[50];
} lora_discovery_packet_t;

void lora_discovery_packet_init(lora_discovery_packet_t *packet);
bool lora_discovery_add_to_path(lora_discovery_packet_t *packet, uint32_t node_id);
void lora_discovery_set_gps(lora_discovery_packet_t *packet, double x, double y, double z);
uint32_t lora_discovery_serialize(const lora_discovery_packet_t *packet,
                                  uint8_t *buffer,
                                  uint32_t buffer_size);
uint32_t lora_discovery_deserialize(lora_discovery_packet_t *packet,
                                    const uint8_t *buffer,
                                    uint32_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* LORA_DISCOVERY_PACKET_H */
