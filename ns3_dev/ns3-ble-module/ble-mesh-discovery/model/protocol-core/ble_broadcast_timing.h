/**
 * @file ble_broadcast_timing.h
 * @brief Pure C implementation of stochastic broadcast timing (Tasks 12 & 14)
 *
 * This module implements:
 * - Task 12: Noisy broadcast phase with randomized listening
 * - Task 14: Stochastic broadcast timing with collision avoidance
 *
 * NO dependencies on NS-3 or C++ - portable to embedded systems.
 */

#ifndef BLE_BROADCAST_TIMING_H
#define BLE_BROADCAST_TIMING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


#define BLE_BROADCAST_MAX_SLOTS 10          /**< Maximum number of time slots */
#define BLE_BROADCAST_DEFAULT_LISTEN_RATIO 0.8  /**< 80% listen, 20% broadcast */
#define BLE_BROADCAST_MAX_RETRIES 3         /**< Maximum broadcast retry attempts */

/**
 * @brief Broadcast schedule type
 */
typedef enum {
    BLE_BROADCAST_SCHEDULE_NOISY,     /**< Noisy broadcast (Task 12) */
    BLE_BROADCAST_SCHEDULE_STOCHASTIC /**< Stochastic timing (Task 14) */
} ble_broadcast_schedule_type_t;

/**
 * @brief Broadcast timing state
 */
typedef struct {
    ble_broadcast_schedule_type_t schedule_type; /**< Schedule type */

    
    uint32_t num_slots;                /**< Number of time slots */
    uint32_t slot_duration_ms;         /**< Duration of each slot (ms) */

    
    uint32_t current_slot;             /**< Current slot index */
    bool is_broadcast_slot;            /**< True if current slot is for broadcasting */
    uint32_t broadcast_attempts;       /**< Number of broadcast attempts so far */

    
    double listen_ratio;               /**< Probability of listening (0.0-1.0) */
    uint32_t seed;                     /**< Random seed for reproducibility */

    
    uint32_t max_retries;              /**< Maximum retry attempts */
    uint32_t retry_count;              /**< Current retry count */
    bool message_sent;                 /**< True if message successfully sent */

    
    uint32_t total_broadcast_slots;    /**< Total broadcast slots assigned */
    uint32_t total_listen_slots;       /**< Total listen slots assigned */
    uint32_t successful_broadcasts;    /**< Successful broadcast count */
    uint32_t failed_broadcasts;        /**< Failed broadcast count */
} ble_broadcast_timing_t;

/**
 * @brief Initialize broadcast timing state
 * @param state Pointer to broadcast timing structure
 * @param schedule_type Type of schedule (noisy or stochastic)
 * @param num_slots Number of time slots
 * @param slot_duration_ms Duration of each slot in milliseconds
 * @param listen_ratio Probability of listening (0.0-1.0)
 */
void ble_broadcast_timing_init(ble_broadcast_timing_t *state,
                                 ble_broadcast_schedule_type_t schedule_type,
                                 uint32_t num_slots,
                                 uint32_t slot_duration_ms,
                                 double listen_ratio);

/**
 * @brief Set random seed for stochastic timing
 * @param state Broadcast timing state
 * @param seed Random seed value
 */
void ble_broadcast_timing_set_seed(ble_broadcast_timing_t *state, uint32_t seed);

/**
 * @brief Advance to next slot and determine if it's broadcast or listen
 *
 * For noisy broadcast (Task 12):
 * - Randomly selects broadcast/listen based on listen_ratio
 * - Creates stochastic pattern for collision avoidance
 *
 * For stochastic timing (Task 14):
 * - Random slot selection for clusterhead broadcasts
 * - Majority-listening, minority-broadcasting schedule
 *
 * @param state Broadcast timing state
 * @return true if current slot is for broadcasting, false for listening
 */
bool ble_broadcast_timing_advance_slot(ble_broadcast_timing_t *state);

/**
 * @brief Check if current slot is for broadcasting
 * @param state Broadcast timing state
 * @return true if should broadcast in current slot
 */
bool ble_broadcast_timing_should_broadcast(const ble_broadcast_timing_t *state);

/**
 * @brief Check if current slot is for listening
 * @param state Broadcast timing state
 * @return true if should listen in current slot
 */
bool ble_broadcast_timing_should_listen(const ble_broadcast_timing_t *state);

/**
 * @brief Record successful broadcast
 *
 * Updates statistics and retry logic.
 *
 * @param state Broadcast timing state
 */
void ble_broadcast_timing_record_success(ble_broadcast_timing_t *state);

/**
 * @brief Record failed broadcast
 *
 * Updates statistics and determines if retry needed.
 *
 * @param state Broadcast timing state
 * @return true if should retry broadcast
 */
bool ble_broadcast_timing_record_failure(ble_broadcast_timing_t *state);

/**
 * @brief Reset retry counter
 * @param state Broadcast timing state
 */
void ble_broadcast_timing_reset_retry(ble_broadcast_timing_t *state);

/**
 * @brief Get broadcast success rate
 * @param state Broadcast timing state
 * @return Success rate (0.0-1.0)
 */
double ble_broadcast_timing_get_success_rate(const ble_broadcast_timing_t *state);

/**
 * @brief Get current slot index
 * @param state Broadcast timing state
 * @return Current slot index (0 to num_slots-1)
 */
uint32_t ble_broadcast_timing_get_current_slot(const ble_broadcast_timing_t *state);

/**
 * @brief Calculate listen/broadcast distribution
 *
 * Returns the actual ratio of listen vs broadcast slots observed.
 *
 * @param state Broadcast timing state
 * @return Actual listen ratio (0.0-1.0)
 */
double ble_broadcast_timing_get_actual_listen_ratio(const ble_broadcast_timing_t *state);

/**
 * @brief Simple pseudo-random number generator (LCG)
 *
 * Used internally for stochastic slot selection.
 *
 * @param seed Pointer to seed value (updated each call)
 * @return Random number (0 to UINT32_MAX)
 */
uint32_t ble_broadcast_timing_rand(uint32_t *seed);

/**
 * @brief Generate random double in range [0.0, 1.0)
 * @param seed Pointer to seed value (updated each call)
 * @return Random double (0.0-1.0)
 */
double ble_broadcast_timing_rand_double(uint32_t *seed);

#ifdef __cplusplus
}
#endif

#endif 