/**
 * @file ble-phase3-engine-c-test.c
 * @brief Standalone C test combining Phase 2 forwarding + Phase 3 engine logic
 *
 * Build example:
 * gcc -I../model/protocol-core -I../model/engine-core \
 *     test/ble-phase3-engine-c-test.c \
 *     model/protocol-core/ble_discovery_packet.c \
 *     model/protocol-core/ble_mesh_node.c \
 *     model/protocol-core/ble_discovery_cycle.c \
 *     model/protocol-core/ble_message_queue.c \
 *     model/protocol-core/ble_forwarding_logic.c \
 *     model/protocol-core/ble_election.c \
 *     model/protocol-core/ble_broadcast_timing.c \
 *     model/engine-core/ble_discovery_engine.c \
 *     -o test-phase3-engine-c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "ble_discovery_engine.h"
#include "ble_discovery_packet.h"
#include "ble_message_queue.h"
#include "ble_mesh_node.h"

#define MAX_TEST_NODES 4U
#define SIM_SLOT_MS 50U
#define SIM_TOTAL_STEPS 240U
#define TEST_DEFAULT_RSSI -45

#define TEST_ASSERT(cond, msg)            \
    do {                                  \
        if (!(cond)) {                    \
            printf("FAIL: %s (line %d)\n", msg, __LINE__); \
            exit(1);                      \
        }                                 \
    } while (0)

static uint32_t g_now_ms = 0;

struct test_sim;

typedef struct test_node {
    ble_engine_t engine;
    ble_engine_config_t config;
    struct test_sim *sim;
    uint32_t node_id;
    uint32_t tx_count;
} test_node_t;

typedef struct test_sim {
    test_node_t nodes[MAX_TEST_NODES];
    uint32_t count;
} test_sim_t;

static void test_deliver_packet(test_node_t *sender,
                                const ble_discovery_packet_t *packet);

static void
ble_test_send_cb(const ble_discovery_packet_t *packet, void *user_context)
{
    if (!packet || !user_context) {
        return;
    }
    test_node_t *node = (test_node_t *)user_context;
    node->tx_count++;
    test_deliver_packet(node, packet);
}

static void
ble_test_log_cb(const char *level, const char *message, void *user_context)
{
    (void)level;
    (void)user_context;
    if (message) {
        printf("[ENGINE] %s\n", message);
    }
}

static void
ble_test_metrics_cb(const ble_connectivity_metrics_t *metrics, void *user_context)
{
    (void)metrics;
    (void)user_context;
    /* Metrics hook present to ensure callback wiring works. */
}

static void
init_test_node(test_sim_t *sim, uint32_t index, uint32_t node_id)
{
    test_node_t *node = &sim->nodes[index];
    node->node_id = node_id;
    node->sim = sim;
    node->tx_count = 0;

    ble_engine_config_init(&node->config);
    node->config.node_id = node_id;
    node->config.slot_duration_ms = SIM_SLOT_MS;
    node->config.initial_ttl = 5;
    node->config.proximity_threshold = 5.0;
    node->config.send_cb = ble_test_send_cb;
    node->config.log_cb = ble_test_log_cb;
    node->config.metrics_cb = ble_test_metrics_cb;
    node->config.user_context = node;

    TEST_ASSERT(ble_engine_init(&node->engine, &node->config), "Engine init should succeed");
}

static void
add_direct_neighbors(test_node_t *node, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        uint32_t neighbor_id = 1000U + i;
        ble_mesh_node_add_neighbor(&node->engine.node, neighbor_id, -40, 1);
    }
    ble_mesh_node_set_noise_level(&node->engine.node, 0.05);
}

static void
advance_all_nodes(test_sim_t *sim)
{
    g_now_ms += SIM_SLOT_MS;
    for (uint32_t i = 0; i < sim->count; i++) {
        ble_engine_tick(&sim->nodes[i].engine, g_now_ms);
    }
}

static void
inject_queue_bursts(test_node_t *node)
{
    ble_discovery_packet_t packet;
    for (uint32_t i = 0; i < BLE_QUEUE_MAX_SIZE + 8; i++) {
        ble_discovery_packet_init(&packet);
        packet.sender_id = 2000U + (i % 2); /* trigger duplicate guard */
        packet.ttl = 2;
        ble_engine_receive_packet(&node->engine, &packet, -60, g_now_ms);
    }
    uint32_t size = ble_queue_get_size(&node->engine.forward_queue);
    TEST_ASSERT(size <= BLE_QUEUE_MAX_SIZE, "Queue should never exceed max size");
}

static void
copy_and_deliver(test_node_t *target, const ble_discovery_packet_t *packet)
{
    if (packet->message_type == BLE_MSG_ELECTION_ANNOUNCEMENT) {
        ble_election_packet_t cloned;
        memcpy(&cloned, packet, sizeof(ble_election_packet_t));
        ble_engine_receive_packet(&target->engine,
                                  (const ble_discovery_packet_t *)&cloned,
                                  TEST_DEFAULT_RSSI,
                                  g_now_ms);
    } else {
        ble_discovery_packet_t cloned;
        memcpy(&cloned, packet, sizeof(ble_discovery_packet_t));
        ble_engine_receive_packet(&target->engine,
                                  &cloned,
                                  TEST_DEFAULT_RSSI,
                                  g_now_ms);
    }
}

static void
test_deliver_packet(test_node_t *sender, const ble_discovery_packet_t *packet)
{
    test_sim_t *sim = sender->sim;
    for (uint32_t i = 0; i < sim->count; i++) {
        test_node_t *target = &sim->nodes[i];
        if (target == sender) {
            continue;
        }
        copy_and_deliver(target, packet);
    }
}

static void
verify_clusterhead_alignment(test_sim_t *sim, uint32_t clusterhead_id)
{
    for (uint32_t i = 0; i < sim->count; i++) {
        ble_mesh_node_t *node = &sim->nodes[i].engine.node;
        if (node->node_id == clusterhead_id) {
            TEST_ASSERT(node->state == BLE_NODE_STATE_CLUSTERHEAD,
                        "Clusterhead should be in CLUSTERHEAD state");
        } else {
            TEST_ASSERT(node->clusterhead_id == clusterhead_id,
                        "Edge nodes should align to clusterhead");
        }
    }
}

static void
run_phase3_engine_test(void)
{
    printf("Running Phase 3 engine integration test...\n");
    test_sim_t sim;
    memset(&sim, 0, sizeof(sim));
    sim.count = 3;
    for (uint32_t i = 0; i < sim.count; i++) {
        init_test_node(&sim, i, i + 1);
    }

    add_direct_neighbors(&sim.nodes[0], 10); /* Node 1 will become clusterhead */

    for (uint32_t step = 0; step < SIM_TOTAL_STEPS; step++) {
        advance_all_nodes(&sim);
    }

    ble_mesh_node_t *node1 = &sim.nodes[0].engine.node;
    TEST_ASSERT(node1->state == BLE_NODE_STATE_CLUSTERHEAD,
                "Node 1 should become a clusterhead after election rounds");

    verify_clusterhead_alignment(&sim, node1->node_id);

    inject_queue_bursts(&sim.nodes[1]);

    printf("  -> Phase 3 integration test passed.\n");
}

int
main(void)
{
    run_phase3_engine_test();
    printf("All Phase 3 engine tests passed.\n");
    return 0;
}
