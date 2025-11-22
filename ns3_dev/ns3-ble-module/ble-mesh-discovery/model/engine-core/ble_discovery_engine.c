/**
 * @file ble_discovery_engine.c
 * @brief Phase 2 BLE discovery event engine (portable C implementation)
 */

#include "ble_discovery_engine.h"
#include <string.h>

static void
ble_engine_log(const ble_engine_t *engine, const char *level, const char *message)
{
    if (engine && engine->config.log_cb) {
        engine->config.log_cb(level, message, engine->config.user_context);
    }
}

static void
ble_engine_slot_dispatch(uint8_t slot, void *user_context);

static void
ble_engine_cycle_complete(uint32_t cycle_count, void *user_context);

static void
ble_engine_transmit_own_message(ble_engine_t *engine);

static void
ble_engine_forward_next_message(ble_engine_t *engine);

void
ble_engine_config_init(ble_engine_config_t *config)
{
    if (!config) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->slot_duration_ms = BLE_DISCOVERY_DEFAULT_SLOT_DURATION_MS;
    config->initial_ttl = BLE_DISCOVERY_DEFAULT_TTL;
    config->proximity_threshold = 10.0;
}

bool
ble_engine_init(ble_engine_t *engine, const ble_engine_config_t *config)
{
    if (!engine || !config || config->node_id == 0 || config->send_cb == NULL) {
        return false;
    }

    memset(engine, 0, sizeof(*engine));
    engine->config = *config;
    engine->proximity_threshold = config->proximity_threshold;
    ble_discovery_cycle_init(&engine->cycle);
    ble_discovery_cycle_set_slot_callback(&engine->cycle,
                                          BLE_DISCOVERY_SLOT_OWN_MESSAGE,
                                          ble_engine_slot_dispatch);
    ble_discovery_cycle_set_slot_callback(&engine->cycle,
                                          BLE_DISCOVERY_SLOT_FORWARD_1,
                                          ble_engine_slot_dispatch);
    ble_discovery_cycle_set_slot_callback(&engine->cycle,
                                          BLE_DISCOVERY_SLOT_FORWARD_2,
                                          ble_engine_slot_dispatch);
    ble_discovery_cycle_set_slot_callback(&engine->cycle,
                                          BLE_DISCOVERY_SLOT_FORWARD_3,
                                          ble_engine_slot_dispatch);
    ble_discovery_cycle_set_complete_callback(&engine->cycle,
                                              ble_engine_cycle_complete);
    ble_discovery_cycle_set_user_data(&engine->cycle, engine);
    ble_discovery_cycle_set_slot_duration(&engine->cycle, config->slot_duration_ms);

    ble_queue_init(&engine->forward_queue);

    ble_mesh_node_init(&engine->node, config->node_id);
    engine->crowding_factor = 0.0;
    engine->last_tick_time_ms = 0;
    engine->initialized = true;
    return true;
}

void
ble_engine_reset(ble_engine_t *engine)
{
    if (!engine) {
        return;
    }
    ble_queue_clear(&engine->forward_queue);
    ble_discovery_cycle_stop(&engine->cycle);
    ble_mesh_node_init(&engine->node, engine->config.node_id);
    engine->crowding_factor = 0.0;
    engine->last_tick_time_ms = 0;
}

void
ble_engine_tick(ble_engine_t *engine, uint32_t now_ms)
{
    if (!engine || !engine->initialized) {
        return;
    }

    engine->last_tick_time_ms = now_ms;

    if (!ble_discovery_cycle_is_running(&engine->cycle)) {
        if (!ble_discovery_cycle_start(&engine->cycle)) {
            ble_engine_log(engine, "WARN", "Cycle already running");
        }
    }

    ble_discovery_cycle_execute_slot(&engine->cycle);
    ble_discovery_cycle_advance_slot(&engine->cycle);
}

bool
ble_engine_receive_packet(ble_engine_t *engine,
                          const ble_discovery_packet_t *packet,
                          int8_t rssi,
                          uint32_t now_ms)
{
    if (!engine || !packet) {
        return false;
    }

    ble_mesh_node_add_neighbor(&engine->node,
                               packet->sender_id,
                               rssi,
                               1);

    bool enqueued = ble_queue_enqueue(&engine->forward_queue,
                                      packet,
                                      engine->node.node_id,
                                      now_ms);
    if (!enqueued) {
        ble_engine_log(engine, "DEBUG", "Queue rejected incoming packet");
    } else {
        ble_mesh_node_inc_received(&engine->node);
    }

    return enqueued;
}

void
ble_engine_set_noise_level(ble_engine_t *engine, double noise_level)
{
    if (!engine) {
        return;
    }
    ble_mesh_node_set_noise_level(&engine->node, noise_level);
}

void
ble_engine_mark_candidate_heard(ble_engine_t *engine)
{
    if (!engine) {
        return;
    }
    ble_mesh_node_mark_candidate_heard(&engine->node);
}

void
ble_engine_set_crowding_factor(ble_engine_t *engine, double crowding_factor)
{
    if (!engine) {
        return;
    }
    if (crowding_factor < 0.0) {
        crowding_factor = 0.0;
    }
    if (crowding_factor > 1.0) {
        crowding_factor = 1.0;
    }
    engine->crowding_factor = crowding_factor;
}

void
ble_engine_seed_random(uint32_t seed)
{
    ble_forwarding_set_random_seed(seed);
}

void
ble_engine_set_gps(ble_engine_t *engine,
                   double x,
                   double y,
                   double z,
                   bool valid)
{
    if (!engine) {
        return;
    }
    if (valid) {
        ble_mesh_node_set_gps(&engine->node, x, y, z);
    } else {
        ble_mesh_node_clear_gps(&engine->node);
    }
}

const ble_mesh_node_t*
ble_engine_get_node(const ble_engine_t *engine)
{
    return engine ? &engine->node : NULL;
}

static void
ble_engine_slot_dispatch(uint8_t slot, void *user_context)
{
    ble_engine_t *engine = (ble_engine_t *)user_context;
    if (!engine || !engine->initialized) {
        return;
    }

    switch (slot) {
    case BLE_DISCOVERY_SLOT_OWN_MESSAGE:
        ble_engine_transmit_own_message(engine);
        break;
    case BLE_DISCOVERY_SLOT_FORWARD_1:
    case BLE_DISCOVERY_SLOT_FORWARD_2:
    case BLE_DISCOVERY_SLOT_FORWARD_3:
        ble_engine_forward_next_message(engine);
        break;
    default:
        break;
    }
}

static void
ble_engine_cycle_complete(uint32_t cycle_count, void *user_context)
{
    ble_engine_t *engine = (ble_engine_t *)user_context;
    if (!engine) {
        return;
    }
    ble_mesh_node_advance_cycle(&engine->node);
    (void)cycle_count;
}

static void
ble_engine_transmit_own_message(ble_engine_t *engine)
{
    ble_discovery_packet_init(&engine->tx_buffer);
    engine->tx_buffer.sender_id = engine->node.node_id;
    engine->tx_buffer.ttl = engine->config.initial_ttl;
    ble_discovery_add_to_path(&engine->tx_buffer, engine->node.node_id);

    if (engine->node.gps_available) {
        ble_discovery_set_gps(&engine->tx_buffer,
                              engine->node.gps_location.x,
                              engine->node.gps_location.y,
                              engine->node.gps_location.z);
    }

    ble_mesh_node_inc_sent(&engine->node);
    if (engine->config.send_cb) {
        engine->config.send_cb(&engine->tx_buffer, engine->config.user_context);
    }
}

static void
ble_engine_forward_next_message(ble_engine_t *engine)
{
    ble_discovery_packet_t peek_packet;
    if (!ble_queue_peek(&engine->forward_queue, &peek_packet)) {
        return;
    }

    const ble_gps_location_t *current_location = NULL;
    ble_gps_location_t local_location;
    if (engine->node.gps_available) {
        local_location = engine->node.gps_location;
        current_location = &local_location;
    }

    uint32_t direct_neighbors = ble_mesh_node_count_direct_neighbors(&engine->node);
    bool should_forward = ble_forwarding_should_forward(&peek_packet,
                                                        current_location,
                                                        engine->crowding_factor,
                                                        engine->proximity_threshold,
                                                        direct_neighbors);

    ble_discovery_packet_t packet_to_process;
    if (!ble_queue_dequeue(&engine->forward_queue, &packet_to_process)) {
        return;
    }

    if (!should_forward) {
        ble_mesh_node_inc_dropped(&engine->node);
        return;
    }

    if (!ble_discovery_decrement_ttl(&packet_to_process)) {
        ble_mesh_node_inc_dropped(&engine->node);
        return;
    }

    ble_discovery_add_to_path(&packet_to_process, engine->node.node_id);
    if (engine->node.gps_available) {
        ble_discovery_set_gps(&packet_to_process,
                              engine->node.gps_location.x,
                              engine->node.gps_location.y,
                              engine->node.gps_location.z);
    }

    if (engine->config.send_cb) {
        engine->config.send_cb(&packet_to_process, engine->config.user_context);
        ble_mesh_node_inc_forwarded(&engine->node);
    }
}
