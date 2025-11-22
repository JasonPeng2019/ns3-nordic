/**
 * @file ble_broadcast_timing.c
 * @brief Implementation of stochastic broadcast timing (Tasks 12 & 14)
 */

#include "ble_broadcast_timing.h"
#include <string.h>
#include <math.h>

/* Linear Congruential Generator (LCG) constants */
#define LCG_A 1664525U
#define LCG_C 1013904223U

void ble_broadcast_timing_init(ble_broadcast_timing_t *state,
                                 ble_broadcast_schedule_type_t schedule_type,
                                 uint32_t num_slots,
                                 uint32_t slot_duration_ms,
                                 double listen_ratio)
{
    if (!state) return;

    memset(state, 0, sizeof(ble_broadcast_timing_t));

    state->schedule_type = schedule_type;
    state->num_slots = (num_slots > 0 && num_slots <= BLE_BROADCAST_MAX_SLOTS)
                        ? num_slots : BLE_BROADCAST_MAX_SLOTS;
    state->slot_duration_ms = slot_duration_ms;
    state->listen_ratio = (listen_ratio >= 0.0 && listen_ratio <= 1.0)
                           ? listen_ratio : BLE_BROADCAST_DEFAULT_LISTEN_RATIO;

    state->current_slot = 0;
    state->is_broadcast_slot = false;
    state->broadcast_attempts = 0;

    state->seed = 12345;  /* Default seed */
    state->max_retries = BLE_BROADCAST_MAX_RETRIES;
    state->retry_count = 0;
    state->message_sent = false;

    state->total_broadcast_slots = 0;
    state->total_listen_slots = 0;
    state->successful_broadcasts = 0;
    state->failed_broadcasts = 0;
}

void ble_broadcast_timing_set_seed(ble_broadcast_timing_t *state, uint32_t seed)
{
    if (state) {
        state->seed = seed;
    }
}

uint32_t ble_broadcast_timing_rand(uint32_t *seed)
{
    if (!seed) return 0;
    *seed = (LCG_A * (*seed) + LCG_C);
    return *seed;
}

double ble_broadcast_timing_rand_double(uint32_t *seed)
{
    uint32_t rand_val = ble_broadcast_timing_rand(seed);
    return (double)rand_val / (double)UINT32_MAX;
}

bool ble_broadcast_timing_advance_slot(ble_broadcast_timing_t *state)
{
    if (!state) return false;

    /* Advance slot */
    state->current_slot = (state->current_slot + 1) % state->num_slots;

    /* Determine if this slot is broadcast or listen */
    double random_value = ble_broadcast_timing_rand_double(&state->seed);

    if (state->schedule_type == BLE_BROADCAST_SCHEDULE_NOISY) {
        /* Task 12: Noisy broadcast
         * - Stochastic randomized listening time slot selection
         * - Use listen_ratio to determine probability
         */
        if (random_value < state->listen_ratio) {
            /* Listen slot */
            state->is_broadcast_slot = false;
            state->total_listen_slots++;
        } else {
            /* Broadcast slot */
            state->is_broadcast_slot = true;
            state->total_broadcast_slots++;
            state->broadcast_attempts++;
        }
    } else {
        /* Task 14: Stochastic broadcast timing
         * - Random time slot selection for clusterhead broadcasts
         * - Majority-listening, minority-broadcasting schedule
         * - Collision avoidance through randomization
         */
        if (random_value < state->listen_ratio) {
            /* Majority: Listen */
            state->is_broadcast_slot = false;
            state->total_listen_slots++;
        } else {
            /* Minority: Broadcast (randomized to avoid collisions) */
            state->is_broadcast_slot = true;
            state->total_broadcast_slots++;
            state->broadcast_attempts++;
        }
    }

    return state->is_broadcast_slot;
}

bool ble_broadcast_timing_should_broadcast(const ble_broadcast_timing_t *state)
{
    if (!state) return false;
    return state->is_broadcast_slot;
}

bool ble_broadcast_timing_should_listen(const ble_broadcast_timing_t *state)
{
    if (!state) return true;  /* Default to listening */
    return !state->is_broadcast_slot;
}

void ble_broadcast_timing_record_success(ble_broadcast_timing_t *state)
{
    if (!state) return;

    state->successful_broadcasts++;
    state->message_sent = true;
    state->retry_count = 0;  /* Reset retry counter on success */
}

bool ble_broadcast_timing_record_failure(ble_broadcast_timing_t *state)
{
    if (!state) return false;

    state->failed_broadcasts++;
    state->retry_count++;

    /* Check if should retry */
    if (state->retry_count < state->max_retries) {
        return true;  /* Retry */
    }

    /* Max retries reached, give up */
    state->retry_count = 0;
    return false;
}

void ble_broadcast_timing_reset_retry(ble_broadcast_timing_t *state)
{
    if (!state) return;

    state->retry_count = 0;
    state->message_sent = false;
}

double ble_broadcast_timing_get_success_rate(const ble_broadcast_timing_t *state)
{
    if (!state) return 0.0;

    uint32_t total_attempts = state->successful_broadcasts + state->failed_broadcasts;
    if (total_attempts == 0) return 0.0;

    return (double)state->successful_broadcasts / (double)total_attempts;
}

uint32_t ble_broadcast_timing_get_current_slot(const ble_broadcast_timing_t *state)
{
    if (!state) return 0;
    return state->current_slot;
}

double ble_broadcast_timing_get_actual_listen_ratio(const ble_broadcast_timing_t *state)
{
    if (!state) return 0.0;

    uint32_t total_slots = state->total_listen_slots + state->total_broadcast_slots;
    if (total_slots == 0) return 0.0;

    return (double)state->total_listen_slots / (double)total_slots;
}