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

static void
ble_broadcast_apply_phase_defaults(ble_broadcast_schedule_type_t schedule_type,
                                   uint32_t *num_slots,
                                   double *listen_ratio)
{
    if (!num_slots || !listen_ratio) {
        return;
    }

    if (schedule_type == BLE_BROADCAST_SCHEDULE_NOISY) {
        if (*num_slots == BLE_BROADCAST_AUTO_SLOTS) {
            *num_slots = BLE_BROADCAST_NOISE_DEFAULT_SLOTS;
        }
        if (*listen_ratio == BLE_BROADCAST_AUTO_RATIO) {
            *listen_ratio = BLE_BROADCAST_NOISE_LISTEN_RATIO;
        }
    } else {
        if (*num_slots == BLE_BROADCAST_AUTO_SLOTS) {
            *num_slots = BLE_BROADCAST_NEIGHBOR_DEFAULT_SLOTS;
        }
        if (*listen_ratio == BLE_BROADCAST_AUTO_RATIO) {
            *listen_ratio = BLE_BROADCAST_NEIGHBOR_LISTEN_RATIO;
        }
    }
}

static double
ble_broadcast_clamp(double value, double min, double max)
{
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static uint32_t
ble_broadcast_compute_neighbor_tx_slots(double crowding_factor)
{
    crowding_factor = ble_broadcast_clamp(crowding_factor, 0.0, 1.0);
    double range = (double)(BLE_BROADCAST_NEIGHBOR_MAX_TX_SLOTS - BLE_BROADCAST_NEIGHBOR_MIN_TX_SLOTS);
    double value = (double)BLE_BROADCAST_NEIGHBOR_MIN_TX_SLOTS +
                   (1.0 - crowding_factor) * range;
    uint32_t slots = (uint32_t)ceil(value);
    if (slots < BLE_BROADCAST_NEIGHBOR_MIN_TX_SLOTS) {
        slots = BLE_BROADCAST_NEIGHBOR_MIN_TX_SLOTS;
    }
    if (slots > BLE_BROADCAST_NEIGHBOR_MAX_TX_SLOTS) {
        slots = BLE_BROADCAST_NEIGHBOR_MAX_TX_SLOTS;
    }
    return slots;
}

static void
ble_broadcast_apply_neighbor_profile(ble_broadcast_timing_t *state)
{
    if (!state || state->schedule_type != BLE_BROADCAST_SCHEDULE_STOCHASTIC) {
        return;
    }

    if (state->num_slots < BLE_BROADCAST_NEIGHBOR_DEFAULT_SLOTS) {
        state->num_slots = BLE_BROADCAST_NEIGHBOR_DEFAULT_SLOTS;
    }
    if (state->num_slots > BLE_BROADCAST_MAX_SLOTS) {
        state->num_slots = BLE_BROADCAST_MAX_SLOTS;
    }

    uint32_t tx_slots = ble_broadcast_compute_neighbor_tx_slots(state->crowding_factor);
    state->max_broadcast_slots = tx_slots;

    if (state->num_slots > 0) {
        double listen_ratio = 1.0 - ((double)tx_slots / (double)state->num_slots);
        state->listen_ratio = ble_broadcast_clamp(listen_ratio, 0.0, 1.0);
    }
}

void ble_broadcast_timing_init(ble_broadcast_timing_t *state,
                                 ble_broadcast_schedule_type_t schedule_type,
                                 uint32_t num_slots,
                                 uint32_t slot_duration_ms,
                                 double listen_ratio)
{
    if (!state) return;

    memset(state, 0, sizeof(ble_broadcast_timing_t));

    double resolved_ratio = listen_ratio;
    uint32_t resolved_slots = num_slots;

    ble_broadcast_apply_phase_defaults(schedule_type, &resolved_slots, &resolved_ratio);

    if (resolved_ratio < 0.0 || resolved_ratio > 1.0) {
        resolved_ratio = BLE_BROADCAST_DEFAULT_LISTEN_RATIO;
    }

    if (resolved_slots == 0) {
        resolved_slots = BLE_BROADCAST_NOISE_DEFAULT_SLOTS;
    }

    if (resolved_slots > BLE_BROADCAST_MAX_SLOTS) {
        resolved_slots = BLE_BROADCAST_MAX_SLOTS;
    }

    state->schedule_type = schedule_type;
    state->num_slots = resolved_slots;
    state->slot_duration_ms = slot_duration_ms;
    state->listen_ratio = resolved_ratio;

    state->current_slot = 0;
    state->is_broadcast_slot = false;
    state->broadcast_attempts = 0;
    state->broadcasts_this_cycle = 0;
    state->max_broadcast_slots = BLE_BROADCAST_MAX_SLOTS;
    state->crowding_factor = 0.5;

    if (schedule_type == BLE_BROADCAST_SCHEDULE_STOCHASTIC) {
        ble_broadcast_apply_neighbor_profile(state);
    } else {
        state->max_broadcast_slots = BLE_BROADCAST_MAX_SLOTS;
    }

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
    if (state->current_slot == 0) {
        state->broadcasts_this_cycle = 0;
    }

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
        bool forced_listen = false;
        if (state->broadcasts_this_cycle >= state->max_broadcast_slots) {
            forced_listen = true;
        }

        if (forced_listen || random_value < state->listen_ratio) {
            /* Majority: Listen */
            state->is_broadcast_slot = false;
            state->total_listen_slots++;
        } else {
            /* Minority: Broadcast (randomized to avoid collisions) */
            state->is_broadcast_slot = true;
            state->total_broadcast_slots++;
            state->broadcast_attempts++;
            state->broadcasts_this_cycle++;
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
void ble_broadcast_timing_set_crowding(ble_broadcast_timing_t *state,
                                       double crowding_factor)
{
    if (!state) return;

    state->crowding_factor = ble_broadcast_clamp(crowding_factor, 0.0, 1.0);

    if (state->schedule_type == BLE_BROADCAST_SCHEDULE_STOCHASTIC) {
        ble_broadcast_apply_neighbor_profile(state);
        /* Reset cycle counters so the new limits apply cleanly */
        state->broadcasts_this_cycle = 0;
    }
}

uint32_t ble_broadcast_timing_get_max_broadcast_slots(const ble_broadcast_timing_t *state)
{
    if (!state) {
        return 0;
    }
    return state->max_broadcast_slots;
}
