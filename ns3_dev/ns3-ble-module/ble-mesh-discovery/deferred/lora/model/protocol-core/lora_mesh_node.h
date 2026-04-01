/**
 * @file lora_mesh_node.h
 * @brief Placeholder LoRA node state machine (discovery/election scaffolding)
 */

#ifndef LORA_MESH_NODE_H
#define LORA_MESH_NODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "lora_discovery_packet.h"

typedef enum {
    LORA_NODE_STATE_INIT = 0,
    LORA_NODE_STATE_DISCOVERY = 1,
    LORA_NODE_STATE_EDGE = 2,
    LORA_NODE_STATE_CLUSTERHEAD_CANDIDATE = 3,
    LORA_NODE_STATE_CLUSTERHEAD = 4
} lora_node_state_t;

typedef struct {
    uint32_t node_id;
    lora_node_state_t state;
    uint8_t channel;
    uint8_t home_channel;
    uint8_t target_channel;
    uint32_t clusterhead_id;
    bool is_clusterhead;
    uint8_t votes_heard;
    double crowding_factor;
    uint32_t current_cycle;
} lora_mesh_node_t;

void lora_mesh_node_init(lora_mesh_node_t *node, uint32_t node_id);
void lora_mesh_node_set_channel(lora_mesh_node_t *node, uint8_t channel);
void lora_mesh_node_set_crowding(lora_mesh_node_t *node, double crowding);
bool lora_mesh_node_set_state(lora_mesh_node_t *node, lora_node_state_t new_state);
const char *lora_mesh_node_state_name(lora_node_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* LORA_MESH_NODE_H */
