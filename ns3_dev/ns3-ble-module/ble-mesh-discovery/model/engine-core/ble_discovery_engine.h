/**
 * @file ble_discovery_engine.h
 * @brief Phase 2 BLE discovery event engine (portable C implementation)
 *
 * This engine wires the pure C helpers (discovery cycle, node model,
 * message queue, forwarding logic) into a single reusable state machine.
 * The engine is platform agnostic: provide callbacks for packet TX/logging,
 * push received packets in, and call ble_engine_tick() at every slot boundary.
 */

#ifndef BLE_DISCOVERY_ENGINE_H
#define BLE_DISCOVERY_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include "ble_discovery_cycle.h"
#include "ble_discovery_packet.h"
#include "ble_forwarding_logic.h"
#include "ble_message_queue.h"
#include "ble_mesh_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback invoked when the engine needs to transmit a packet
 * @param packet Discovery packet ready for TX
 * @param user_context User-provided context pointer
 */
typedef void (*ble_engine_send_callback)(const ble_discovery_packet_t *packet,
                                         void *user_context);

/**
 * @brief Optional logging callback
 * @param level Informational string describing log severity
 * @param message Null-terminated message
 * @param user_context User-provided context pointer
 */
typedef void (*ble_engine_log_callback)(const char *level,
                                        const char *message,
                                        void *user_context);

/**
 * @brief Engine configuration (static parameters + callbacks)
 */
typedef struct {
    uint32_t node_id;                  /**< Unique node identifier */
    uint32_t slot_duration_ms;         /**< Duration of each discovery slot */
    uint8_t initial_ttl;               /**< TTL for locally-originated messages */
    double proximity_threshold;        /**< GPS proximity threshold (meters) */
    ble_engine_send_callback send_cb;  /**< Packet transmission callback */
    ble_engine_log_callback log_cb;    /**< Optional logging callback */
    void *user_context;                /**< Passed to callbacks */
} ble_engine_config_t;

/**
 * @brief Discovery engine context
 */
typedef struct {
    bool initialized;
    ble_engine_config_t config;
    ble_discovery_cycle_t cycle;
    ble_message_queue_t forward_queue;
    ble_mesh_node_t node;
    ble_discovery_packet_t tx_buffer;
    double crowding_factor;
    double proximity_threshold;
    uint32_t last_tick_time_ms;
} ble_engine_t;

/**
 * @brief Initialize configuration with sensible defaults
 * @param config Pointer to configuration struct
 */
void ble_engine_config_init(ble_engine_config_t *config);

/**
 * @brief Initialize engine instance
 * @param engine Pointer to engine context
 * @param config Engine configuration
 * @return true on success
 */
bool ble_engine_init(ble_engine_t *engine, const ble_engine_config_t *config);

/**
 * @brief Reset engine state (clears queue, cycle, stats)
 * @param engine Pointer to engine context
 */
void ble_engine_reset(ble_engine_t *engine);

/**
 * @brief Advance the engine by one discovery slot
 * @param engine Pointer to engine context
 * @param now_ms Current time in milliseconds
 */
void ble_engine_tick(ble_engine_t *engine, uint32_t now_ms);

/**
 * @brief Push a received discovery packet into the engine
 * @param engine Pointer to engine context
 * @param packet Received discovery packet
 * @param rssi RSSI (dBm) for neighbor tracking
 * @param now_ms Timestamp in milliseconds
 * @return true if packet accepted/queued
 */
bool ble_engine_receive_packet(ble_engine_t *engine,
                               const ble_discovery_packet_t *packet,
                               int8_t rssi,
                               uint32_t now_ms);

/**
 * @brief Update measured noise/crowding level
 * @param engine Pointer to engine context
 * @param noise_level Noise value (>=0)
 */
void ble_engine_set_noise_level(ble_engine_t *engine, double noise_level);

/**
 * @brief Mark that another candidate announcement was heard
 * @param engine Pointer to engine context
 */
void ble_engine_mark_candidate_heard(ble_engine_t *engine);

/**
 * @brief Update current crowding factor (0.0 - 1.0)
 * @param engine Pointer to engine context
 * @param crowding_factor New crowding factor
 */
void ble_engine_set_crowding_factor(ble_engine_t *engine, double crowding_factor);

/**
 * @brief Seed forwarding RNG (useful for deterministic testing)
 * @param seed Seed value (0 selects default)
 */
void ble_engine_seed_random(uint32_t seed);

/**
 * @brief Update node GPS
 * @param engine Pointer to engine context
 * @param x Latitude (meters or consistent coordinate)
 * @param y Longitude
 * @param z Altitude
 * @param valid Whether GPS is available
 */
void ble_engine_set_gps(ble_engine_t *engine,
                        double x,
                        double y,
                        double z,
                        bool valid);

/**
 * @brief Access underlying node (read-only)
 * @param engine Pointer to engine context
 * @return Pointer to node structure
 */
const ble_mesh_node_t* ble_engine_get_node(const ble_engine_t *engine);

#ifdef __cplusplus
}
#endif

#endif 
