/**
 * @file lora_engine.h
 * @brief Minimal LoRA engine scaffolding (discovery/channel placeholder)
 *
 * This mirrors the BLE engine pattern: pure C core with callbacks so it can be
 * reused in NS-3 via a thin C++ wrapper.
 */

#ifndef LORA_ENGINE_H
#define LORA_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "lora_mesh_node.h"
#include "lora_discovery_packet.h"

typedef void (*lora_engine_send_callback)(const lora_discovery_packet_t *packet,
                                          void *user_context);

typedef void (*lora_engine_log_callback)(const char *level,
                                         const char *message,
                                         void *user_context);

typedef struct {
    uint32_t node_id;
    uint8_t initial_channel;
    uint8_t max_channels;
    uint32_t slot_duration_ms;
    lora_engine_send_callback send_cb;
    lora_engine_log_callback log_cb;
    void *user_context;
} lora_engine_config_t;

typedef struct {
    bool initialized;
    lora_engine_config_t config;
    lora_mesh_node_t node;
    lora_discovery_packet_t tx_buffer;
} lora_engine_t;

void lora_engine_config_init(lora_engine_config_t *config);
bool lora_engine_init(lora_engine_t *engine, const lora_engine_config_t *config);
void lora_engine_tick(lora_engine_t *engine, uint32_t now_ms);
bool lora_engine_receive_packet(lora_engine_t *engine,
                                const lora_discovery_packet_t *packet,
                                int8_t rssi,
                                uint32_t now_ms);
void lora_engine_set_crowding(lora_engine_t *engine, double crowding);

#ifdef __cplusplus
}
#endif

#endif /* LORA_ENGINE_H */
