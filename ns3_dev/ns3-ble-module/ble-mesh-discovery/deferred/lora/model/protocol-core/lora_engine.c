/**
 * @file lora_engine.c
 * @brief Minimal LoRA engine scaffolding (discovery/channel placeholder)
 */

#include "lora_engine.h"
#include <string.h>

static void
lora_engine_log(const lora_engine_t *engine, const char *level, const char *message)
{
    if (engine && engine->config.log_cb) {
        engine->config.log_cb(level, message, engine->config.user_context);
    }
}

void
lora_engine_config_init(lora_engine_config_t *config)
{
    if (!config) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->initial_channel = 1;
    config->max_channels = 1;
    config->slot_duration_ms = 100;
}

bool
lora_engine_init(lora_engine_t *engine, const lora_engine_config_t *config)
{
    if (!engine || !config || config->node_id == 0 || config->send_cb == NULL) {
        return false;
    }
    memset(engine, 0, sizeof(*engine));
    engine->config = *config;
    lora_mesh_node_init(&engine->node, config->node_id);
    lora_mesh_node_set_channel(&engine->node, config->initial_channel);
    lora_discovery_packet_init(&engine->tx_buffer);
    engine->initialized = true;
    return true;
}

void
lora_engine_tick(lora_engine_t *engine, uint32_t now_ms)
{
    (void)now_ms;
    if (!engine || !engine->initialized) {
        return;
    }
    /* Emit discovery or vote each tick */
    lora_discovery_packet_init(&engine->tx_buffer);
    engine->tx_buffer.sender_id = engine->node.node_id;
    engine->tx_buffer.ttl = 6;
    engine->tx_buffer.channel = engine->node.channel;
    engine->tx_buffer.home_channel = engine->node.home_channel;
    engine->tx_buffer.target_channel = engine->node.target_channel;
    engine->tx_buffer.crowding = (uint8_t)(engine->node.crowding_factor * 255.0);
    engine->tx_buffer.is_clusterhead = engine->node.is_clusterhead;
    lora_discovery_add_to_path(&engine->tx_buffer, engine->node.node_id);
    if (engine->config.send_cb) {
        engine->config.send_cb(&engine->tx_buffer, engine->config.user_context);
    }
}

bool
lora_engine_receive_packet(lora_engine_t *engine,
                           const lora_discovery_packet_t *packet,
                           int8_t rssi,
                           uint32_t now_ms)
{
    if (!engine || !packet) {
        return false;
    }
    (void)now_ms;
    /* Minimal channel check: only accept on current channel */
    if (packet->channel != engine->node.channel) {
        return false;
    }
    /* Placeholder: crowding tracking */
    if (rssi > -80) {
        double cf = engine->node.crowding_factor + 0.05;
        if (cf > 1.0) {
            cf = 1.0;
        }
        lora_mesh_node_set_crowding(&engine->node, cf);
        /* Vote to move up a channel when crowded and channels available */
        if (engine->node.channel < engine->config.max_channels) {
            engine->node.target_channel = engine->node.channel + 1;
        }
    }
    /* Simple clusterhead adoption: lowest sender_id becomes clusterhead */
    if (!engine->node.is_clusterhead && packet->is_clusterhead) {
        if (engine->node.clusterhead_id == 0 || packet->sender_id < engine->node.clusterhead_id) {
            engine->node.clusterhead_id = packet->sender_id;
        }
    }
    return true;
}

void
lora_engine_set_crowding(lora_engine_t *engine, double crowding)
{
    if (!engine) {
        return;
    }
    lora_mesh_node_set_crowding(&engine->node, crowding);
}
