/**
 * @file ble-broadcast-timing-c-test.c
 * @brief Standalone C tests for broadcast timing (Tasks 12 & 14)
 *
 * Tests can be compiled and run without NS-3 for embedded systems validation.
 */

#include "../model/protocol-core/ble_broadcast_timing.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>


static int test_count = 0;
static int test_passed = 0;

#define TEST_START(name) \
    do { \
        test_count++; \
        printf("Test %d: %s ... ", test_count, name); \
    } while (0)

#define TEST_PASS() \
    do { \
        test_passed++; \
        printf("PASS\n"); \
    } while (0)


void test_init(void)
{
    TEST_START("Initialization");

    ble_broadcast_timing_t state;
    ble_broadcast_timing_init(&state, BLE_BROADCAST_SCHEDULE_NOISY, 10, 100, 0.8);

    assert(state.schedule_type == BLE_BROADCAST_SCHEDULE_NOISY);
    assert(state.num_slots == 10);
    assert(state.slot_duration_ms == 100);
    assert(fabs(state.listen_ratio - 0.8) < 0.001);
    assert(state.current_slot == 0);
    assert(state.is_broadcast_slot == false);
    assert(state.max_retries == BLE_BROADCAST_MAX_RETRIES);

    TEST_PASS();
}


void test_advance_slot(void)
{
    TEST_START("Slot advancement");

    ble_broadcast_timing_t state;
    ble_broadcast_timing_init(&state, BLE_BROADCAST_SCHEDULE_NOISY, 5, 100, 0.8);

    uint32_t prev_slot = state.current_slot;
    ble_broadcast_timing_advance_slot(&state);

    assert(state.current_slot == (prev_slot + 1) % 5);

    
    for (int i = 0; i < 10; i++) {
        ble_broadcast_timing_advance_slot(&state);
    }

    assert(state.current_slot < 5);

    TEST_PASS();
}


void test_listen_ratio(void)
{
    TEST_START("Listen ratio distribution");

    ble_broadcast_timing_t state;
    ble_broadcast_timing_init(&state, BLE_BROADCAST_SCHEDULE_STOCHASTIC, 10, 100, 0.8);
    ble_broadcast_timing_set_seed(&state, 12345);

    
    int num_trials = 1000;
    int listen_count = 0;
    int broadcast_count = 0;

    for (int i = 0; i < num_trials; i++) {
        bool is_broadcast = ble_broadcast_timing_advance_slot(&state);
        if (is_broadcast) {
            broadcast_count++;
        } else {
            listen_count++;
        }
    }

    double actual_listen_ratio = (double)listen_count / (double)num_trials;

    
    assert(actual_listen_ratio > 0.7 && actual_listen_ratio < 0.9);

    TEST_PASS();
}


void test_success_tracking(void)
{
    TEST_START("Success tracking");

    ble_broadcast_timing_t state;
    ble_broadcast_timing_init(&state, BLE_BROADCAST_SCHEDULE_NOISY, 10, 100, 0.5);

    assert(state.successful_broadcasts == 0);
    assert(state.failed_broadcasts == 0);

    ble_broadcast_timing_record_success(&state);
    assert(state.successful_broadcasts == 1);
    assert(state.retry_count == 0);

    ble_broadcast_timing_record_success(&state);
    assert(state.successful_broadcasts == 2);

    double success_rate = ble_broadcast_timing_get_success_rate(&state);
    assert(fabs(success_rate - 1.0) < 0.001);

    TEST_PASS();
}


void test_retry_logic(void)
{
    TEST_START("Retry logic");

    ble_broadcast_timing_t state;
    ble_broadcast_timing_init(&state, BLE_BROADCAST_SCHEDULE_STOCHASTIC, 10, 100, 0.8);

    assert(state.retry_count == 0);

    
    bool should_retry = ble_broadcast_timing_record_failure(&state);
    assert(should_retry == true);
    assert(state.retry_count == 1);
    assert(state.failed_broadcasts == 1);

    
    should_retry = ble_broadcast_timing_record_failure(&state);
    assert(should_retry == true);
    assert(state.retry_count == 2);

    
    should_retry = ble_broadcast_timing_record_failure(&state);
    assert(should_retry == false);  
    assert(state.retry_count == 0);  

    TEST_PASS();
}


void test_success_rate(void)
{
    TEST_START("Success rate calculation");

    ble_broadcast_timing_t state;
    ble_broadcast_timing_init(&state, BLE_BROADCAST_SCHEDULE_NOISY, 10, 100, 0.5);

    
    for (int i = 0; i < 7; i++) {
        ble_broadcast_timing_record_success(&state);
    }
    for (int i = 0; i < 3; i++) {
        ble_broadcast_timing_record_failure(&state);
    }

    double success_rate = ble_broadcast_timing_get_success_rate(&state);
    assert(fabs(success_rate - 0.7) < 0.001);

    TEST_PASS();
}


void test_actual_listen_ratio(void)
{
    TEST_START("Actual listen ratio");

    ble_broadcast_timing_t state;
    ble_broadcast_timing_init(&state, BLE_BROADCAST_SCHEDULE_STOCHASTIC, 10, 100, 0.8);
    ble_broadcast_timing_set_seed(&state, 42);

    
    state.total_listen_slots = 80;
    state.total_broadcast_slots = 20;

    double actual_ratio = ble_broadcast_timing_get_actual_listen_ratio(&state);
    assert(fabs(actual_ratio - 0.8) < 0.001);

    TEST_PASS();
}


void test_rng(void)
{
    TEST_START("Random number generator");

    uint32_t seed = 12345;

    
    uint32_t r1 = ble_broadcast_timing_rand(&seed);
    uint32_t r2 = ble_broadcast_timing_rand(&seed);
    uint32_t r3 = ble_broadcast_timing_rand(&seed);

    
    assert(r1 != r2);
    assert(r2 != r3);
    assert(r1 != r3);

    
    seed = 12345;
    double d1 = ble_broadcast_timing_rand_double(&seed);
    double d2 = ble_broadcast_timing_rand_double(&seed);

    assert(d1 >= 0.0 && d1 < 1.0);
    assert(d2 >= 0.0 && d2 < 1.0);
    assert(fabs(d1 - d2) > 0.001);

    TEST_PASS();
}


void test_noisy_broadcast(void)
{
    TEST_START("Noisy broadcast schedule");

    ble_broadcast_timing_t state;
    ble_broadcast_timing_init(&state, BLE_BROADCAST_SCHEDULE_NOISY, 10, 50, 0.8);
    ble_broadcast_timing_set_seed(&state, 999);

    
    int trials = 100;
    for (int i = 0; i < trials; i++) {
        ble_broadcast_timing_advance_slot(&state);
    }

    
    double actual_ratio = ble_broadcast_timing_get_actual_listen_ratio(&state);
    assert(actual_ratio > 0.7 && actual_ratio < 0.9);

    TEST_PASS();
}


void test_stochastic_timing(void)
{
    TEST_START("Stochastic broadcast timing");

    ble_broadcast_timing_t state;
    ble_broadcast_timing_init(&state, BLE_BROADCAST_SCHEDULE_STOCHASTIC, 10, 100, 0.75);
    ble_broadcast_timing_set_seed(&state, 777);

    
    int trials = 200;
    for (int i = 0; i < trials; i++) {
        ble_broadcast_timing_advance_slot(&state);
    }

    double actual_ratio = ble_broadcast_timing_get_actual_listen_ratio(&state);
    assert(actual_ratio > 0.65 && actual_ratio < 0.85);  

    TEST_PASS();
}


void test_collision_avoidance(void)
{
    TEST_START("Collision avoidance");

    
    ble_broadcast_timing_t node1, node2;
    ble_broadcast_timing_init(&node1, BLE_BROADCAST_SCHEDULE_STOCHASTIC, 10, 100, 0.8);
    ble_broadcast_timing_init(&node2, BLE_BROADCAST_SCHEDULE_STOCHASTIC, 10, 100, 0.8);

    ble_broadcast_timing_set_seed(&node1, 111);
    ble_broadcast_timing_set_seed(&node2, 222);

    
    int collisions = 0;
    int trials = 100;

    for (int i = 0; i < trials; i++) {
        bool n1_broadcast = ble_broadcast_timing_advance_slot(&node1);
        bool n2_broadcast = ble_broadcast_timing_advance_slot(&node2);

        if (n1_broadcast && n2_broadcast) {
            collisions++;
        }
    }

    
    double collision_rate = (double)collisions / (double)trials;
    assert(collision_rate < 0.1);  

    TEST_PASS();
}


void test_reset_retry(void)
{
    TEST_START("Reset retry counter");

    ble_broadcast_timing_t state;
    ble_broadcast_timing_init(&state, BLE_BROADCAST_SCHEDULE_NOISY, 10, 100, 0.5);

    ble_broadcast_timing_record_failure(&state);
    ble_broadcast_timing_record_failure(&state);
    assert(state.retry_count == 2);

    ble_broadcast_timing_reset_retry(&state);
    assert(state.retry_count == 0);
    assert(state.message_sent == false);

    TEST_PASS();
}

int main(void)
{
    printf("=== BLE Broadcast Timing C Core Tests (Tasks 12 & 14) ===\n\n");

    test_init();
    test_advance_slot();
    test_listen_ratio();
    test_success_tracking();
    test_retry_logic();
    test_success_rate();
    test_actual_listen_ratio();
    test_rng();
    test_noisy_broadcast();
    test_stochastic_timing();
    test_collision_avoidance();
    test_reset_retry();

    printf("\n=== Test Summary ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passed);
    printf("Failed: %d\n", test_count - test_passed);

    if (test_passed == test_count) {
        printf("\nALL TESTS PASSED ✓\n");
        return 0;
    } else {
        printf("\nSOME TESTS FAILED ✗\n");
        return 1;
    }
}