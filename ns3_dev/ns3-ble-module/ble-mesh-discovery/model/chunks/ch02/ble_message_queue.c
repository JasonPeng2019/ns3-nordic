/**
 * @file ble_message_queue.c
 * @brief Pure C implementation of BLE mesh discovery message queue
 */

#include "ble_message_queue.h"
#include <string.h>

void
ble_queue_init(ble_message_queue_t *queue)
{
    if (!queue) {
        return;
    }

    memset(queue, 0, sizeof(ble_message_queue_t));
    queue->size = 0;
    queue->seen_count = 0;
    queue->total_enqueued = 0;
    queue->total_dequeued = 0;
    queue->total_duplicates = 0;
    queue->total_loops = 0;
    queue->total_overflows = 0;
}

bool
ble_queue_enqueue(ble_message_queue_t *queue,
                  const ble_discovery_packet_t *packet,
                  uint32_t node_id,
                  uint32_t current_time_ms)
{
    if (!queue || !packet) {
        return false;
    }

    /* Check for loop (this node already in path) */
    if (ble_queue_is_in_path(packet, node_id)) {
        queue->total_loops++;
        return false;
    }

    /* Check for duplicate */
    uint64_t message_id = ble_queue_generate_message_id(packet);
    if (ble_queue_has_seen(queue, packet->sender_id, message_id)) {
        queue->total_duplicates++;
        return false;
    }

    /* Check queue size limit */
    if (queue->size >= BLE_QUEUE_MAX_SIZE) {
        queue->total_overflows++;
        return false;
    }

    /* Find first free slot */
    uint32_t slot = 0;
    for (slot = 0; slot < BLE_QUEUE_MAX_SIZE; slot++) {
        if (!queue->messages[slot].valid) {
            break;
        }
    }

    if (slot >= BLE_QUEUE_MAX_SIZE) {
        queue->total_overflows++;
        return false;
    }

    /* Add message to queue */
    ble_election_packet_t *dst = &queue->messages[slot].packet;
    memset(dst, 0, sizeof(*dst));
    if (packet->message_type == BLE_MSG_ELECTION_ANNOUNCEMENT) {
        const ble_election_packet_t *election_packet =
            (const ble_election_packet_t *)packet;
        memcpy(dst, election_packet, sizeof(ble_election_packet_t));
    } else {
        memcpy(&dst->base, packet, sizeof(ble_discovery_packet_t));
    }
    queue->messages[slot].received_time_ms = current_time_ms;
    queue->messages[slot].priority = ble_queue_calculate_priority(&dst->base);
    queue->messages[slot].valid = true;
    queue->size++;

    /* Mark as seen */
    if (queue->seen_count < BLE_SEEN_CACHE_SIZE) {
        uint32_t seen_slot = 0;
        for (seen_slot = 0; seen_slot < BLE_SEEN_CACHE_SIZE; seen_slot++) {
            if (!queue->seen_cache[seen_slot].valid) {
                queue->seen_cache[seen_slot].sender_id = packet->sender_id;
                queue->seen_cache[seen_slot].message_id = message_id;
                queue->seen_cache[seen_slot].seen_time_ms = current_time_ms;
                queue->seen_cache[seen_slot].valid = true;
                queue->seen_count++;
                break;
            }
        }
    }

    queue->total_enqueued++;
    return true;
}

bool
ble_queue_dequeue(ble_message_queue_t *queue, ble_election_packet_t *packet)
{
    if (!queue || !packet || queue->size == 0) {
        return false;
    }

    /* Find highest priority message (lowest priority value) */
    uint32_t best_slot = 0;
    uint8_t best_priority = 255;
    bool found = false;

    for (uint32_t i = 0; i < BLE_QUEUE_MAX_SIZE; i++) {
        if (queue->messages[i].valid) {
            if (!found || queue->messages[i].priority < best_priority) {
                best_slot = i;
                best_priority = queue->messages[i].priority;
                found = true;
            }
        }
    }

    if (!found) {
        return false;
    }

    /* Copy message and mark slot as free */
    memcpy(packet, &queue->messages[best_slot].packet, sizeof(ble_election_packet_t));
    queue->messages[best_slot].valid = false;
    queue->size--;
    queue->total_dequeued++;

    return true;
}

bool
ble_queue_peek(const ble_message_queue_t *queue, ble_election_packet_t *packet)
{
    if (!queue || !packet || queue->size == 0) {
        return false;
    }

    /* Find highest priority message (lowest priority value) */
    uint32_t best_slot = 0;
    uint8_t best_priority = 255;
    bool found = false;

    for (uint32_t i = 0; i < BLE_QUEUE_MAX_SIZE; i++) {
        if (queue->messages[i].valid) {
            if (!found || queue->messages[i].priority < best_priority) {
                best_slot = i;
                best_priority = queue->messages[i].priority;
                found = true;
            }
        }
    }

    if (!found) {
        return false;
    }

    /* Copy message without removing from queue */
    memcpy(packet, &queue->messages[best_slot].packet, sizeof(ble_election_packet_t));
    return true;
}

bool
ble_queue_is_empty(const ble_message_queue_t *queue)
{
    if (!queue) {
        return true;
    }
    return queue->size == 0;
}

uint32_t
ble_queue_get_size(const ble_message_queue_t *queue)
{
    if (!queue) {
        return 0;
    }
    return queue->size;
}

void
ble_queue_clear(ble_message_queue_t *queue)
{
    if (!queue) {
        return;
    }

    for (uint32_t i = 0; i < BLE_QUEUE_MAX_SIZE; i++) {
        queue->messages[i].valid = false;
    }

    for (uint32_t i = 0; i < BLE_SEEN_CACHE_SIZE; i++) {
        queue->seen_cache[i].valid = false;
    }

    queue->size = 0;
    queue->seen_count = 0;
}

bool
ble_queue_has_seen(const ble_message_queue_t *queue,
                   uint32_t sender_id,
                   uint64_t message_id)
{
    if (!queue) {
        return false;
    }

    for (uint32_t i = 0; i < BLE_SEEN_CACHE_SIZE; i++) {
        if (queue->seen_cache[i].valid &&
            queue->seen_cache[i].sender_id == sender_id &&
            queue->seen_cache[i].message_id == message_id) {
            return true;
        }
    }

    return false;
}

bool
ble_queue_is_in_path(const ble_discovery_packet_t *packet, uint32_t node_id)
{
    if (!packet) {
        return false;
    }

    return ble_discovery_is_in_path(packet, node_id);
}

uint64_t
ble_queue_generate_message_id(const ble_discovery_packet_t *packet)
{
    if (!packet) {
        return 0;
    }

    /* Simple hash: combine sender ID and TTL */
    /* In production, you might want to include more fields or use a better hash */
    uint64_t id = ((uint64_t)packet->sender_id << 32) | ((uint64_t)packet->ttl);

    return id;
}

uint8_t
ble_queue_calculate_priority(const ble_discovery_packet_t *packet)
{
    if (!packet) {
        return 255; /* Lowest priority */
    }

    /* Higher TTL = higher priority (lower priority value) */
    /* Priority range: 0 (highest) to 255 (lowest) */
    uint8_t ttl = packet->ttl;

    if (ttl == 0) {
        return 255; /* Lowest priority for expired messages */
    }

    /* Invert TTL: higher TTL gets lower priority number (higher actual priority) */
    return 255 - ttl;
}

void
ble_queue_clean_old_entries(ble_message_queue_t *queue,
                             uint32_t current_time_ms,
                             uint32_t max_age_ms)
{
    if (!queue) {
        return;
    }

    uint32_t removed_count = 0;

    for (uint32_t i = 0; i < BLE_SEEN_CACHE_SIZE; i++) {
        if (queue->seen_cache[i].valid) {
            uint32_t age_ms = current_time_ms - queue->seen_cache[i].seen_time_ms;
            if (age_ms > max_age_ms) {
                queue->seen_cache[i].valid = false;
                queue->seen_count--;
                removed_count++;
            }
        }
    }
}

void
ble_queue_get_statistics(const ble_message_queue_t *queue,
                          uint32_t *total_enqueued,
                          uint32_t *total_dequeued,
                          uint32_t *total_duplicates,
                          uint32_t *total_loops,
                          uint32_t *total_overflows)
{
    if (!queue) {
        return;
    }

    if (total_enqueued) {
        *total_enqueued = queue->total_enqueued;
    }
    if (total_dequeued) {
        *total_dequeued = queue->total_dequeued;
    }
    if (total_duplicates) {
        *total_duplicates = queue->total_duplicates;
    }
    if (total_loops) {
        *total_loops = queue->total_loops;
    }
    if (total_overflows) {
        *total_overflows = queue->total_overflows;
    }
}
