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
#include "ble_broadcast_timing.h"
#include "ble_discovery_cycle.h"
#include "ble_discovery_packet.h"
#include "ble_election.h"
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
 * @brief Optional callback when connectivity metrics are updated
 * @param metrics Latest snapshot
 * @param user_context User context pointer
 */
typedef void (*ble_engine_metrics_callback)(const ble_connectivity_metrics_t *metrics,
                                            void *user_context);

struct ble_engine_slot_event_s;
typedef void (*ble_engine_slot_callback)(const struct ble_engine_slot_event_s *evt,
                                         void *user_context);

#define BLE_ENGINE_DEFAULT_NOISE_SLOTS 10U
#define BLE_ENGINE_DEFAULT_NOISE_SLOT_DURATION_MS 200U
#define BLE_ENGINE_DEFAULT_NEIGHBOR_SLOTS 200U
#define BLE_ENGINE_DEFAULT_NEIGHBOR_SLOT_DURATION_MS 10U
#define BLE_ENGINE_DEFAULT_NEIGHBOR_TIMEOUT_CYCLES 3U
#define BLE_ENGINE_MAX_ELECTION_ROUNDS 3U
#define BLE_ENGINE_DEFAULT_TDMA_SLOTS 8U
#define BLE_ENGINE_DEFAULT_FDMA_CHANNELS 1U
#define BLE_ENGINE_DEFAULT_FRAME_DURATION_MS 100U
#define BLE_ENGINE_DEFAULT_MODE_DURATION_MS 1500U

/**
 * @brief Engine configuration (static parameters + callbacks)
 */
typedef struct {
    uint32_t node_id;                  /**< Unique node identifier */
    uint32_t slot_duration_ms;         /**< Duration of each discovery slot */
    uint8_t initial_ttl;               /**< TTL for locally-originated messages */
    double proximity_threshold;        /**< GPS proximity threshold (meters) */
    uint32_t noise_slot_count;         /**< Micro-slots in noisy RSSI phase */
    uint32_t noise_slot_duration_ms;   /**< Duration of each noisy micro-slot */
    uint32_t neighbor_slot_count;      /**< Micro-slots in direct neighbor phase */
    uint32_t neighbor_slot_duration_ms;/**< Duration of each neighbor micro-slot */
    uint32_t neighbor_timeout_cycles;  /**< Discovery cycles before a neighbor is stale */
    /* Slotting (hash-derived data phase) */
    uint32_t tdma_slots;               /**< TDMA slots per frame */
    uint32_t fdma_channels;            /**< FDMA channels */
    uint32_t frame_duration_ms;        /**< Frame duration (ms) */
    /* Mode placeholders (Compass pipeline, no behavior enforced) */
    uint32_t mode1_duration_ms;        /**< Mode1 duration (ms) */
    uint32_t mode2_duration_ms;        /**< Mode2 duration (ms) */
    bool enable_data_phase;            /**< Enable slot/data phase gating */
    bool enable_collision_model;       /**< Enable collision detection */
    ble_engine_send_callback send_cb;  /**< Packet transmission callback */
    ble_engine_log_callback log_cb;    /**< Optional logging callback */
    ble_engine_metrics_callback metrics_cb; /**< Optional metrics callback */
    ble_engine_slot_callback slot_cb;  /**< Optional slot event callback */
    void *user_context;                /**< Passed to callbacks */
} ble_engine_config_t;

typedef enum {
    BLE_ENGINE_PHASE_NOISY = 0,
    BLE_ENGINE_PHASE_NEIGHBOR = 1,
    BLE_ENGINE_PHASE_DISCOVERY = 2
} ble_engine_phase_t;

/**
 * @brief Data slot outcome (placeholder for future tracing)
 */
typedef enum {
    BLE_ENGINE_SLOT_OUTCOME_EMPTY = 0,
    BLE_ENGINE_SLOT_OUTCOME_TX = 1,
    BLE_ENGINE_SLOT_OUTCOME_RX = 2,
    BLE_ENGINE_SLOT_OUTCOME_COLLISION = 3
} ble_engine_slot_outcome_t;

/**
 * @brief Slot event metadata (data phase placeholder)
 */
typedef struct ble_engine_slot_event_s {
    uint32_t node_id;
    bool is_cluster_slot;
    uint32_t frame_index;
    uint32_t slot_index;
    uint32_t channel_index;
    uint8_t iteration; /**< 0..2 */
    ble_engine_slot_outcome_t outcome;
} ble_engine_slot_event_t;

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
    ble_election_state_t election;
    ble_election_packet_t election_packet;
    ble_election_packet_t renouncement_packet;
    ble_broadcast_timing_t noisy_timing;
    ble_broadcast_timing_t neighbor_timing;
    ble_engine_phase_t phase;
    uint32_t noisy_slots_completed;
    uint32_t neighbor_slots_completed;
    bool phase_listen_active;
    double crowding_factor;
    double proximity_threshold;
    uint32_t neighbor_timeout_cycles;
    /* Slotting (hash-derived data phase) */
    uint32_t tdma_slots;
    uint32_t fdma_channels;
    uint32_t frame_duration_ms;
    /* Mode placeholders (no enforcement yet) */
    uint32_t mode1_duration_ms;
    uint32_t mode2_duration_ms;
    uint32_t last_tick_time_ms;
    ble_connectivity_metrics_t last_metrics;
    uint8_t election_rounds_remaining;
    uint32_t last_election_cycle_sent;
    uint8_t renouncement_rounds_remaining;
    uint32_t last_renouncement_cycle_sent;
    uint16_t selected_clusterhead_hops;
    uint32_t selected_clusterhead_direct_connections;
    /* Data/slot phase placeholders */
    bool data_phase_active;
    uint32_t data_phase_start_ms;
    uint8_t data_slot_iteration;   /**< 0..2 for three iterations */
    uint32_t slots_tx;
    uint32_t slots_rx;
    uint32_t slots_collision;
    uint32_t slots_empty;
    /* Collision bookkeeping */
    bool last_slot_active;
    uint32_t last_slot_frame;
    uint32_t last_slot_index;
    uint32_t last_slot_channel;
    uint8_t last_slot_iteration;
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

/* ===== Data/slot phase placeholders ===== */

/**
 * @brief Start data phase (placeholder) and reset slot counters/iteration
 * @param engine Pointer to engine context
 * @param start_ms Absolute start time in ms
 */
void ble_engine_start_data_phase(ble_engine_t *engine, uint32_t start_ms);

/**
 * @brief End data phase (placeholder)
 * @param engine Pointer to engine context
 */
void ble_engine_end_data_phase(ble_engine_t *engine);

/**
 * @brief Record a slot event outcome (TX/RX/COLLISION/EMPTY) for metrics
 * @param engine Pointer to engine context
 * @param outcome Slot outcome enum
 */
/**
 * @brief Record a slot event if current time falls in the node's slot
 * @param engine Pointer to engine context
 * @param use_cluster_slot True to use cluster slot; false for self slot
 * @param outcome Outcome to record
 * @param now_ms Current time in milliseconds
 */
void ble_engine_record_slot_event(ble_engine_t *engine,
                                  bool use_cluster_slot,
                                  ble_engine_slot_outcome_t outcome,
                                  uint32_t now_ms);

/**
 * @brief Advance slot iteration counter (0..2) for three-iteration window
 * @param engine Pointer to engine context
 */
void ble_engine_advance_slot_iteration(ble_engine_t *engine);

/**
 * @brief Gate and record a slot event; returns true if within assigned slot
 * @param engine Pointer to engine context
 * @param use_cluster_slot True to use cluster slot; false for self slot
 * @param now_ms Current time in milliseconds
 * @param is_tx True if TX, false if RX
 * @return outcome recorded (EMPTY if not in slot)
 */
ble_engine_slot_outcome_t ble_engine_gate_and_record_slot(ble_engine_t *engine,
                                                          bool use_cluster_slot,
                                                          uint32_t now_ms,
                                                          bool is_tx);

/**
 * @brief Access underlying node (read-only)
 * @param engine Pointer to engine context
 * @return Pointer to node structure
 */
const ble_mesh_node_t* ble_engine_get_node(const ble_engine_t *engine);

#ifdef __cplusplus
}
#endif

#endif /* BLE_DISCOVERY_ENGINE_H */
