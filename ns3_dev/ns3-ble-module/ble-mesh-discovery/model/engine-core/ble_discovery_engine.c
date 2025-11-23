/**
 * @file ble_discovery_engine.c
 * @brief Phase 2 BLE discovery event engine (portable C implementation)
 */

#include "ble_discovery_engine.h"
#include <string.h>
#include <limits.h>

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

static void
ble_engine_enter_phase(ble_engine_t *engine, ble_engine_phase_t phase, uint32_t now_ms);

static bool
ble_engine_run_phase_slot(ble_engine_t *engine, uint32_t now_ms);

static uint32_t
ble_engine_neighbor_timeout_ms(const ble_engine_t *engine);

static void
ble_engine_publish_metrics(ble_engine_t *engine);

static void
ble_engine_evaluate_state(ble_engine_t *engine);

static void
ble_engine_start_election_rounds(ble_engine_t *engine);

static void
ble_engine_cancel_election_rounds(ble_engine_t *engine);

static bool
ble_engine_should_send_election(const ble_engine_t *engine);

static void
ble_engine_prepare_election_packet(ble_engine_t *engine);

static void
ble_engine_send_election_packet(ble_engine_t *engine);

static void
ble_engine_handle_election_packet(ble_engine_t *engine,
                                  const ble_election_packet_t *packet,
                                  int8_t rssi,
                                  uint32_t now_ms);

static bool
ble_engine_should_send_renouncement(const ble_engine_t *engine);

static void
ble_engine_prepare_renouncement_packet(ble_engine_t *engine);

static void
ble_engine_send_renouncement_packet(ble_engine_t *engine);

static void
ble_engine_start_renouncement_rounds(ble_engine_t *engine);

static void
ble_engine_cancel_renouncement_rounds(ble_engine_t *engine);

static void
ble_engine_clear_selected_clusterhead(ble_engine_t *engine);

static void
ble_engine_try_promote_clusterhead(ble_engine_t *engine);

static void
ble_engine_update_clusterhead_selection(ble_engine_t *engine,
                                        const ble_election_packet_t *packet);

static void
ble_engine_handle_clusterhead_renouncement(ble_engine_t *engine,
                                            const ble_election_packet_t *packet);

static uint32_t
ble_engine_count_already_reached(ble_engine_t *engine,
                                 const ble_discovery_packet_t *packet);

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
    config->noise_slot_count = BLE_ENGINE_DEFAULT_NOISE_SLOTS;
    config->noise_slot_duration_ms = BLE_ENGINE_DEFAULT_NOISE_SLOT_DURATION_MS;
    config->neighbor_slot_count = BLE_ENGINE_DEFAULT_NEIGHBOR_SLOTS;
    config->neighbor_slot_duration_ms = BLE_ENGINE_DEFAULT_NEIGHBOR_SLOT_DURATION_MS;
    config->neighbor_timeout_cycles = BLE_ENGINE_DEFAULT_NEIGHBOR_TIMEOUT_CYCLES;
}

bool
ble_engine_init(ble_engine_t *engine, const ble_engine_config_t *config)
{
    if (!engine || !config || config->node_id == 0 || config->send_cb == NULL) {
        return false;
    }

    memset(engine, 0, sizeof(*engine));
    engine->config = *config;
    if (engine->config.noise_slot_count == 0) {
        engine->config.noise_slot_count = BLE_ENGINE_DEFAULT_NOISE_SLOTS;
    }
    if (engine->config.noise_slot_duration_ms == 0) {
        engine->config.noise_slot_duration_ms = BLE_ENGINE_DEFAULT_NOISE_SLOT_DURATION_MS;
    }
    if (engine->config.neighbor_slot_count == 0) {
        engine->config.neighbor_slot_count = BLE_ENGINE_DEFAULT_NEIGHBOR_SLOTS;
    }
    if (engine->config.neighbor_slot_duration_ms == 0) {
        engine->config.neighbor_slot_duration_ms = BLE_ENGINE_DEFAULT_NEIGHBOR_SLOT_DURATION_MS;
    }
    engine->proximity_threshold = config->proximity_threshold;
    engine->neighbor_timeout_cycles =
        (config->neighbor_timeout_cycles == 0) ? BLE_ENGINE_DEFAULT_NEIGHBOR_TIMEOUT_CYCLES
                                               : config->neighbor_timeout_cycles;
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
    ble_mesh_node_set_state(&engine->node, BLE_NODE_STATE_DISCOVERY);
    ble_election_init(&engine->election);
    ble_broadcast_timing_init(&engine->noisy_timing,
                              BLE_BROADCAST_SCHEDULE_NOISY,
                              config->noise_slot_count,
                              config->noise_slot_duration_ms,
                              BLE_BROADCAST_NOISE_LISTEN_RATIO);
    ble_broadcast_timing_init(&engine->neighbor_timing,
                              BLE_BROADCAST_SCHEDULE_STOCHASTIC,
                              config->neighbor_slot_count,
                              config->neighbor_slot_duration_ms,
                              BLE_BROADCAST_NEIGHBOR_LISTEN_RATIO);
    engine->phase = BLE_ENGINE_PHASE_NOISY;
    engine->noisy_slots_completed = 0;
    engine->neighbor_slots_completed = 0;
    engine->phase_listen_active = true;
    engine->crowding_factor = 0.0;
    engine->last_tick_time_ms = 0;
    engine->initialized = true;
    engine->election_rounds_remaining = 0;
    engine->last_election_cycle_sent = UINT32_MAX;
    engine->renouncement_rounds_remaining = 0;
    engine->last_renouncement_cycle_sent = UINT32_MAX;
    engine->selected_clusterhead_hops = UINT16_MAX;
    engine->selected_clusterhead_direct_connections = 0;
    ble_engine_enter_phase(engine, BLE_ENGINE_PHASE_NOISY, 0);
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
    ble_mesh_node_set_state(&engine->node, BLE_NODE_STATE_DISCOVERY);
    ble_election_init(&engine->election);
    ble_broadcast_timing_init(&engine->noisy_timing,
                              BLE_BROADCAST_SCHEDULE_NOISY,
                              engine->config.noise_slot_count,
                              engine->config.noise_slot_duration_ms,
                              BLE_BROADCAST_NOISE_LISTEN_RATIO);
    ble_broadcast_timing_init(&engine->neighbor_timing,
                              BLE_BROADCAST_SCHEDULE_STOCHASTIC,
                              engine->config.neighbor_slot_count,
                              engine->config.neighbor_slot_duration_ms,
                              BLE_BROADCAST_NEIGHBOR_LISTEN_RATIO);
    engine->phase = BLE_ENGINE_PHASE_NOISY;
    engine->noisy_slots_completed = 0;
    engine->neighbor_slots_completed = 0;
    engine->phase_listen_active = true;
    engine->crowding_factor = 0.0;
    engine->last_tick_time_ms = 0;
    engine->election_rounds_remaining = 0;
    engine->last_election_cycle_sent = UINT32_MAX;
    engine->renouncement_rounds_remaining = 0;
    engine->last_renouncement_cycle_sent = UINT32_MAX;
    engine->selected_clusterhead_hops = UINT16_MAX;
    engine->selected_clusterhead_direct_connections = 0;
    ble_engine_enter_phase(engine, BLE_ENGINE_PHASE_NOISY, 0);
}

void
ble_engine_tick(ble_engine_t *engine, uint32_t now_ms)
{
    if (!engine || !engine->initialized) {
        return;
    }

    engine->last_tick_time_ms = now_ms;

    if (ble_engine_run_phase_slot(engine, now_ms)) {
        return;
    }

    if (!ble_discovery_cycle_is_running(&engine->cycle)) {
        if (engine->phase == BLE_ENGINE_PHASE_DISCOVERY) {
            if (!ble_discovery_cycle_start(&engine->cycle)) {
                ble_engine_log(engine, "WARN", "Cycle already running");
            }
        } else {
            return;
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

    bool in_noisy_phase = (engine->phase == BLE_ENGINE_PHASE_NOISY);
    bool in_neighbor_phase = (engine->phase == BLE_ENGINE_PHASE_NEIGHBOR);
    bool is_election = (packet->message_type == BLE_MSG_ELECTION_ANNOUNCEMENT);

    if (is_election) {
        const ble_election_packet_t *election_packet =
            (const ble_election_packet_t *)packet;
        ble_engine_handle_election_packet(engine, election_packet, rssi, now_ms);
    }

    if (in_noisy_phase &&
        !is_election &&
        engine->phase_listen_active &&
        ble_election_is_crowding_measurement_active(&engine->election)) {
        ble_election_add_rssi_sample(&engine->election, rssi, now_ms);
    }

    if (in_neighbor_phase && engine->phase_listen_active && !is_election) {
        ble_mesh_node_add_neighbor(&engine->node,
                                   packet->sender_id,
                                   rssi,
                                   1);
        const ble_gps_location_t *location =
            packet->gps_available ? &packet->gps_location : NULL;
        ble_election_update_neighbor(&engine->election,
                                     packet->sender_id,
                                     location,
                                     rssi,
                                     now_ms);
    }

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
    ble_broadcast_timing_set_crowding(&engine->neighbor_timing, crowding_factor);
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
    ble_mesh_node_prune_stale_neighbors(&engine->node,
                                        engine->neighbor_timeout_cycles);
    uint32_t timeout_ms = ble_engine_neighbor_timeout_ms(engine);
    if (timeout_ms > 0) {
        ble_election_clean_old_neighbors(&engine->election,
                                         engine->last_tick_time_ms,
                                         timeout_ms);
    }
    ble_engine_publish_metrics(engine);
    ble_engine_evaluate_state(engine);
    ble_engine_enter_phase(engine, BLE_ENGINE_PHASE_NOISY, engine->last_tick_time_ms);
    (void)cycle_count;
}

static void
ble_engine_transmit_own_message(ble_engine_t *engine)
{
    if (ble_engine_should_send_renouncement(engine)) {
        ble_engine_send_renouncement_packet(engine);
        return;
    }

    if (ble_engine_should_send_election(engine)) {
        ble_engine_send_election_packet(engine);
        return;
    }

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
    ble_election_packet_t peek_packet;
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
    bool should_forward = ble_forwarding_should_forward(&peek_packet.base,
                                                        current_location,
                                                        engine->crowding_factor,
                                                        engine->proximity_threshold,
                                                        direct_neighbors);

    ble_election_packet_t packet_to_process;
    if (!ble_queue_dequeue(&engine->forward_queue, &packet_to_process)) {
        return;
    }

    if (!should_forward) {
        ble_mesh_node_inc_dropped(&engine->node);
        return;
    }

    bool is_election = (packet_to_process.base.message_type == BLE_MSG_ELECTION_ANNOUNCEMENT);
    bool is_renouncement = false;
    if (is_election) {
        is_renouncement = packet_to_process.election.is_renouncement;
    }

    if (is_election && !is_renouncement &&
        packet_to_process.election.pdsf >= BLE_DISCOVERY_MAX_CLUSTER_SIZE) {
        ble_mesh_node_inc_dropped(&engine->node);
        return;
    }

    if (!ble_discovery_decrement_ttl(&packet_to_process.base)) {
        ble_mesh_node_inc_dropped(&engine->node);
        return;
    }

    if (is_election && !is_renouncement) {
        uint32_t already_reached =
            ble_engine_count_already_reached(engine, &packet_to_process.base);
        ble_election_update_pdsf(&packet_to_process, direct_neighbors, already_reached);
        if (packet_to_process.election.pdsf >= BLE_DISCOVERY_MAX_CLUSTER_SIZE) {
            ble_mesh_node_inc_dropped(&engine->node);
            return;
        }
    }

    ble_discovery_add_to_path(&packet_to_process.base, engine->node.node_id);
    if (engine->node.gps_available) {
        ble_discovery_set_gps(&packet_to_process.base,
                              engine->node.gps_location.x,
                              engine->node.gps_location.y,
                              engine->node.gps_location.z);
    }

    if (engine->config.send_cb) {
        engine->config.send_cb((const ble_discovery_packet_t *)&packet_to_process,
                               engine->config.user_context);
        ble_mesh_node_inc_forwarded(&engine->node);
    }
}

static void
ble_engine_enter_phase(ble_engine_t *engine, ble_engine_phase_t phase, uint32_t now_ms)
{
    if (!engine) {
        return;
    }

    engine->phase = phase;
    engine->phase_listen_active = true;

    switch (phase) {
    case BLE_ENGINE_PHASE_NOISY: {
        engine->noisy_slots_completed = 0;
        ble_broadcast_timing_init(&engine->noisy_timing,
                                  BLE_BROADCAST_SCHEDULE_NOISY,
                                  engine->config.noise_slot_count,
                                  engine->config.noise_slot_duration_ms,
                                  BLE_BROADCAST_NOISE_LISTEN_RATIO);
        uint32_t window_ms =
            engine->config.noise_slot_count * engine->config.noise_slot_duration_ms;
        ble_election_begin_crowding_measurement(&engine->election, window_ms);
        break;
    }
    case BLE_ENGINE_PHASE_NEIGHBOR:
        engine->neighbor_slots_completed = 0;
        ble_broadcast_timing_init(&engine->neighbor_timing,
                                  BLE_BROADCAST_SCHEDULE_STOCHASTIC,
                                  engine->config.neighbor_slot_count,
                                  engine->config.neighbor_slot_duration_ms,
                                  BLE_BROADCAST_NEIGHBOR_LISTEN_RATIO);
        ble_broadcast_timing_set_crowding(&engine->neighbor_timing, engine->crowding_factor);
        break;
    case BLE_ENGINE_PHASE_DISCOVERY:
    default:
        break;
    }

    (void)now_ms;
}

static bool
ble_engine_run_phase_slot(ble_engine_t *engine, uint32_t now_ms)
{
    if (!engine || engine->phase == BLE_ENGINE_PHASE_DISCOVERY) {
        return false;
    }

    ble_broadcast_timing_t *timing =
        (engine->phase == BLE_ENGINE_PHASE_NOISY) ? &engine->noisy_timing : &engine->neighbor_timing;
    ble_broadcast_timing_advance_slot(timing);
    engine->phase_listen_active = ble_broadcast_timing_should_listen(timing);

    if (engine->phase == BLE_ENGINE_PHASE_NOISY) {
        engine->noisy_slots_completed++;
        if (engine->noisy_slots_completed >= engine->config.noise_slot_count) {
            double factor = ble_election_end_crowding_measurement(&engine->election);
            ble_engine_set_crowding_factor(engine, factor);
            ble_engine_set_noise_level(engine, factor);
            ble_engine_enter_phase(engine, BLE_ENGINE_PHASE_NEIGHBOR, now_ms);
        }
    } else if (engine->phase == BLE_ENGINE_PHASE_NEIGHBOR) {
        engine->neighbor_slots_completed++;
        if (engine->neighbor_slots_completed >= engine->config.neighbor_slot_count) {
            ble_engine_enter_phase(engine, BLE_ENGINE_PHASE_DISCOVERY, now_ms);
        }
    }

    return true;
}

static uint32_t
ble_engine_neighbor_timeout_ms(const ble_engine_t *engine)
{
    if (!engine) {
        return 0;
    }
    uint32_t cycle_duration = ble_discovery_cycle_get_cycle_duration(&engine->cycle);
    if (cycle_duration == 0) {
        cycle_duration = engine->config.slot_duration_ms * BLE_DISCOVERY_NUM_SLOTS;
    }
    return cycle_duration * engine->neighbor_timeout_cycles;
}

static void
ble_engine_publish_metrics(ble_engine_t *engine)
{
    if (!engine) {
        return;
    }

    ble_mesh_node_update_statistics(&engine->node);
    ble_election_update_metrics(&engine->election);
    engine->last_metrics = engine->election.metrics;

    if (engine->config.metrics_cb) {
        engine->config.metrics_cb(&engine->last_metrics, engine->config.user_context);
    }
}

static void
ble_engine_evaluate_state(ble_engine_t *engine)
{
    if (!engine) {
        return;
    }

    ble_node_state_t current_state = ble_mesh_node_get_state(&engine->node);
    if (current_state == BLE_NODE_STATE_INIT) {
        if (!ble_mesh_node_set_state(&engine->node, BLE_NODE_STATE_DISCOVERY)) {
            return;
        }
        current_state = BLE_NODE_STATE_DISCOVERY;
    }

    if (ble_mesh_node_should_become_edge(&engine->node)) {
        if (current_state != BLE_NODE_STATE_EDGE &&
            ble_mesh_node_set_state(&engine->node, BLE_NODE_STATE_EDGE)) {
            ble_engine_log(engine, "INFO", "Node transitioned to EDGE state");
        }
        ble_engine_cancel_election_rounds(engine);
        return;
    }

    if (ble_mesh_node_should_become_candidate(&engine->node)) {
        if (current_state != BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE &&
            ble_mesh_node_set_state(&engine->node, BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE)) {
            double score = ble_mesh_node_calculate_candidacy_score(&engine->node,
                                                                  engine->node.noise_level);
            engine->node.candidacy_score = score;
            ble_engine_log(engine, "INFO", "Node transitioned to CLUSTERHEAD_CANDIDATE state");
            ble_engine_clear_selected_clusterhead(engine);
            ble_engine_start_election_rounds(engine);
        }
        current_state = ble_mesh_node_get_state(&engine->node);
    }

    if (ble_mesh_node_get_state(&engine->node) == BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE) {
        ble_engine_try_promote_clusterhead(engine);
        return;
    }

    ble_engine_cancel_election_rounds(engine);
}

static void
ble_engine_start_election_rounds(ble_engine_t *engine)
{
    if (!engine) {
        return;
    }
    engine->election_rounds_remaining = BLE_ENGINE_MAX_ELECTION_ROUNDS;
    engine->last_election_cycle_sent = UINT32_MAX;
    ble_engine_cancel_renouncement_rounds(engine);
}

static void
ble_engine_cancel_election_rounds(ble_engine_t *engine)
{
    if (!engine) {
        return;
    }
    engine->election_rounds_remaining = 0;
    engine->last_election_cycle_sent = UINT32_MAX;
}

static bool
ble_engine_should_send_election(const ble_engine_t *engine)
{
    if (!engine) {
        return false;
    }
    if (ble_mesh_node_get_state(&engine->node) != BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE) {
        return false;
    }
    if (engine->election_rounds_remaining == 0) {
        return false;
    }
    if (engine->last_election_cycle_sent == engine->node.current_cycle) {
        return false;
    }
    return true;
}

static void
ble_engine_prepare_election_packet(ble_engine_t *engine)
{
    if (!engine) {
        return;
    }

    ble_election_packet_init(&engine->election_packet);
    engine->election_packet.base.sender_id = engine->node.node_id;
    engine->election_packet.base.ttl = engine->config.initial_ttl;
    ble_discovery_add_to_path(&engine->election_packet.base, engine->node.node_id);
    if (engine->node.gps_available) {
        ble_discovery_set_gps(&engine->election_packet.base,
                              engine->node.gps_location.x,
                              engine->node.gps_location.y,
                              engine->node.gps_location.z);
    }

    uint32_t direct_connections = ble_mesh_node_count_direct_neighbors(&engine->node);
    engine->election_packet.election.class_id = engine->node.cluster_class;
    engine->election_packet.election.direct_connections = direct_connections;
    engine->election_packet.election.hash = engine->node.election_hash;

    double score = ble_election_calculate_score(direct_connections, engine->node.noise_level);
    engine->election_packet.election.score = score;
    engine->node.candidacy_score = score;

    ble_election_pdsf_history_reset(&engine->election_packet.election.pdsf_history);
    engine->election_packet.election.pdsf = 0;
    engine->election_packet.election.last_pi = 1;
    uint32_t already_reached =
        ble_engine_count_already_reached(engine, &engine->election_packet.base);
    ble_election_update_pdsf(&engine->election_packet, direct_connections, already_reached);
    engine->node.pdsf = engine->election_packet.election.pdsf;
}

static void
ble_engine_send_election_packet(ble_engine_t *engine)
{
    if (!engine) {
        return;
    }

    ble_engine_prepare_election_packet(engine);

    if (engine->config.send_cb) {
        const ble_discovery_packet_t *packet =
            (const ble_discovery_packet_t *)&engine->election_packet;
        engine->config.send_cb(packet, engine->config.user_context);
    }

    ble_mesh_node_inc_sent(&engine->node);
    if (engine->election_rounds_remaining > 0) {
        engine->election_rounds_remaining--;
    }
    engine->last_election_cycle_sent = engine->node.current_cycle;
}

static bool
ble_engine_should_send_renouncement(const ble_engine_t *engine)
{
    if (!engine) {
        return false;
    }
    if (engine->renouncement_rounds_remaining == 0) {
        return false;
    }
    if (engine->last_renouncement_cycle_sent == engine->node.current_cycle) {
        return false;
    }
    return true;
}

static void
ble_engine_prepare_renouncement_packet(ble_engine_t *engine)
{
    if (!engine) {
        return;
    }

    ble_election_packet_init(&engine->renouncement_packet);
    engine->renouncement_packet.base.sender_id = engine->node.node_id;
    engine->renouncement_packet.base.ttl = engine->config.initial_ttl;
    ble_discovery_add_to_path(&engine->renouncement_packet.base, engine->node.node_id);
    if (engine->node.gps_available) {
        ble_discovery_set_gps(&engine->renouncement_packet.base,
                              engine->node.gps_location.x,
                              engine->node.gps_location.y,
                              engine->node.gps_location.z);
    }

    engine->renouncement_packet.election.is_renouncement = true;
    engine->renouncement_packet.election.direct_connections = 0;
    engine->renouncement_packet.election.score = 0.0;
    engine->renouncement_packet.election.pdsf = 0;
    engine->renouncement_packet.election.last_pi = 1;
    ble_election_pdsf_history_reset(&engine->renouncement_packet.election.pdsf_history);
}

static void
ble_engine_send_renouncement_packet(ble_engine_t *engine)
{
    if (!engine) {
        return;
    }

    ble_engine_prepare_renouncement_packet(engine);

    if (engine->config.send_cb) {
        const ble_discovery_packet_t *packet =
            (const ble_discovery_packet_t *)&engine->renouncement_packet;
        engine->config.send_cb(packet, engine->config.user_context);
    }

    ble_mesh_node_inc_sent(&engine->node);
    if (engine->renouncement_rounds_remaining > 0) {
        engine->renouncement_rounds_remaining--;
    }
    engine->last_renouncement_cycle_sent = engine->node.current_cycle;
}

static void
ble_engine_start_renouncement_rounds(ble_engine_t *engine)
{
    if (!engine) {
        return;
    }
    engine->renouncement_rounds_remaining = BLE_ENGINE_MAX_ELECTION_ROUNDS;
    engine->last_renouncement_cycle_sent = UINT32_MAX;
}

static void
ble_engine_cancel_renouncement_rounds(ble_engine_t *engine)
{
    if (!engine) {
        return;
    }
    engine->renouncement_rounds_remaining = 0;
    engine->last_renouncement_cycle_sent = UINT32_MAX;
}

static void
ble_engine_clear_selected_clusterhead(ble_engine_t *engine)
{
    if (!engine) {
        return;
    }
    engine->node.clusterhead_id = BLE_MESH_INVALID_NODE_ID;
    engine->node.cluster_class = 0;
    engine->selected_clusterhead_direct_connections = 0;
    engine->selected_clusterhead_hops = UINT16_MAX;
}

static void
ble_engine_try_promote_clusterhead(ble_engine_t *engine)
{
    if (!engine) {
        return;
    }
    if (ble_mesh_node_get_state(&engine->node) != BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE) {
        return;
    }
    if (engine->election_rounds_remaining > 0) {
        return;
    }
    if (engine->node.current_cycle <= engine->last_election_cycle_sent) {
        return;
    }
    if (ble_mesh_node_set_state(&engine->node, BLE_NODE_STATE_CLUSTERHEAD)) {
        ble_engine_log(engine, "INFO", "Node promoted to CLUSTERHEAD state");
    }
}

static void
ble_engine_update_clusterhead_selection(ble_engine_t *engine,
                                        const ble_election_packet_t *packet)
{
    if (!engine || !packet || packet->election.is_renouncement) {
        return;
    }

    ble_node_state_t state = ble_mesh_node_get_state(&engine->node);
    if (state == BLE_NODE_STATE_CLUSTERHEAD || state == BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE) {
        return;
    }

    uint16_t incoming_hops = packet->base.path_length;
    if (incoming_hops == 0) {
        incoming_hops = 1;
    }

    bool accept = false;
    if (engine->node.clusterhead_id == BLE_MESH_INVALID_NODE_ID) {
        accept = true;
    } else if (incoming_hops < engine->selected_clusterhead_hops) {
        accept = true;
    } else if (incoming_hops == engine->selected_clusterhead_hops) {
        if (packet->election.direct_connections >
            engine->selected_clusterhead_direct_connections) {
            accept = true;
        } else if (packet->election.direct_connections ==
                   engine->selected_clusterhead_direct_connections &&
                   packet->base.sender_id < engine->node.clusterhead_id) {
            accept = true;
        }
    }

    if (accept) {
        engine->node.clusterhead_id = packet->base.sender_id;
        engine->node.cluster_class = packet->election.class_id;
        engine->selected_clusterhead_direct_connections = packet->election.direct_connections;
        engine->selected_clusterhead_hops = incoming_hops;
        engine->node.pdsf = packet->election.pdsf;
        ble_engine_log(engine, "INFO", "Adopted new clusterhead candidate");
    }
}

static void
ble_engine_handle_clusterhead_renouncement(ble_engine_t *engine,
                                            const ble_election_packet_t *packet)
{
    if (!engine || !packet) {
        return;
    }
    if (!packet->election.is_renouncement) {
        return;
    }

    if (engine->node.clusterhead_id == packet->base.sender_id) {
        ble_engine_log(engine, "INFO", "Selected clusterhead renounced; clearing alignment");
        ble_engine_clear_selected_clusterhead(engine);
        if (ble_mesh_node_get_state(&engine->node) == BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE) {
            return;
        }
        /* Re-enter discovery so the node can search for a new clusterhead */
        ble_mesh_node_set_state(&engine->node, BLE_NODE_STATE_DISCOVERY);
    }
}

static uint32_t
ble_engine_count_already_reached(ble_engine_t *engine,
                                 const ble_discovery_packet_t *packet)
{
    if (!engine || !packet || packet->path_length == 0) {
        return 0;
    }

    uint32_t count = 0;
    for (uint16_t i = 0; i < packet->path_length; i++) {
        ble_neighbor_info_t *neighbor =
            ble_mesh_node_find_neighbor(&engine->node, packet->path[i]);
        if (neighbor && neighbor->hop_count == 1) {
            count++;
        }
    }

    uint32_t direct_neighbors = ble_mesh_node_count_direct_neighbors(&engine->node);
    if (count > direct_neighbors) {
        count = direct_neighbors;
    }
    return count;
}

static void
ble_engine_handle_election_packet(ble_engine_t *engine,
                                  const ble_election_packet_t *packet,
                                  int8_t rssi,
                                  uint32_t now_ms)
{
    (void)rssi;
    (void)now_ms;

    if (!engine || !packet) {
        return;
    }

    ble_mesh_node_mark_candidate_heard(&engine->node);

    if (packet->election.is_renouncement) {
        ble_engine_handle_clusterhead_renouncement(engine, packet);
        return;
    }

    if (ble_mesh_node_get_state(&engine->node) == BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE) {
        uint32_t local_direct = ble_mesh_node_count_direct_neighbors(&engine->node);
        uint32_t remote_direct = packet->election.direct_connections;
        bool remote_better = false;

        if (remote_direct > local_direct) {
            remote_better = true;
        } else if (remote_direct == local_direct &&
                   packet->base.sender_id < engine->node.node_id) {
            remote_better = true;
        }

        if (remote_better) {
            if (ble_mesh_node_set_state(&engine->node, BLE_NODE_STATE_EDGE)) {
                ble_engine_log(engine,
                               "INFO",
                               "Heard stronger candidate; reverting to EDGE state");
            }
            ble_engine_cancel_election_rounds(engine);
            ble_engine_start_renouncement_rounds(engine);
            ble_engine_clear_selected_clusterhead(engine);
            ble_engine_update_clusterhead_selection(engine, packet);
        }
        return;
    }

    ble_engine_update_clusterhead_selection(engine, packet);
}
