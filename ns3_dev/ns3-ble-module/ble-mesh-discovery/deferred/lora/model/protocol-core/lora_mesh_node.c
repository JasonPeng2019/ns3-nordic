/**
 * @file lora_mesh_node.c
 * @brief Placeholder LoRA node state machine (discovery/election scaffolding)
 */

#include "lora_mesh_node.h"
#include <string.h>

static bool
lora_is_valid_transition(lora_node_state_t current, lora_node_state_t next)
{
    if (current == next) {
        return true;
    }
    switch (current) {
        case LORA_NODE_STATE_INIT:
            return next == LORA_NODE_STATE_DISCOVERY;
        case LORA_NODE_STATE_DISCOVERY:
            return next == LORA_NODE_STATE_EDGE || next == LORA_NODE_STATE_CLUSTERHEAD_CANDIDATE;
        case LORA_NODE_STATE_EDGE:
            return next == LORA_NODE_STATE_CLUSTERHEAD_CANDIDATE || next == LORA_NODE_STATE_CLUSTERHEAD;
        case LORA_NODE_STATE_CLUSTERHEAD_CANDIDATE:
            return next == LORA_NODE_STATE_CLUSTERHEAD || next == LORA_NODE_STATE_EDGE;
        case LORA_NODE_STATE_CLUSTERHEAD:
            return next == LORA_NODE_STATE_EDGE;
        default:
            return false;
    }
}

void
lora_mesh_node_init(lora_mesh_node_t *node, uint32_t node_id)
{
    if (!node) {
        return;
    }
    memset(node, 0, sizeof(*node));
    node->node_id = node_id;
    node->state = LORA_NODE_STATE_INIT;
    node->channel = 1;
    node->home_channel = 1;
    node->target_channel = 1;
    node->clusterhead_id = 0;
    node->is_clusterhead = false;
    node->votes_heard = 0;
    node->crowding_factor = 0.0;
    node->current_cycle = 0;
}

void
lora_mesh_node_set_channel(lora_mesh_node_t *node, uint8_t channel)
{
    if (!node) {
        return;
    }
    node->channel = channel;
}

void
lora_mesh_node_set_crowding(lora_mesh_node_t *node, double crowding)
{
    if (!node) {
        return;
    }
    if (crowding < 0.0) {
        crowding = 0.0;
    }
    if (crowding > 1.0) {
        crowding = 1.0;
    }
    node->crowding_factor = crowding;
}

bool
lora_mesh_node_set_state(lora_mesh_node_t *node, lora_node_state_t new_state)
{
    if (!node) {
        return false;
    }
    if (!lora_is_valid_transition(node->state, new_state)) {
        return false;
    }
    node->state = new_state;
    return true;
}

const char *
lora_mesh_node_state_name(lora_node_state_t state)
{
    switch (state) {
        case LORA_NODE_STATE_INIT: return "INIT";
        case LORA_NODE_STATE_DISCOVERY: return "DISCOVERY";
        case LORA_NODE_STATE_EDGE: return "EDGE";
        case LORA_NODE_STATE_CLUSTERHEAD_CANDIDATE: return "CH_CANDIDATE";
        case LORA_NODE_STATE_CLUSTERHEAD: return "CLUSTERHEAD";
        default: return "UNKNOWN";
    }
}
