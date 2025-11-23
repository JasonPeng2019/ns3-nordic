/**
 * @file ble-discovery-cycle-c-test.c
 * @brief Standalone C tests for BLE Discovery Cycle
 *
 * These tests can be compiled and run without NS-3, validating the
 * pure C implementation of the discovery cycle state machine.
 *
 * Compile with:
 *   gcc -I../model/protocol-core test/ble-discovery-cycle-c-test.c \
 *       model/protocol-core/ble_discovery_cycle.c -o test-discovery-cycle-c
 *
 * Run with:
 *   ./test-discovery-cycle-c
 *
 * Copyright (c) 2025
 * Author: jason peng <jason.p>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ble_discovery_cycle.h"


static int tests_passed = 0;
static int tests_failed = 0;


#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("FAIL: %s (line %d)\n", msg, __LINE__); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, msg) do { \
    if ((a) != (b)) { \
        printf("FAIL: %s - expected %d, got %d (line %d)\n", msg, (int)(b), (int)(a), __LINE__); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define TEST_ASSERT_STR_EQ(a, b, msg) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("FAIL: %s - expected '%s', got '%s' (line %d)\n", msg, (b), (a), __LINE__); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)


static int slot_callback_counts[BLE_DISCOVERY_NUM_SLOTS];
static int cycle_complete_count;
static uint32_t last_cycle_count;
static uint8_t last_slot_number;
static void *last_user_data;


static void reset_callback_counters(void)
{
    for (int i = 0; i < BLE_DISCOVERY_NUM_SLOTS; i++) {
        slot_callback_counts[i] = 0;
    }
    cycle_complete_count = 0;
    last_cycle_count = 0;
    last_slot_number = 255;
    last_user_data = NULL;
}


static void test_slot_callback(uint8_t slot_number, void *user_data)
{
    if (slot_number < BLE_DISCOVERY_NUM_SLOTS) {
        slot_callback_counts[slot_number]++;
    }
    last_slot_number = slot_number;
    last_user_data = user_data;
}

static void test_cycle_complete_callback(uint32_t cycle_count, void *user_data)
{
    cycle_complete_count++;
    last_cycle_count = cycle_count;
    last_user_data = user_data;
}


void test_init(void)
{
    printf("Test: Initialization...\n");

    ble_discovery_cycle_t cycle;
    ble_discovery_cycle_init(&cycle);

    TEST_ASSERT_EQ(cycle.running, false, "Running should be false");
    TEST_ASSERT_EQ(cycle.current_slot, 0, "Current slot should be 0");
    TEST_ASSERT_EQ(cycle.slot_duration_ms, BLE_DISCOVERY_DEFAULT_SLOT_DURATION_MS,
                   "Default slot duration should be 100ms");
    TEST_ASSERT_EQ(cycle.cycle_count, 0, "Cycle count should be 0");
    TEST_ASSERT(cycle.user_data == NULL, "User data should be NULL");
    TEST_ASSERT(cycle.cycle_complete_callback == NULL, "Cycle complete callback should be NULL");

    for (int i = 0; i < BLE_DISCOVERY_NUM_SLOTS; i++) {
        TEST_ASSERT(cycle.slot_callbacks[i] == NULL, "Slot callbacks should be NULL");
    }

    
    ble_discovery_cycle_init(NULL); 

    printf("  Passed!\n");
}


void test_slot_duration(void)
{
    printf("Test: Slot duration configuration...\n");

    ble_discovery_cycle_t cycle;
    ble_discovery_cycle_init(&cycle);

    
    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_duration(&cycle), 100,
                   "Default slot duration");

    
    TEST_ASSERT(ble_discovery_cycle_set_slot_duration(&cycle, 50),
                "Should be able to set duration when not running");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_duration(&cycle), 50,
                   "Slot duration should be 50");

    
    TEST_ASSERT_EQ(ble_discovery_cycle_get_cycle_duration(&cycle), 200,
                   "Cycle duration should be 4 * 50 = 200");

    
    ble_discovery_cycle_start(&cycle);
    TEST_ASSERT(!ble_discovery_cycle_set_slot_duration(&cycle, 75),
                "Should not be able to change duration while running");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_duration(&cycle), 50,
                   "Duration should still be 50");
    ble_discovery_cycle_stop(&cycle);

    
    TEST_ASSERT(ble_discovery_cycle_set_slot_duration(&cycle, 75),
                "Should be able to change duration after stopping");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_duration(&cycle), 75,
                   "Duration should be 75");

    
    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_duration(NULL), 0,
                   "NULL cycle should return 0");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_cycle_duration(NULL), 0,
                   "NULL cycle should return 0 for cycle duration");
    TEST_ASSERT(!ble_discovery_cycle_set_slot_duration(NULL, 100),
                "NULL cycle should return false");

    printf("  Passed!\n");
}


void test_start_stop(void)
{
    printf("Test: Start/Stop behavior...\n");

    ble_discovery_cycle_t cycle;
    ble_discovery_cycle_init(&cycle);

    
    TEST_ASSERT(!ble_discovery_cycle_is_running(&cycle), "Should not be running initially");

    
    TEST_ASSERT(ble_discovery_cycle_start(&cycle), "Start should succeed");
    TEST_ASSERT(ble_discovery_cycle_is_running(&cycle), "Should be running after start");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_current_slot(&cycle), 0,
                   "Current slot should be 0 after start");

    
    TEST_ASSERT(!ble_discovery_cycle_start(&cycle), "Double start should return false");
    TEST_ASSERT(ble_discovery_cycle_is_running(&cycle), "Should still be running");

    
    ble_discovery_cycle_stop(&cycle);
    TEST_ASSERT(!ble_discovery_cycle_is_running(&cycle), "Should not be running after stop");

    
    ble_discovery_cycle_stop(&cycle);
    TEST_ASSERT(!ble_discovery_cycle_is_running(&cycle), "Should still not be running");

    
    TEST_ASSERT(!ble_discovery_cycle_start(NULL), "NULL cycle start should return false");
    TEST_ASSERT(!ble_discovery_cycle_is_running(NULL), "NULL cycle is_running should return false");
    ble_discovery_cycle_stop(NULL); 

    printf("  Passed!\n");
}


void test_slot_validation(void)
{
    printf("Test: Slot validation...\n");

    
    TEST_ASSERT(ble_discovery_cycle_is_valid_slot(0), "Slot 0 should be valid");
    TEST_ASSERT(ble_discovery_cycle_is_valid_slot(1), "Slot 1 should be valid");
    TEST_ASSERT(ble_discovery_cycle_is_valid_slot(2), "Slot 2 should be valid");
    TEST_ASSERT(ble_discovery_cycle_is_valid_slot(3), "Slot 3 should be valid");
    TEST_ASSERT(!ble_discovery_cycle_is_valid_slot(4), "Slot 4 should be invalid");
    TEST_ASSERT(!ble_discovery_cycle_is_valid_slot(255), "Slot 255 should be invalid");

    
    TEST_ASSERT(!ble_discovery_cycle_is_forwarding_slot(0), "Slot 0 is not forwarding");
    TEST_ASSERT(ble_discovery_cycle_is_forwarding_slot(1), "Slot 1 is forwarding");
    TEST_ASSERT(ble_discovery_cycle_is_forwarding_slot(2), "Slot 2 is forwarding");
    TEST_ASSERT(ble_discovery_cycle_is_forwarding_slot(3), "Slot 3 is forwarding");
    TEST_ASSERT(!ble_discovery_cycle_is_forwarding_slot(4), "Slot 4 is not forwarding");

    
    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_type(0), BLE_SLOT_TYPE_OWN_MESSAGE,
                   "Slot 0 type should be OWN_MESSAGE");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_type(1), BLE_SLOT_TYPE_FORWARDING,
                   "Slot 1 type should be FORWARDING");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_type(2), BLE_SLOT_TYPE_FORWARDING,
                   "Slot 2 type should be FORWARDING");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_type(3), BLE_SLOT_TYPE_FORWARDING,
                   "Slot 3 type should be FORWARDING");

    printf("  Passed!\n");
}


void test_slot_offset(void)
{
    printf("Test: Slot offset calculation...\n");

    ble_discovery_cycle_t cycle;
    ble_discovery_cycle_init(&cycle);
    ble_discovery_cycle_set_slot_duration(&cycle, 100);

    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_offset(&cycle, 0), 0,
                   "Slot 0 offset should be 0");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_offset(&cycle, 1), 100,
                   "Slot 1 offset should be 100");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_offset(&cycle, 2), 200,
                   "Slot 2 offset should be 200");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_offset(&cycle, 3), 300,
                   "Slot 3 offset should be 300");

    
    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_offset(&cycle, 4), 0,
                   "Invalid slot should return 0");

    
    ble_discovery_cycle_set_slot_duration(&cycle, 50);
    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_offset(&cycle, 2), 100,
                   "Slot 2 offset with 50ms slots should be 100");

    
    TEST_ASSERT_EQ(ble_discovery_cycle_get_slot_offset(NULL, 0), 0,
                   "NULL cycle should return 0");

    printf("  Passed!\n");
}


void test_callback_registration(void)
{
    printf("Test: Callback registration...\n");

    ble_discovery_cycle_t cycle;
    ble_discovery_cycle_init(&cycle);

    
    TEST_ASSERT(ble_discovery_cycle_set_slot_callback(&cycle, 0, test_slot_callback),
                "Setting slot 0 callback should succeed");
    TEST_ASSERT(ble_discovery_cycle_set_slot_callback(&cycle, 1, test_slot_callback),
                "Setting slot 1 callback should succeed");
    TEST_ASSERT(ble_discovery_cycle_set_slot_callback(&cycle, 2, test_slot_callback),
                "Setting slot 2 callback should succeed");
    TEST_ASSERT(ble_discovery_cycle_set_slot_callback(&cycle, 3, test_slot_callback),
                "Setting slot 3 callback should succeed");

    
    TEST_ASSERT(!ble_discovery_cycle_set_slot_callback(&cycle, 4, test_slot_callback),
                "Setting slot 4 callback should fail");

    
    ble_discovery_cycle_set_complete_callback(&cycle, test_cycle_complete_callback);
    TEST_ASSERT(cycle.cycle_complete_callback == test_cycle_complete_callback,
                "Cycle complete callback should be set");

    
    int user_data = 42;
    ble_discovery_cycle_set_user_data(&cycle, &user_data);
    TEST_ASSERT(cycle.user_data == &user_data, "User data should be set");

    
    TEST_ASSERT(!ble_discovery_cycle_set_slot_callback(NULL, 0, test_slot_callback),
                "NULL cycle should return false");
    ble_discovery_cycle_set_complete_callback(NULL, test_cycle_complete_callback); 
    ble_discovery_cycle_set_user_data(NULL, &user_data); 

    printf("  Passed!\n");
}


void test_slot_execution(void)
{
    printf("Test: Slot execution...\n");

    reset_callback_counters();

    ble_discovery_cycle_t cycle;
    ble_discovery_cycle_init(&cycle);

    
    for (int i = 0; i < BLE_DISCOVERY_NUM_SLOTS; i++) {
        ble_discovery_cycle_set_slot_callback(&cycle, i, test_slot_callback);
    }

    int user_data = 12345;
    ble_discovery_cycle_set_user_data(&cycle, &user_data);

    
    ble_discovery_cycle_start(&cycle);

    
    cycle.current_slot = 0;
    ble_discovery_cycle_execute_slot(&cycle);
    TEST_ASSERT_EQ(slot_callback_counts[0], 1, "Slot 0 callback should be called once");
    TEST_ASSERT_EQ(last_slot_number, 0, "Last slot number should be 0");
    TEST_ASSERT(last_user_data == &user_data, "User data should be passed");

    
    cycle.current_slot = 1;
    ble_discovery_cycle_execute_slot(&cycle);
    TEST_ASSERT_EQ(slot_callback_counts[1], 1, "Slot 1 callback should be called once");

    
    cycle.current_slot = 2;
    ble_discovery_cycle_execute_slot(&cycle);
    TEST_ASSERT_EQ(slot_callback_counts[2], 1, "Slot 2 callback should be called once");

    
    cycle.current_slot = 3;
    ble_discovery_cycle_execute_slot(&cycle);
    TEST_ASSERT_EQ(slot_callback_counts[3], 1, "Slot 3 callback should be called once");

    
    ble_discovery_cycle_stop(&cycle);
    cycle.current_slot = 0;
    ble_discovery_cycle_execute_slot(&cycle);
    TEST_ASSERT_EQ(slot_callback_counts[0], 1, "Callback should not be called when stopped");

    
    ble_discovery_cycle_execute_slot(NULL); 

    printf("  Passed!\n");
}


void test_slot_advancement(void)
{
    printf("Test: Slot advancement and cycle completion...\n");

    reset_callback_counters();

    ble_discovery_cycle_t cycle;
    ble_discovery_cycle_init(&cycle);

    ble_discovery_cycle_set_complete_callback(&cycle, test_cycle_complete_callback);

    int user_data = 99;
    ble_discovery_cycle_set_user_data(&cycle, &user_data);

    ble_discovery_cycle_start(&cycle);

    
    TEST_ASSERT_EQ(ble_discovery_cycle_get_current_slot(&cycle), 0, "Should start at slot 0");

    uint8_t next_slot = ble_discovery_cycle_advance_slot(&cycle);
    TEST_ASSERT_EQ(next_slot, 1, "Next slot should be 1");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_current_slot(&cycle), 1, "Current slot should be 1");

    next_slot = ble_discovery_cycle_advance_slot(&cycle);
    TEST_ASSERT_EQ(next_slot, 2, "Next slot should be 2");

    next_slot = ble_discovery_cycle_advance_slot(&cycle);
    TEST_ASSERT_EQ(next_slot, 3, "Next slot should be 3");

    
    next_slot = ble_discovery_cycle_advance_slot(&cycle);
    TEST_ASSERT_EQ(next_slot, 0, "Should wrap to slot 0");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_current_slot(&cycle), 0, "Current slot should be 0");
    TEST_ASSERT_EQ(cycle_complete_count, 1, "Cycle complete callback should be called once");
    TEST_ASSERT_EQ(last_cycle_count, 1, "Cycle count should be 1");
    TEST_ASSERT(last_user_data == &user_data, "User data should be passed to callback");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_cycle_count(&cycle), 1, "Cycle count should be 1");

    
    for (int i = 0; i < 4; i++) {
        ble_discovery_cycle_advance_slot(&cycle);
    }
    TEST_ASSERT_EQ(cycle_complete_count, 2, "Cycle complete callback should be called twice");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_cycle_count(&cycle), 2, "Cycle count should be 2");

    
    ble_discovery_cycle_stop(&cycle);
    next_slot = ble_discovery_cycle_advance_slot(&cycle);
    TEST_ASSERT_EQ(next_slot, 0, "Should return 0 when not running");

    
    TEST_ASSERT_EQ(ble_discovery_cycle_advance_slot(NULL), 0, "NULL cycle should return 0");

    printf("  Passed!\n");
}


void test_cycle_count_reset(void)
{
    printf("Test: Cycle count reset...\n");

    ble_discovery_cycle_t cycle;
    ble_discovery_cycle_init(&cycle);

    ble_discovery_cycle_start(&cycle);

    
    for (int c = 0; c < 5; c++) {
        for (int s = 0; s < 4; s++) {
            ble_discovery_cycle_advance_slot(&cycle);
        }
    }

    TEST_ASSERT_EQ(ble_discovery_cycle_get_cycle_count(&cycle), 5, "Should have 5 cycles");

    
    ble_discovery_cycle_reset_count(&cycle);
    TEST_ASSERT_EQ(ble_discovery_cycle_get_cycle_count(&cycle), 0, "Cycle count should be 0");

    
    TEST_ASSERT_EQ(ble_discovery_cycle_get_cycle_count(NULL), 0, "NULL cycle should return 0");
    ble_discovery_cycle_reset_count(NULL); 

    printf("  Passed!\n");
}


void test_slot_names(void)
{
    printf("Test: Slot names...\n");

    TEST_ASSERT_STR_EQ(ble_discovery_cycle_slot_name(0), "Slot 0 (Own Message)",
                       "Slot 0 name");
    TEST_ASSERT_STR_EQ(ble_discovery_cycle_slot_name(1), "Slot 1 (Forward 1)",
                       "Slot 1 name");
    TEST_ASSERT_STR_EQ(ble_discovery_cycle_slot_name(2), "Slot 2 (Forward 2)",
                       "Slot 2 name");
    TEST_ASSERT_STR_EQ(ble_discovery_cycle_slot_name(3), "Slot 3 (Forward 3)",
                       "Slot 3 name");
    TEST_ASSERT_STR_EQ(ble_discovery_cycle_slot_name(4), "Invalid Slot",
                       "Invalid slot name");

    TEST_ASSERT_STR_EQ(ble_discovery_cycle_slot_type_name(BLE_SLOT_TYPE_OWN_MESSAGE),
                       "OWN_MESSAGE", "Own message type name");
    TEST_ASSERT_STR_EQ(ble_discovery_cycle_slot_type_name(BLE_SLOT_TYPE_FORWARDING),
                       "FORWARDING", "Forwarding type name");

    printf("  Passed!\n");
}


void test_full_cycle_simulation(void)
{
    printf("Test: Full cycle simulation...\n");

    reset_callback_counters();

    ble_discovery_cycle_t cycle;
    ble_discovery_cycle_init(&cycle);

    
    for (int i = 0; i < BLE_DISCOVERY_NUM_SLOTS; i++) {
        ble_discovery_cycle_set_slot_callback(&cycle, i, test_slot_callback);
    }
    ble_discovery_cycle_set_complete_callback(&cycle, test_cycle_complete_callback);

    ble_discovery_cycle_start(&cycle);

    
    for (int c = 0; c < 3; c++) {
        for (int s = 0; s < 4; s++) {
            ble_discovery_cycle_execute_slot(&cycle);
            ble_discovery_cycle_advance_slot(&cycle);
        }
    }

    
    TEST_ASSERT_EQ(slot_callback_counts[0], 3, "Slot 0 should be called 3 times");
    TEST_ASSERT_EQ(slot_callback_counts[1], 3, "Slot 1 should be called 3 times");
    TEST_ASSERT_EQ(slot_callback_counts[2], 3, "Slot 2 should be called 3 times");
    TEST_ASSERT_EQ(slot_callback_counts[3], 3, "Slot 3 should be called 3 times");
    TEST_ASSERT_EQ(cycle_complete_count, 3, "Cycle complete should be called 3 times");
    TEST_ASSERT_EQ(ble_discovery_cycle_get_cycle_count(&cycle), 3, "Cycle count should be 3");

    printf("  Passed!\n");
}


void test_null_callbacks(void)
{
    printf("Test: Null callback handling...\n");

    reset_callback_counters();

    ble_discovery_cycle_t cycle;
    ble_discovery_cycle_init(&cycle);

    
    ble_discovery_cycle_set_slot_callback(&cycle, 1, test_slot_callback);

    ble_discovery_cycle_start(&cycle);

    
    for (int s = 0; s < 4; s++) {
        cycle.current_slot = s;
        ble_discovery_cycle_execute_slot(&cycle);
    }

    
    TEST_ASSERT_EQ(slot_callback_counts[0], 0, "Slot 0 callback was not set");
    TEST_ASSERT_EQ(slot_callback_counts[1], 1, "Slot 1 callback should be called");
    TEST_ASSERT_EQ(slot_callback_counts[2], 0, "Slot 2 callback was not set");
    TEST_ASSERT_EQ(slot_callback_counts[3], 0, "Slot 3 callback was not set");

    printf("  Passed!\n");
}


int main(int argc, char *argv[])
{
    printf("===========================================\n");
    printf("BLE Discovery Cycle - Pure C Unit Tests\n");
    printf("===========================================\n\n");

    test_init();
    test_slot_duration();
    test_start_stop();
    test_slot_validation();
    test_slot_offset();
    test_callback_registration();
    test_slot_execution();
    test_slot_advancement();
    test_cycle_count_reset();
    test_slot_names();
    test_full_cycle_simulation();
    test_null_callbacks();

    printf("\n===========================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("===========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
