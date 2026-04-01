/**
 * @file ble-gps-cache-c-test.c
 * @brief Standalone C tests for GPS caching functionality
 * @author Benjamin Huh
 * @date 2025-11-21
 *
 * Pure C test suite for GPS location caching mechanism
 * Tests cache TTL, invalidation, and age tracking
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

/* ===== Test: GPS Cache TTL ===== */

void test_gps_cache_ttl_disabled(void)
{
    printf("Running test_gps_cache_ttl_disabled...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 1);

    // Default: TTL is 0 (disabled, cache never expires)
    TEST_ASSERT(node.gps_cache_ttl == 0, "Initial TTL should be 0");

    // Set GPS at cycle 0
    ble_mesh_node_set_gps(&node, 10.0, 20.0, 5.0);
    TEST_ASSERT(node.gps_last_update_cycle == 0, "GPS set at cycle 0");

    // Advance many cycles
    for (int i = 0; i < 100; i++) {
        ble_mesh_node_advance_cycle(&node);
    }

    // Cache should still be valid (TTL=0 means no expiration)
    bool valid = ble_mesh_node_is_gps_cache_valid(&node);
    TEST_ASSERT(valid == true, "Cache should be valid with TTL=0");

    uint32_t age = ble_mesh_node_get_gps_age(&node);
    TEST_ASSERT(age == 100, "GPS age should be 100 cycles");
}

void test_gps_cache_ttl_enabled(void)
{
    printf("Running test_gps_cache_ttl_enabled...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 2);

    // Set TTL to 10 cycles
    ble_mesh_node_set_gps_cache_ttl(&node, 10);
    TEST_ASSERT(node.gps_cache_ttl == 10, "TTL should be 10");

    // Set GPS at cycle 0
    ble_mesh_node_set_gps(&node, 15.0, 25.0, 3.0);

    // Advance 5 cycles - cache should be valid
    for (int i = 0; i < 5; i++) {
        ble_mesh_node_advance_cycle(&node);
    }

    bool valid = ble_mesh_node_is_gps_cache_valid(&node);
    TEST_ASSERT(valid == true, "Cache should be valid after 5 cycles");

    // Advance 5 more cycles (total 10) - cache should expire
    for (int i = 0; i < 5; i++) {
        ble_mesh_node_advance_cycle(&node);
    }

    valid = ble_mesh_node_is_gps_cache_valid(&node);
    TEST_ASSERT(valid == false, "Cache should be invalid after 10 cycles");

    uint32_t age = ble_mesh_node_get_gps_age(&node);
    TEST_ASSERT(age == 10, "GPS age should be 10 cycles");
}

void test_gps_cache_refresh(void)
{
    printf("Running test_gps_cache_refresh...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 3);

    // Set TTL to 5 cycles
    ble_mesh_node_set_gps_cache_ttl(&node, 5);

    // Set GPS at cycle 0
    ble_mesh_node_set_gps(&node, 10.0, 20.0, 5.0);

    // Advance 3 cycles
    for (int i = 0; i < 3; i++) {
        ble_mesh_node_advance_cycle(&node);
    }

    TEST_ASSERT(ble_mesh_node_is_gps_cache_valid(&node) == true, "Cache valid at cycle 3");

    // Refresh GPS at cycle 3
    ble_mesh_node_set_gps(&node, 11.0, 21.0, 6.0);
    TEST_ASSERT(node.gps_last_update_cycle == 3, "Update cycle should be 3");

    // Advance 4 more cycles (total 7)
    for (int i = 0; i < 4; i++) {
        ble_mesh_node_advance_cycle(&node);
    }

    // Cache should still be valid (refreshed at cycle 3, now at cycle 7, age=4 < 5)
    bool valid = ble_mesh_node_is_gps_cache_valid(&node);
    TEST_ASSERT(valid == true, "Cache should be valid after refresh");

    uint32_t age = ble_mesh_node_get_gps_age(&node);
    TEST_ASSERT(age == 4, "GPS age should be 4 after refresh");
}

void test_gps_cache_invalidation(void)
{
    printf("Running test_gps_cache_invalidation...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 4);

    // Set TTL to 20 cycles
    ble_mesh_node_set_gps_cache_ttl(&node, 20);

    // Set GPS at cycle 0
    ble_mesh_node_set_gps(&node, 5.0, 10.0, 2.0);

    // Advance 5 cycles
    for (int i = 0; i < 5; i++) {
        ble_mesh_node_advance_cycle(&node);
    }

    TEST_ASSERT(ble_mesh_node_is_gps_cache_valid(&node) == true, "Cache should be valid");

    // Manually invalidate cache
    ble_mesh_node_invalidate_gps_cache(&node);

    // Cache should now be invalid
    bool valid = ble_mesh_node_is_gps_cache_valid(&node);
    TEST_ASSERT(valid == false, "Cache should be invalid after invalidation");

    // GPS should be marked unavailable after invalidation
    TEST_ASSERT(node.gps_available == false, "GPS should be unavailable after invalidation");
}

void test_gps_unavailable_makes_cache_invalid(void)
{
    printf("Running test_gps_unavailable_makes_cache_invalid...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 5);

    // Set GPS
    ble_mesh_node_set_gps(&node, 10.0, 20.0, 5.0);
    TEST_ASSERT(ble_mesh_node_is_gps_cache_valid(&node) == true, "Cache should be valid");

    // Clear GPS (mark as unavailable)
    ble_mesh_node_clear_gps(&node);

    // Cache should be invalid when GPS is unavailable
    bool valid = ble_mesh_node_is_gps_cache_valid(&node);
    TEST_ASSERT(valid == false, "Cache should be invalid when GPS unavailable");
}

void test_gps_cache_boundary_conditions(void)
{
    printf("Running test_gps_cache_boundary_conditions...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 6);

    // Set TTL to 1 cycle (edge case)
    ble_mesh_node_set_gps_cache_ttl(&node, 1);
    ble_mesh_node_set_gps(&node, 1.0, 2.0, 3.0);

    TEST_ASSERT(ble_mesh_node_is_gps_cache_valid(&node) == true, "Cache valid at cycle 0");

    // Advance 1 cycle - should expire
    ble_mesh_node_advance_cycle(&node);

    bool valid = ble_mesh_node_is_gps_cache_valid(&node);
    TEST_ASSERT(valid == false, "Cache should expire after 1 cycle with TTL=1");
}

void test_gps_age_calculation(void)
{
    printf("Running test_gps_age_calculation...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 7);

    // Set GPS at cycle 0
    ble_mesh_node_set_gps(&node, 10.0, 20.0, 5.0);
    TEST_ASSERT(ble_mesh_node_get_gps_age(&node) == 0, "Age should be 0 initially");

    // Advance cycles and check age
    for (int i = 1; i <= 50; i++) {
        ble_mesh_node_advance_cycle(&node);
        uint32_t age = ble_mesh_node_get_gps_age(&node);
        TEST_ASSERT(age == (uint32_t)i, "Age should match cycle count");
    }
}

void test_multiple_gps_updates(void)
{
    printf("Running test_multiple_gps_updates...\n");

    ble_mesh_node_t node;
    ble_mesh_node_init(&node, 8);

    ble_mesh_node_set_gps_cache_ttl(&node, 10);

    // Update GPS multiple times
    ble_mesh_node_set_gps(&node, 1.0, 2.0, 3.0);
    TEST_ASSERT(node.gps_last_update_cycle == 0, "First update at cycle 0");

    ble_mesh_node_advance_cycle(&node);
    ble_mesh_node_set_gps(&node, 2.0, 3.0, 4.0);
    TEST_ASSERT(node.gps_last_update_cycle == 1, "Second update at cycle 1");

    ble_mesh_node_advance_cycle(&node);
    ble_mesh_node_set_gps(&node, 3.0, 4.0, 5.0);
    TEST_ASSERT(node.gps_last_update_cycle == 2, "Third update at cycle 2");

    // Age should be relative to most recent update
    uint32_t age = ble_mesh_node_get_gps_age(&node);
    TEST_ASSERT(age == 0, "Age should be 0 after immediate update");
}

/* ===== Main Test Runner ===== */

int main(void)
{
    printf("========================================\n");
    printf("BLE GPS Cache C Test Suite\n");
    printf("========================================\n\n");

    /* Run all tests */
    test_gps_cache_ttl_disabled();
    test_gps_cache_ttl_enabled();
    test_gps_cache_refresh();
    test_gps_cache_invalidation();
    test_gps_unavailable_makes_cache_invalid();
    test_gps_cache_boundary_conditions();
    test_gps_age_calculation();
    test_multiple_gps_updates();

    /* Print results */
    printf("\n========================================\n");
    printf("Test Results:\n");
    printf("  PASSED: %d\n", tests_passed);
    printf("  FAILED: %d\n", tests_failed);
    printf("========================================\n");

    return (tests_failed == 0) ? 0 : 1;
}
