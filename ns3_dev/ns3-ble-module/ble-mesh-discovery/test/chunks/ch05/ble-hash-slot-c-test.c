#include <assert.h>
#include <stdio.h>
#include <stdint.h>

/* Hash/slot mapping regression test (portable C). */
#include "../model/protocol-core/ble_discovery_packet.h"

static void test_hash_map_basic(void)
{
    uint32_t slot = 0;
    uint32_t channel = 0;
    bool ok = ble_hash_map_to_slot(0x12345678, 8, 3, &slot, &channel);
    assert(ok);
    assert(slot < 8);
    assert(channel < 3);
}

static void test_next_slot_time_monotonic(void)
{
    uint32_t slot = 0;
    uint32_t channel = 0;
    bool ok = ble_hash_map_to_slot(0xABCDEF01, 4, 2, &slot, &channel);
    assert(ok);
    uint64_t first = ble_hash_next_slot_time_ms(100, 40, 4, slot);
    uint64_t second = ble_hash_next_slot_time_ms(first + 1, 40, 4, slot);
    assert(second >= first + 1);
}

static void test_edge_slot_determinism(void)
{
    uint32_t slot1 = 0, chan1 = 0;
    uint32_t slot2 = 0, chan2 = 0;
    bool ok1 = ble_hash_map_edge_slot(0xAAAA5555, 42, 8, 2, &slot1, &chan1);
    bool ok2 = ble_hash_map_edge_slot(0xAAAA5555, 42, 8, 2, &slot2, &chan2);
    assert(ok1 && ok2);
    assert(slot1 == slot2);
    assert(chan1 == chan2);

    uint32_t slot3 = 0, chan3 = 0;
    bool ok3 = ble_hash_map_edge_slot(0xAAAA5555, 43, 8, 2, &slot3, &chan3);
    assert(ok3);
    /* Different edge ID should usually produce a different bucket */
    assert(slot3 != slot1 || chan3 != chan1);
}

int main(void)
{
    test_hash_map_basic();
    test_next_slot_time_monotonic();
    test_edge_slot_determinism();
    printf("ble-hash-slot-c-test passed\\n");
    return 0;
}
