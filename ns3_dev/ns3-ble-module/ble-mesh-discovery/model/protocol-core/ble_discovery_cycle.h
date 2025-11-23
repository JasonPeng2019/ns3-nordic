/**
 * @file ble_discovery_cycle.h
 * @brief Pure C implementation of BLE Discovery Cycle state machine
 *
 * This file contains the portable C implementation of the 4-slot discovery
 * cycle for BLE mesh networks. It can be used standalone on embedded systems
 * or wrapped for NS-3 simulation.
 *
 * The discovery cycle consists of:
 * - Slot 0: Own message transmission
 * - Slots 1-3: Forwarding received messages
 *
 * Copyright (c) 2025
 * Author: jason peng <jason.p>
 */

#ifndef BLE_DISCOVERY_CYCLE_H
#define BLE_DISCOVERY_CYCLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Number of slots in a discovery cycle - NOT stochastic slots, but messaging protocol slots
 */
#define BLE_DISCOVERY_NUM_SLOTS 4 

/**
 * @brief Default slot duration in milliseconds
 */
#define BLE_DISCOVERY_DEFAULT_SLOT_DURATION_MS 100

/**
 * @brief Slot 0 - own message transmission
 */
#define BLE_DISCOVERY_SLOT_OWN_MESSAGE 0

/**
 * @brief First forwarding slot
 */
#define BLE_DISCOVERY_SLOT_FORWARD_1 1

/**
 * @brief Second forwarding slot
 */
#define BLE_DISCOVERY_SLOT_FORWARD_2 2

/**
 * @brief Third forwarding slot
 */
#define BLE_DISCOVERY_SLOT_FORWARD_3 3

/**
 * @brief Slot type enumeration
 */
typedef enum {
    BLE_SLOT_TYPE_OWN_MESSAGE = 0,   /**< Slot for transmitting own discovery message */
    BLE_SLOT_TYPE_FORWARDING = 1     /**< Slot for forwarding received messages */
} ble_slot_type_t;

/**
 * @brief Callback function type for slot execution
 * @param slot_number The slot number (0-3)
 * @param user_data User-provided context pointer
 */
typedef void (*ble_slot_callback_t)(uint8_t slot_number, void *user_data);

/**
 * @brief Callback function type for cycle completion
 * @param cycle_count The number of completed cycles
 * @param user_data User-provided context pointer
 */
typedef void (*ble_cycle_complete_callback_t)(uint32_t cycle_count, void *user_data);

/**
 * @brief Discovery cycle state structure
 *
 * This structure maintains the state of a discovery cycle.
 * It is designed to be portable and not depend on any specific
 * timing implementation.
 */
typedef struct {
    bool running;                    /**< Whether the cycle is currently active */
    uint8_t current_slot;            /**< Current slot number (0-3) */
    uint32_t slot_duration_ms;       /**< Duration of each slot in milliseconds */
    uint32_t cycle_count;            /**< Number of completed cycles */

    
    ble_slot_callback_t slot_callbacks[BLE_DISCOVERY_NUM_SLOTS];  /**< Callbacks for each slot */
    ble_cycle_complete_callback_t cycle_complete_callback;        /**< Callback when cycle completes */
    void *user_data;                 /**< User-provided context for callbacks */
} ble_discovery_cycle_t;

/**
 * @brief Initialize a discovery cycle structure
 * @param cycle Pointer to the cycle structure to initialize
 */
void ble_discovery_cycle_init(ble_discovery_cycle_t *cycle);

/**
 * @brief Set the slot duration
 * @param cycle Pointer to the cycle structure
 * @param duration_ms Slot duration in milliseconds
 * @return true if successful, false if cycle is running
 */
bool ble_discovery_cycle_set_slot_duration(ble_discovery_cycle_t *cycle, uint32_t duration_ms);

/**
 * @brief Get the slot duration
 * @param cycle Pointer to the cycle structure
 * @return Slot duration in milliseconds
 */
uint32_t ble_discovery_cycle_get_slot_duration(const ble_discovery_cycle_t *cycle);

/**
 * @brief Get the total cycle duration (4 slots)
 * @param cycle Pointer to the cycle structure
 * @return Total cycle duration in milliseconds
 */
uint32_t ble_discovery_cycle_get_cycle_duration(const ble_discovery_cycle_t *cycle);

/**
 * @brief Start the discovery cycle
 * @param cycle Pointer to the cycle structure
 * @return true if started successfully, false if already running
 */
bool ble_discovery_cycle_start(ble_discovery_cycle_t *cycle);

/**
 * @brief Stop the discovery cycle
 * @param cycle Pointer to the cycle structure
 */
void ble_discovery_cycle_stop(ble_discovery_cycle_t *cycle);

/**
 * @brief Check if the cycle is running
 * @param cycle Pointer to the cycle structure
 * @return true if running, false otherwise
 */
bool ble_discovery_cycle_is_running(const ble_discovery_cycle_t *cycle);

/**
 * @brief Get the current slot number
 * @param cycle Pointer to the cycle structure
 * @return Current slot number (0-3)
 */
uint8_t ble_discovery_cycle_get_current_slot(const ble_discovery_cycle_t *cycle);

/**
 * @brief Get the type of a slot
 * @param slot_number Slot number (0-3)
 * @return Slot type (own message or forwarding)
 */
ble_slot_type_t ble_discovery_cycle_get_slot_type(uint8_t slot_number);

/**
 * @brief Check if a slot number is valid
 * @param slot_number Slot number to check
 * @return true if valid (0-3), false otherwise
 */
bool ble_discovery_cycle_is_valid_slot(uint8_t slot_number);

/**
 * @brief Check if a slot is a forwarding slot
 * @param slot_number Slot number to check
 * @return true if forwarding slot (1-3), false otherwise
 */
bool ble_discovery_cycle_is_forwarding_slot(uint8_t slot_number);

/**
 * @brief Set callback for a specific slot
 * @param cycle Pointer to the cycle structure
 * @param slot_number Slot number (0-3)
 * @param callback Callback function to invoke
 * @return true if successful, false if invalid slot number
 */
bool ble_discovery_cycle_set_slot_callback(ble_discovery_cycle_t *cycle,
                                            uint8_t slot_number,
                                            ble_slot_callback_t callback);

/**
 * @brief Set callback for cycle completion
 * @param cycle Pointer to the cycle structure
 * @param callback Callback function to invoke when cycle completes
 */
void ble_discovery_cycle_set_complete_callback(ble_discovery_cycle_t *cycle,
                                                ble_cycle_complete_callback_t callback);

/**
 * @brief Set user data for callbacks
 * @param cycle Pointer to the cycle structure
 * @param user_data Pointer to user-provided context
 */
void ble_discovery_cycle_set_user_data(ble_discovery_cycle_t *cycle, void *user_data);

/**
 * @brief Execute the current slot (call this from timer interrupt or scheduler)
 * @param cycle Pointer to the cycle structure
 *
 * This function should be called by the platform-specific timing mechanism
 * (e.g., timer interrupt, NS-3 scheduler) at each slot boundary.
 */
void ble_discovery_cycle_execute_slot(ble_discovery_cycle_t *cycle);

/**
 * @brief Advance to the next slot
 * @param cycle Pointer to the cycle structure
 * @return The new slot number, or 0 if cycle wrapped
 *
 * This function advances the slot counter and handles cycle completion.
 * It does NOT execute the slot callback - use ble_discovery_cycle_execute_slot for that.
 */
uint8_t ble_discovery_cycle_advance_slot(ble_discovery_cycle_t *cycle);

/**
 * @brief Get the time offset for a specific slot within a cycle
 * @param cycle Pointer to the cycle structure
 * @param slot_number Slot number (0-3)
 * @return Time offset in milliseconds from cycle start, or 0 if invalid slot
 */
uint32_t ble_discovery_cycle_get_slot_offset(const ble_discovery_cycle_t *cycle, uint8_t slot_number);

/**
 * @brief Get the number of completed cycles
 * @param cycle Pointer to the cycle structure
 * @return Number of completed cycles
 */
uint32_t ble_discovery_cycle_get_cycle_count(const ble_discovery_cycle_t *cycle);

/**
 * @brief Reset the cycle counter
 * @param cycle Pointer to the cycle structure
 */
void ble_discovery_cycle_reset_count(ble_discovery_cycle_t *cycle);

/**
 * @brief Get a string name for a slot type
 * @param slot_type The slot type
 * @return String name of the slot type
 */
const char* ble_discovery_cycle_slot_type_name(ble_slot_type_t slot_type);

/**
 * @brief Get a string description for a slot number
 * @param slot_number Slot number (0-3)
 * @return String description of the slot
 */
const char* ble_discovery_cycle_slot_name(uint8_t slot_number);

#ifdef __cplusplus
}
#endif

#endif 
