/**
 * @file ble_message_queue.h
 * @brief Pure C implementation of BLE mesh discovery message queue
 *
 * This is a portable C implementation that can be used in both simulation (NS-3)
 * and embedded systems (ARM Cortex-M, ESP32, nRF52, etc.)
 *
 * Features:
 * - Message deduplication
 * - PSF loop detection
 * - Priority-based queuing (TTL-based)
 * - Fixed-size queue with overflow handling
 *
 * NO dependencies on NS-3 or C++ standard library.
 */

#ifndef BLE_MESSAGE_QUEUE_H
#define BLE_MESSAGE_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ble_discovery_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration constants */
#define BLE_QUEUE_MAX_SIZE 100           /**< Maximum number of queued messages */
#define BLE_SEEN_CACHE_SIZE 200          /**< Maximum size of seen messages cache */

/**
 * @brief Queued message entry
 */
typedef struct {
    ble_election_packet_t packet;        /**< Packet data (discovery or election) */
    uint32_t received_time_ms;           /**< When message was received (ms) */
    uint8_t priority;                    /**< Priority for forwarding (lower = higher priority) */
    bool valid;                          /**< Whether this slot is occupied */
} ble_queued_message_t;

/**
 * @brief Seen message entry for deduplication
 */
typedef struct {
    uint32_t sender_id;                  /**< Sender ID */
    uint64_t message_id;                 /**< Unique message ID (hash) */
    uint32_t seen_time_ms;               /**< When message was seen (ms) */
    bool valid;                          /**< Whether this slot is occupied */
} ble_seen_message_t;

/**
 * @brief Message queue structure
 */
typedef struct {
    ble_queued_message_t messages[BLE_QUEUE_MAX_SIZE]; /**< Queue array */
    uint32_t size;                       /**< Current number of messages in queue */

    ble_seen_message_t seen_cache[BLE_SEEN_CACHE_SIZE]; /**< Seen messages cache */
    uint32_t seen_count;                 /**< Number of entries in seen cache */

    /* Statistics */
    uint32_t total_enqueued;             /**< Total messages enqueued */
    uint32_t total_dequeued;             /**< Total messages dequeued */
    uint32_t total_duplicates;           /**< Total duplicates rejected */
    uint32_t total_loops;                /**< Total loops detected */
    uint32_t total_overflows;            /**< Total overflows */
} ble_message_queue_t;

/**
 * @brief Initialize message queue
 * @param queue Pointer to queue structure
 */
void ble_queue_init(ble_message_queue_t *queue);

/**
 * @brief Add a message to the queue
 * @param queue Pointer to queue structure
 * @param packet Pointer to discovery packet
 * @param node_id This node's ID (for loop detection)
 * @param current_time_ms Current time in milliseconds
 * @return true if message was added, false if rejected (duplicate/loop/overflow)
 */
bool ble_queue_enqueue(ble_message_queue_t *queue,
                       const ble_discovery_packet_t *packet,
                       uint32_t node_id,
                       uint32_t current_time_ms);

/**
 * @brief Get the next highest-priority message from queue
 * @param queue Pointer to queue structure
 * @param packet Pointer to store dequeued packet
 * @return true if message was dequeued, false if queue empty
 */
bool ble_queue_dequeue(ble_message_queue_t *queue, ble_election_packet_t *packet);

/**
 * @brief Peek at the highest-priority message without removing it
 * @param queue Pointer to queue structure
 * @param packet Pointer to store peeked packet
 * @return true if message exists, false if queue empty
 */
bool ble_queue_peek(const ble_message_queue_t *queue, ble_election_packet_t *packet);

/**
 * @brief Check if queue is empty
 * @param queue Pointer to queue structure
 * @return true if queue is empty
 */
bool ble_queue_is_empty(const ble_message_queue_t *queue);

/**
 * @brief Get number of messages in queue
 * @param queue Pointer to queue structure
 * @return Number of messages
 */
uint32_t ble_queue_get_size(const ble_message_queue_t *queue);

/**
 * @brief Clear all messages from queue
 * @param queue Pointer to queue structure
 */
void ble_queue_clear(ble_message_queue_t *queue);

/**
 * @brief Check if a message has been seen before (deduplication)
 * @param queue Pointer to queue structure
 * @param sender_id Sender ID from message
 * @param message_id Message ID (hash)
 * @return true if message was seen before
 */
bool ble_queue_has_seen(const ble_message_queue_t *queue,
                        uint32_t sender_id,
                        uint64_t message_id);

/**
 * @brief Check if this node is in the message's path (loop detection)
 * @param packet Pointer to discovery packet
 * @param node_id This node's ID
 * @return true if node is in path (would create loop)
 */
bool ble_queue_is_in_path(const ble_discovery_packet_t *packet, uint32_t node_id);

/**
 * @brief Generate a unique message ID for deduplication
 * @param packet Pointer to discovery packet
 * @return Unique message ID (hash)
 */
uint64_t ble_queue_generate_message_id(const ble_discovery_packet_t *packet);

/**
 * @brief Calculate priority for a message (based on TTL)
 * @param packet Pointer to discovery packet
 * @return Priority value (lower = higher priority)
 */
uint8_t ble_queue_calculate_priority(const ble_discovery_packet_t *packet);

/**
 * @brief Clean old entries from seen messages cache
 * @param queue Pointer to queue structure
 * @param current_time_ms Current time in milliseconds
 * @param max_age_ms Maximum age for cached entries (milliseconds)
 */
void ble_queue_clean_old_entries(ble_message_queue_t *queue,
                                  uint32_t current_time_ms,
                                  uint32_t max_age_ms);

/**
 * @brief Get queue statistics
 * @param queue Pointer to queue structure
 * @param total_enqueued Pointer to store total enqueued count
 * @param total_dequeued Pointer to store total dequeued count
 * @param total_duplicates Pointer to store total duplicates count
 * @param total_loops Pointer to store total loops count
 * @param total_overflows Pointer to store total overflows count
 */
void ble_queue_get_statistics(const ble_message_queue_t *queue,
                               uint32_t *total_enqueued,
                               uint32_t *total_dequeued,
                               uint32_t *total_duplicates,
                               uint32_t *total_loops,
                               uint32_t *total_overflows);

#ifdef __cplusplus
}
#endif

#endif /* BLE_MESSAGE_QUEUE_H */
