/**
 * @file ble_discovery_cycle.c
 * @brief Pure C implementation of BLE Discovery Cycle state machine
 *
 */
jason peng
#include "ble_discovery_cycle.h"
#include <string.h>

void ble_discovery_cycle_init(ble_discovery_cycle_t *cycle)
{
    if (cycle == NULL) {
        return;
    }

    memset(cycle, 0, sizeof(ble_discovery_cycle_t));
    cycle->running = false;
    cycle->current_slot = 0;
    cycle->slot_duration_ms = BLE_DISCOVERY_DEFAULT_SLOT_DURATION_MS;
    cycle->cycle_count = 0;
    cycle->user_data = NULL;
    cycle->cycle_complete_callback = NULL;

    for (int i = 0; i < BLE_DISCOVERY_NUM_SLOTS; i++) {
        cycle->slot_callbacks[i] = NULL;
    }
}


bool ble_discovery_cycle_set_slot_duration(ble_discovery_cycle_t *cycle, uint32_t duration_ms)
{
    if (cycle == NULL) {
        return false;
    }

    if (cycle->running) {
        return false;
    }

    cycle->slot_duration_ms = duration_ms;
    return true;
}


uint32_t ble_discovery_cycle_get_slot_duration(const ble_discovery_cycle_t *cycle)
{
    if (cycle == NULL) {
        return 0;
    }
    return cycle->slot_duration_ms;
}

uint32_t ble_discovery_cycle_get_cycle_duration(const ble_discovery_cycle_t *cycle)
{
    if (cycle == NULL) {
        return 0;
    }
    return cycle->slot_duration_ms * BLE_DISCOVERY_NUM_SLOTS;
}

bool ble_discovery_cycle_start(ble_discovery_cycle_t *cycle)
{
    if (cycle == NULL) {
        return false;
    }

    
    if (cycle->running) {
        return false;
    }

    cycle->running = true;
    cycle->current_slot = 0;
    return true;
}

void ble_discovery_cycle_stop(ble_discovery_cycle_t *cycle)
{
    if (cycle == NULL) {
        return;
    }

    cycle->running = false;
}

bool ble_discovery_cycle_is_running(const ble_discovery_cycle_t *cycle)
{
    if (cycle == NULL) {
        return false;
    }
    return cycle->running;
}

uint8_t ble_discovery_cycle_get_current_slot(const ble_discovery_cycle_t *cycle)
{
    if (cycle == NULL) {
        return 0;
    }
    return cycle->current_slot;
}

ble_slot_type_t ble_discovery_cycle_get_slot_type(uint8_t slot_number)
{
    if (slot_number == BLE_DISCOVERY_SLOT_OWN_MESSAGE) {
        return BLE_SLOT_TYPE_OWN_MESSAGE;
    }
    return BLE_SLOT_TYPE_FORWARDING;
}

bool ble_discovery_cycle_is_valid_slot(uint8_t slot_number)
{
    return slot_number < BLE_DISCOVERY_NUM_SLOTS;
}

bool ble_discovery_cycle_is_forwarding_slot(uint8_t slot_number)
{
    return slot_number >= BLE_DISCOVERY_SLOT_FORWARD_1 &&
           slot_number <= BLE_DISCOVERY_SLOT_FORWARD_3;
}

bool ble_discovery_cycle_set_slot_callback(ble_discovery_cycle_t *cycle,
                                            uint8_t slot_number,
                                            ble_slot_callback_t callback)
{
    if (cycle == NULL) {
        return false;
    }

    if (!ble_discovery_cycle_is_valid_slot(slot_number)) {
        return false;
    }

    cycle->slot_callbacks[slot_number] = callback;
    return true;
}

void ble_discovery_cycle_set_complete_callback(ble_discovery_cycle_t *cycle,
                                                ble_cycle_complete_callback_t callback)
{
    if (cycle == NULL) {
        return;
    }
    cycle->cycle_complete_callback = callback;
}

void ble_discovery_cycle_set_user_data(ble_discovery_cycle_t *cycle, void *user_data)
{
    if (cycle == NULL) {
        return;
    }
    cycle->user_data = user_data;
}


void ble_discovery_cycle_execute_slot(ble_discovery_cycle_t *cycle)
{
    if (cycle == NULL || !cycle->running) {
        return;
    }

    uint8_t slot = cycle->current_slot;

    
    if (slot < BLE_DISCOVERY_NUM_SLOTS && cycle->slot_callbacks[slot] != NULL) {
        cycle->slot_callbacks[slot](slot, cycle->user_data);
    }
}

uint8_t ble_discovery_cycle_advance_slot(ble_discovery_cycle_t *cycle)
{
    if (cycle == NULL || !cycle->running) {
        return 0;
    }

    cycle->current_slot++;

    
    if (cycle->current_slot >= BLE_DISCOVERY_NUM_SLOTS) {
        cycle->current_slot = 0;
        cycle->cycle_count++;

        
        if (cycle->cycle_complete_callback != NULL) {
            cycle->cycle_complete_callback(cycle->cycle_count, cycle->user_data);
        }
    }

    return cycle->current_slot;
}

uint32_t ble_discovery_cycle_get_slot_offset(const ble_discovery_cycle_t *cycle, uint8_t slot_number)
{
    if (cycle == NULL) {
        return 0;
    }

    if (!ble_discovery_cycle_is_valid_slot(slot_number)) {
        return 0;
    }

    return slot_number * cycle->slot_duration_ms;
}

uint32_t ble_discovery_cycle_get_cycle_count(const ble_discovery_cycle_t *cycle)
{
    if (cycle == NULL) {
        return 0;
    }
    return cycle->cycle_count;
}

void ble_discovery_cycle_reset_count(ble_discovery_cycle_t *cycle)
{
    if (cycle == NULL) {
        return;
    }
    cycle->cycle_count = 0;
}

const char* ble_discovery_cycle_slot_type_name(ble_slot_type_t slot_type)
{
    switch (slot_type) {
        case BLE_SLOT_TYPE_OWN_MESSAGE:
            return "OWN_MESSAGE";
        case BLE_SLOT_TYPE_FORWARDING:
            return "FORWARDING";
        default:
            return "UNKNOWN";
    }
}

const char* ble_discovery_cycle_slot_name(uint8_t slot_number)
{
    switch (slot_number) {
        case 0:
            return "Slot 0 (Own Message)";
        case 1:
            return "Slot 1 (Forward 1)";
        case 2:
            return "Slot 2 (Forward 2)";
        case 3:
            return "Slot 3 (Forward 3)";
        default:
            return "Invalid Slot";
    }
}
