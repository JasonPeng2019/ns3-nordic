/**
 * @file ble-mesh-node-c-test.c
 * @brief Standalone C tests for BLE Mesh Node state machine
 * @author Benjamin Huh
 * @date 2025-11-21
 *
 * Pure C test suite that can run without NS-3
 * Tests the protocol-core/ble_mesh_node.c implementation
 */

#include "../model/protocol-core/ble_mesh_node.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Test counter */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
        } else { \
            tests_failed++; \
            printf("FAIL: %s (line %d): %s\n", __func__, __LINE__, message); \
        } \
    } while(0)

/* ===== Test: Node Initialization ===== */

void test_node_init(void)
{
    printf("Running test_node_init...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 42);

    TEST_ASSERT(node.node_id == 42, "Node ID should be 42");
    TEST_ASSERT(node.state == BLE_NODE_STATE_INIT, "Initial state should be INIT");
    TEST_ASSERT(node.prev_state == BLE_NODE_STATE_INIT, "Previous state should be INIT");
    TEST_ASSERT(node.state_entry_cycle == 0, "State entry cycle should be 0");
    TEST_ASSERT(node.gps_available == false, "GPS should be unavailable initially");
    TEST_ASSERT(node.neighbors.count == 0, "Neighbor count should be 0");
    TEST_ASSERT(node.clusterhead_id == BLE_MESH_INVALID_NODE_ID, "Clusterhead ID should be invalid");
    TEST_ASSERT(node.cluster_class == 0, "Cluster class should be 0");
    TEST_ASSERT(node.pdsf == 0, "PDSF should be 0");
    TEST_ASSERT(node.candidacy_score == 0.0, "Candidacy score should be 0.0");
    TEST_ASSERT(node.current_cycle == 0, "Current cycle should be 0");
    TEST_ASSERT(node.stats.messages_sent == 0, "Messages sent should be 0");
    TEST_ASSERT(node.stats.messages_received == 0, "Messages received should be 0");
    TEST_ASSERT(node.election_hash != 0, "Election hash should be generated");
}

/* ===== Test: GPS Management ===== */

void test_gps_operations(void)
{
    printf("Running test_gps_operations...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 1);

    // Initially GPS unavailable
    TEST_ASSERT(node.gps_available == false, "GPS initially unavailable");

    // Set GPS location
    ble_mesh_node_set_gps(&node, 10.5, 20.3, 5.0);
    TEST_ASSERT(node.gps_available == true, "GPS should be available after setting");
    TEST_ASSERT(node.gps_location.x == 10.5, "GPS X should be 10.5");
    TEST_ASSERT(node.gps_location.y == 20.3, "GPS Y should be 20.3");
    TEST_ASSERT(node.gps_location.z == 5.0, "GPS Z should be 5.0");

    // Clear GPS
    ble_mesh_node_clear_gps(&node);
    TEST_ASSERT(node.gps_available == false, "GPS should be unavailable after clearing");
}

/* ===== Test: State Transitions ===== */

void test_valid_state_transitions(void)
{
    printf("Running test_valid_state_transitions...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 10);

    // INIT -> DISCOVERY
    bool result = ble_mesh_node_set_state(&node, BLE_NODE_STATE_DISCOVERY);
    TEST_ASSERT(result == true, "INIT -> DISCOVERY should succeed");
    TEST_ASSERT(node.state == BLE_NODE_STATE_DISCOVERY, "State should be DISCOVERY");
    TEST_ASSERT(node.prev_state == BLE_NODE_STATE_INIT, "Previous state should be INIT");

    // DISCOVERY -> EDGE
    result = ble_mesh_node_set_state(&node, BLE_NODE_STATE_EDGE);
    TEST_ASSERT(result == true, "DISCOVERY -> EDGE should succeed");
    TEST_ASSERT(node.state == BLE_NODE_STATE_EDGE, "State should be EDGE");
    TEST_ASSERT(node.prev_state == BLE_NODE_STATE_DISCOVERY, "Previous state should be DISCOVERY");

    // EDGE -> CLUSTERHEAD_CANDIDATE
    result = ble_mesh_node_set_state(&node, BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE);
    TEST_ASSERT(result == true, "EDGE -> CLUSTERHEAD_CANDIDATE should succeed");
    TEST_ASSERT(node.state == BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE, "State should be CLUSTERHEAD_CANDIDATE");

    // CLUSTERHEAD_CANDIDATE -> CLUSTERHEAD
    result = ble_mesh_node_set_state(&node, BLE_NODE_STATE_CLUSTERHEAD);
    TEST_ASSERT(result == true, "CLUSTERHEAD_CANDIDATE -> CLUSTERHEAD should succeed");
    TEST_ASSERT(node.state == BLE_NODE_STATE_CLUSTERHEAD, "State should be CLUSTERHEAD");
}

void test_invalid_state_transitions(void)
{
    printf("Running test_invalid_state_transitions...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 11);

    // INIT -> EDGE (invalid, must go through DISCOVERY)
    bool result = ble_mesh_node_set_state(&node, BLE_NODE_STATE_EDGE);
    TEST_ASSERT(result == false, "INIT -> EDGE should fail");
    TEST_ASSERT(node.state == BLE_NODE_STATE_INIT, "State should remain INIT");

    // INIT -> CLUSTERHEAD (invalid)
    result = ble_mesh_node_set_state(&node, BLE_NODE_STATE_CLUSTERHEAD);
    TEST_ASSERT(result == false, "INIT -> CLUSTERHEAD should fail");

    // Go to DISCOVERY first
    ble_mesh_node_set_state(&node, BLE_NODE_STATE_DISCOVERY);

    // DISCOVERY -> CLUSTERHEAD (invalid, must be candidate first)
    result = ble_mesh_node_set_state(&node, BLE_NODE_STATE_CLUSTERHEAD);
    TEST_ASSERT(result == false, "DISCOVERY -> CLUSTERHEAD should fail");
    TEST_ASSERT(node.state == BLE_NODE_STATE_DISCOVERY, "State should remain DISCOVERY");
}

void test_state_names(void)
{
    printf("Running test_state_names...\n");

    const char* name;

    name = ble_mesh_node_state_name(BLE_NODE_STATE_INIT);
    TEST_ASSERT(strcmp(name, "INIT") == 0, "INIT state name");

    name = ble_mesh_node_state_name(BLE_NODE_STATE_DISCOVERY);
    TEST_ASSERT(strcmp(name, "DISCOVERY") == 0, "DISCOVERY state name");

    name = ble_mesh_node_state_name(BLE_NODE_STATE_EDGE);
    TEST_ASSERT(strcmp(name, "EDGE") == 0, "EDGE state name");

    name = ble_mesh_node_state_name(BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE);
    TEST_ASSERT(strcmp(name, "CLUSTERHEAD_CANDIDATE") == 0, "CLUSTERHEAD_CANDIDATE state name");

    name = ble_mesh_node_state_name(BLE_NODE_STATE_CLUSTERHEAD);
    TEST_ASSERT(strcmp(name, "CLUSTERHEAD") == 0, "CLUSTERHEAD state name");

    name = ble_mesh_node_state_name(BLE_NODE_STATE_CLUSTER_MEMBER);
    TEST_ASSERT(strcmp(name, "CLUSTER_MEMBER") == 0, "CLUSTER_MEMBER state name");
}

/* ===== Test: Cycle Management ===== */

void test_cycle_advance(void)
{
    printf("Running test_cycle_advance...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 20);

    TEST_ASSERT(node.current_cycle == 0, "Initial cycle should be 0");
    TEST_ASSERT(node.stats.discovery_cycles == 0, "Initial discovery cycles should be 0");

    ble_mesh_node_advance_cycle(&node);
    TEST_ASSERT(node.current_cycle == 1, "Cycle should be 1 after advance");
    TEST_ASSERT(node.stats.discovery_cycles == 1, "Discovery cycles should be 1");

    ble_mesh_node_advance_cycle(&node);
    TEST_ASSERT(node.current_cycle == 2, "Cycle should be 2 after second advance");
    TEST_ASSERT(node.stats.discovery_cycles == 2, "Discovery cycles should be 2");
}

/* ===== Test: Neighbor Management ===== */

void test_add_neighbor(void)
{
    printf("Running test_add_neighbor...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 30);

    // Add first neighbor
    bool result = ble_mesh_node_add_neighbor(&node, 100, -50, 1);
    TEST_ASSERT(result == true, "Adding first neighbor should succeed");
    TEST_ASSERT(node.neighbors.count == 1, "Neighbor count should be 1");

    // Verify neighbor details
    ble_neighbor_info_t* neighbor = ble_mesh_node_find_neighbor(&node, 100);
    TEST_ASSERT(neighbor != NULL, "Should find neighbor 100");
    TEST_ASSERT(neighbor->node_id == 100, "Neighbor ID should be 100");
    TEST_ASSERT(neighbor->rssi == -50, "Neighbor RSSI should be -50");
    TEST_ASSERT(neighbor->hop_count == 1, "Neighbor hop count should be 1");
    TEST_ASSERT(neighbor->last_seen_cycle == 0, "Last seen cycle should be 0");

    // Add second neighbor
    result = ble_mesh_node_add_neighbor(&node, 200, -60, 2);
    TEST_ASSERT(result == true, "Adding second neighbor should succeed");
    TEST_ASSERT(node.neighbors.count == 2, "Neighbor count should be 2");
}

void test_update_existing_neighbor(void)
{
    printf("Running test_update_existing_neighbor...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 31);

    // Add neighbor
    ble_mesh_node_add_neighbor(&node, 100, -50, 1);
    ble_mesh_node_advance_cycle(&node);

    // Update same neighbor with new RSSI
    bool result = ble_mesh_node_add_neighbor(&node, 100, -45, 1);
    TEST_ASSERT(result == true, "Updating existing neighbor should succeed");
    TEST_ASSERT(node.neighbors.count == 1, "Neighbor count should still be 1");

    ble_neighbor_info_t* neighbor = ble_mesh_node_find_neighbor(&node, 100);
    TEST_ASSERT(neighbor->rssi == -45, "RSSI should be updated to -45");
    TEST_ASSERT(neighbor->last_seen_cycle == 1, "Last seen cycle should be updated");
}

void test_neighbor_gps_update(void)
{
    printf("Running test_neighbor_gps_update...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 32);

    // Add neighbor
    ble_mesh_node_add_neighbor(&node, 100, -50, 1);

    // Update neighbor GPS
    ble_gps_location_t gps = {15.0, 25.0, 3.0};
    bool result = ble_mesh_node_update_neighbor_gps(&node, 100, &gps);
    TEST_ASSERT(result == true, "Updating neighbor GPS should succeed");

    ble_neighbor_info_t* neighbor = ble_mesh_node_find_neighbor(&node, 100);
    TEST_ASSERT(neighbor->gps_valid == true, "GPS should be valid");
    TEST_ASSERT(neighbor->gps.x == 15.0, "GPS X should be 15.0");
    TEST_ASSERT(neighbor->gps.y == 25.0, "GPS Y should be 25.0");
    TEST_ASSERT(neighbor->gps.z == 3.0, "GPS Z should be 3.0");

    // Try updating non-existent neighbor
    result = ble_mesh_node_update_neighbor_gps(&node, 999, &gps);
    TEST_ASSERT(result == false, "Updating non-existent neighbor GPS should fail");
}

void test_neighbor_counts(void)
{
    printf("Running test_neighbor_counts...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 33);

    // Add direct neighbors (hop_count = 1)
    ble_mesh_node_add_neighbor(&node, 100, -50, 1);
    ble_mesh_node_add_neighbor(&node, 101, -55, 1);
    ble_mesh_node_add_neighbor(&node, 102, -60, 1);

    // Add indirect neighbors (hop_count = 2)
    ble_mesh_node_add_neighbor(&node, 200, -70, 2);
    ble_mesh_node_add_neighbor(&node, 201, -75, 2);

    TEST_ASSERT(node.neighbors.count == 5, "Total neighbor count should be 5");

    uint16_t direct_count = ble_mesh_node_count_direct_neighbors(&node);
    TEST_ASSERT(direct_count == 3, "Direct neighbor count should be 3");
}

void test_average_rssi(void)
{
    printf("Running test_average_rssi...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 34);

    // Add neighbors with known RSSI values
    ble_mesh_node_add_neighbor(&node, 100, -40, 1);
    ble_mesh_node_add_neighbor(&node, 101, -50, 1);
    ble_mesh_node_add_neighbor(&node, 102, -60, 1);

    // Average = (-40 + -50 + -60) / 3 = -50
    int8_t avg = ble_mesh_node_calculate_avg_rssi(&node);
    TEST_ASSERT(avg == -50, "Average RSSI should be -50");
}

void test_prune_stale_neighbors(void)
{
    printf("Running test_prune_stale_neighbors...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 35);

    // Add neighbors at cycle 0
    ble_mesh_node_add_neighbor(&node, 100, -50, 1);
    ble_mesh_node_add_neighbor(&node, 101, -55, 1);
    ble_mesh_node_add_neighbor(&node, 102, -60, 1);

    // Advance to cycle 5
    for (int i = 0; i < 5; i++) {
        ble_mesh_node_advance_cycle(&node);
    }

    // Update neighbor 100 at cycle 5
    ble_mesh_node_add_neighbor(&node, 100, -50, 1);

    // Advance to cycle 10
    for (int i = 0; i < 5; i++) {
        ble_mesh_node_advance_cycle(&node);
    }

    // Prune neighbors older than 8 cycles
    // Neighbor 100: age = 10 - 5 = 5 (keep)
    // Neighbor 101: age = 10 - 0 = 10 (remove)
    // Neighbor 102: age = 10 - 0 = 10 (remove)
    uint16_t removed = ble_mesh_node_prune_stale_neighbors(&node, 8);
    TEST_ASSERT(removed == 2, "Should remove 2 stale neighbors");
    TEST_ASSERT(node.neighbors.count == 1, "Should have 1 neighbor remaining");

    ble_neighbor_info_t* neighbor = ble_mesh_node_find_neighbor(&node, 100);
    TEST_ASSERT(neighbor != NULL, "Neighbor 100 should still exist");
}

/* ===== Test: Election Decision Logic ===== */

void test_should_become_edge(void)
{
    printf("Running test_should_become_edge...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 40);

    // Node with very few direct neighbors should become edge
    ble_mesh_node_add_neighbor(&node, 100, -50, 1);
    ble_mesh_node_add_neighbor(&node, 101, -55, 1);

    bool result = ble_mesh_node_should_become_edge(&node);
    TEST_ASSERT(result == true, "Node with 2 direct neighbors should become edge");

    // Add more neighbors
    ble_mesh_node_add_neighbor(&node, 102, -60, 1);
    ble_mesh_node_add_neighbor(&node, 103, -65, 1);

    result = ble_mesh_node_should_become_edge(&node);
    TEST_ASSERT(result == false, "Node with 4 direct neighbors should not become edge");
}

void test_should_become_candidate(void)
{
    printf("Running test_should_become_candidate...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 50);
    ble_mesh_node_set_noise_level(&node, 0.0);

    // Node with few neighbors should not be candidate
    ble_mesh_node_add_neighbor(&node, 100, -50, 1);
    ble_mesh_node_add_neighbor(&node, 101, -55, 1);

    bool result = ble_mesh_node_should_become_candidate(&node);
    TEST_ASSERT(result == false, "Node with 2 direct neighbors should not be candidate");

    // Add enough neighbors with good RSSI
    ble_mesh_node_add_neighbor(&node, 102, -55, 1);
    ble_mesh_node_add_neighbor(&node, 103, -60, 1);
    ble_mesh_node_add_neighbor(&node, 104, -60, 1);

    // Initial cycle (n=6) should still fail
    result = ble_mesh_node_should_become_candidate(&node);
    TEST_ASSERT(result == false, "High threshold should block candidacy initially");

    // After one cycle with no candidates heard (n=3)
    node.current_cycle = 1;
    result = ble_mesh_node_should_become_candidate(&node);
    TEST_ASSERT(result == false, "Threshold of 3 should still block candidacy");

    // After two silent cycles (n=1) candidacy should pass
    node.current_cycle = 2;
    result = ble_mesh_node_should_become_candidate(&node);
    TEST_ASSERT(result == true, "Node should become candidate once requirement drops to 1");

    // Hearing another candidate should reset the threshold
    ble_mesh_node_mark_candidate_heard(&node);
    result = ble_mesh_node_should_become_candidate(&node);
    TEST_ASSERT(result == false, "Hearing another candidate should reset requirement");

    // Noise reduces the ratio further
    node.current_cycle = 4;
    ble_mesh_node_set_noise_level(&node, 50.0);
    result = ble_mesh_node_should_become_candidate(&node);
    TEST_ASSERT(result == false, "High noise level should block candidacy even when requirement is minimal");

    ble_mesh_node_set_noise_level(&node, 0.0);
    result = ble_mesh_node_should_become_candidate(&node);
    TEST_ASSERT(result == true, "Removing noise after cooldown should allow candidacy again");
}

void test_candidacy_score_calculation(void)
{
    printf("Running test_candidacy_score_calculation...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 60);

    // Add 5 direct neighbors
    for (int i = 0; i < 5; i++) {
        ble_mesh_node_add_neighbor(&node, 100 + i, -50, 1);
    }
    node.stats.messages_received = 10;
    node.stats.messages_forwarded = 6;

    double score = ble_mesh_node_calculate_candidacy_score(&node, 10.0);
    TEST_ASSERT(score > 0.0, "Candidacy score should be positive");
    TEST_ASSERT(score <= 1.0, "Score should be normalized to <= 1.0");

    // Higher connectivity should give higher score
    ble_mesh_node_t node2;
    ble_mesh_node_init(&node2, 61);
    for (int i = 0; i < 10; i++) {
        ble_mesh_node_add_neighbor(&node2, 100 + i, -50, 1);
    }
    node2.stats.messages_received = 10;
    node2.stats.messages_forwarded = 6;

    double score2 = ble_mesh_node_calculate_candidacy_score(&node2, 10.0);
    TEST_ASSERT(score2 > score, "More connected node should have higher score");
}

void test_candidacy_score_forwarding_bias(void)
{
    printf("Running test_candidacy_score_forwarding_bias...\n");

    ble_mesh_node_t reliableNode;
    ble_mesh_node_init(&reliableNode, 80);
    ble_mesh_node_t unreliableNode;
    ble_mesh_node_init(&unreliableNode, 81);

    for (int i = 0; i < 6; i++) {
        ble_mesh_node_add_neighbor(&reliableNode, 200 + i, -55, 1);
        ble_mesh_node_add_neighbor(&unreliableNode, 300 + i, -55, 1);
    }

    reliableNode.stats.messages_received = 20;
    reliableNode.stats.messages_forwarded = 18;
    unreliableNode.stats.messages_received = 20;
    unreliableNode.stats.messages_forwarded = 5;

    double reliableScore = ble_mesh_node_calculate_candidacy_score(&reliableNode, 5.0, 0.5);
    double unreliableScore = ble_mesh_node_calculate_candidacy_score(&unreliableNode, 5.0, 0.5);

    TEST_ASSERT(reliableScore > unreliableScore,
                "Higher forwarding success should improve candidacy score");
}

/* ===== Test: Statistics ===== */

void test_statistics_updates(void)
{
    printf("Running test_statistics_updates...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 70);

    // Add some neighbors
    ble_mesh_node_add_neighbor(&node, 100, -50, 1);
    ble_mesh_node_add_neighbor(&node, 101, -60, 1);

    // Update statistics
    ble_mesh_node_update_statistics(&node);

    TEST_ASSERT(node.stats.avg_rssi == -55, "Average RSSI should be -55");
    TEST_ASSERT(node.stats.direct_connections == 2, "Direct connections should be 2");
}

void test_message_counters(void)
{
    printf("Running test_message_counters...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 71);

    // Increment counters
    ble_mesh_node_inc_sent(&node);
    ble_mesh_node_inc_sent(&node);
    TEST_ASSERT(node.stats.messages_sent == 2, "Messages sent should be 2");

    ble_mesh_node_inc_received(&node);
    TEST_ASSERT(node.stats.messages_received == 1, "Messages received should be 1");

    ble_mesh_node_inc_forwarded(&node);
    ble_mesh_node_inc_forwarded(&node);
    ble_mesh_node_inc_forwarded(&node);
    TEST_ASSERT(node.stats.messages_forwarded == 3, "Messages forwarded should be 3");

    ble_mesh_node_inc_dropped(&node);
    TEST_ASSERT(node.stats.messages_dropped == 1, "Messages dropped should be 1");
}

/* ===== Test: Max Neighbors Limit ===== */

void test_max_neighbors_limit(void)
{
    printf("Running test_max_neighbors_limit...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 80);

    // Fill neighbor table to capacity
    for (int i = 0; i < BLE_MESH_MAX_NEIGHBORS; i++) {
        bool result = ble_mesh_node_add_neighbor(&node, 1000 + i, -50, 1);
        TEST_ASSERT(result == true, "Adding neighbor should succeed");
    }

    TEST_ASSERT(node.neighbors.count == BLE_MESH_MAX_NEIGHBORS,
                "Neighbor count should be at maximum");

    // Try to add one more (should fail)
    bool result = ble_mesh_node_add_neighbor(&node, 9999, -50, 1);
    TEST_ASSERT(result == false, "Adding neighbor beyond capacity should fail");
    TEST_ASSERT(node.neighbors.count == BLE_MESH_MAX_NEIGHBORS,
                "Neighbor count should remain at maximum");
}

/* ===== Main Test Runner ===== */

int main(void)
{
    printf("========================================\n");
    printf("BLE Mesh Node C Test Suite\n");
    printf("========================================\n\n");

    /* Run all tests */
    test_node_init();
    test_gps_operations();
    test_valid_state_transitions();
    test_invalid_state_transitions();
    test_state_names();
    test_cycle_advance();
    test_add_neighbor();
    test_update_existing_neighbor();
    test_neighbor_gps_update();
    test_neighbor_counts();
    test_average_rssi();
    test_prune_stale_neighbors();
    test_should_become_edge();
    test_should_become_candidate();
    test_candidacy_score_calculation();
    test_candidacy_score_forwarding_bias();
    test_statistics_updates();
    test_message_counters();
    test_max_neighbors_limit();

    /* Print results */
    printf("\n========================================\n");
    printf("Test Results:\n");
    printf("  PASSED: %d\n", tests_passed);
    printf("  FAILED: %d\n", tests_failed);
    printf("========================================\n");

    return (tests_failed == 0) ? 0 : 1;
}
