/**
 * @file ble-discovery-packet-c-test.c
 * @brief Comprehensive C-level unit tests for BLE Discovery Protocol core
 * @author Benjamin Huh
 * @date 2025-11-21
 *
 * These tests validate the pure C implementation independently of NS-3.
 * This file can be compiled and tested standalone on embedded systems.
 */

#include "../model/protocol-core/ble_discovery_packet.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>

/* Test counter */
static int tests_passed = 0;
static int tests_failed = 0;

/* Test helper macros */
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
        } else { \
            tests_failed++; \
            printf("FAIL: %s (line %d): %s\n", __func__, __LINE__, message); \
        } \
    } while(0)

#define TEST_ASSERT_EQ(actual, expected, message) \
    TEST_ASSERT((actual) == (expected), message)

#define TEST_ASSERT_DOUBLE_EQ(actual, expected, message) \
    TEST_ASSERT(fabs((actual) - (expected)) < 1e-9, message)

/**
 * Test: Packet initialization
 */
void test_packet_init(void)
{
    ble_discovery_packet_t packet;
    ble_discovery_packet_init(&packet);

    TEST_ASSERT_EQ(packet.message_type, BLE_MSG_DISCOVERY, "Default message type should be DISCOVERY");
    TEST_ASSERT_EQ(packet.sender_id, 0, "Default sender ID should be 0");
    TEST_ASSERT_EQ(packet.ttl, BLE_DISCOVERY_DEFAULT_TTL, "Default TTL should be 10");
    TEST_ASSERT_EQ(packet.path_length, 0, "Default path length should be 0");
    TEST_ASSERT_EQ(packet.gps_available, false, "Default GPS availability should be false");
}

/**
 * Test: Election packet initialization
 */
void test_election_init(void)
{
    ble_election_packet_t packet;
    ble_election_packet_init(&packet);

    TEST_ASSERT_EQ(packet.base.message_type, BLE_MSG_ELECTION_ANNOUNCEMENT,
                   "Election message type should be ELECTION_ANNOUNCEMENT");
    TEST_ASSERT_EQ(packet.election.class_id, 0, "Default class ID should be 0");
    TEST_ASSERT_EQ(packet.election.pdsf, 0, "Default PDSF should be 0");
    TEST_ASSERT_DOUBLE_EQ(packet.election.score, 0.0, "Default score should be 0.0");
    TEST_ASSERT_EQ(packet.election.hash, 0, "Default hash should be 0");
}

/**
 * Test: TTL operations
 */
void test_ttl_operations(void)
{
    ble_discovery_packet_t packet;
    ble_discovery_packet_init(&packet);

    // Set TTL to 3
    packet.ttl = 3;

    // Decrement 1: should return true, TTL = 2
    bool result = ble_discovery_decrement_ttl(&packet);
    TEST_ASSERT_EQ(result, true, "First decrement should return true");
    TEST_ASSERT_EQ(packet.ttl, 2, "TTL should be 2 after first decrement");

    // Decrement 2: should return true, TTL = 1
    result = ble_discovery_decrement_ttl(&packet);
    TEST_ASSERT_EQ(result, true, "Second decrement should return true");
    TEST_ASSERT_EQ(packet.ttl, 1, "TTL should be 1 after second decrement");

    // Decrement 3: should return true, TTL = 0
    result = ble_discovery_decrement_ttl(&packet);
    TEST_ASSERT_EQ(result, true, "Third decrement should return true");
    TEST_ASSERT_EQ(packet.ttl, 0, "TTL should be 0 after third decrement");

    // Decrement 4: should return false, TTL = 0
    result = ble_discovery_decrement_ttl(&packet);
    TEST_ASSERT_EQ(result, false, "Fourth decrement should return false");
    TEST_ASSERT_EQ(packet.ttl, 0, "TTL should still be 0");
}

/**
 * Test: Path operations
 */
void test_path_operations(void)
{
    ble_discovery_packet_t packet;
    ble_discovery_packet_init(&packet);

    // Add nodes to path
    bool result = ble_discovery_add_to_path(&packet, 101);
    TEST_ASSERT_EQ(result, true, "Should add first node");
    TEST_ASSERT_EQ(packet.path_length, 1, "Path length should be 1");
    TEST_ASSERT_EQ(packet.path[0], 101, "First node should be 101");

    result = ble_discovery_add_to_path(&packet, 102);
    TEST_ASSERT_EQ(result, true, "Should add second node");
    TEST_ASSERT_EQ(packet.path_length, 2, "Path length should be 2");

    result = ble_discovery_add_to_path(&packet, 103);
    TEST_ASSERT_EQ(result, true, "Should add third node");
    TEST_ASSERT_EQ(packet.path_length, 3, "Path length should be 3");

    // Check loop detection
    TEST_ASSERT_EQ(ble_discovery_is_in_path(&packet, 101), true, "Node 101 should be in path");
    TEST_ASSERT_EQ(ble_discovery_is_in_path(&packet, 102), true, "Node 102 should be in path");
    TEST_ASSERT_EQ(ble_discovery_is_in_path(&packet, 103), true, "Node 103 should be in path");
    TEST_ASSERT_EQ(ble_discovery_is_in_path(&packet, 999), false, "Node 999 should not be in path");
}

/**
 * Test: Path overflow protection
 */
void test_path_overflow(void)
{
    ble_discovery_packet_t packet;
    ble_discovery_packet_init(&packet);

    // Fill path to maximum
    for (uint16_t i = 0; i < BLE_DISCOVERY_MAX_PATH_LENGTH; i++)
    {
        bool result = ble_discovery_add_to_path(&packet, i);
        TEST_ASSERT_EQ(result, true, "Should add node to path");
    }

    TEST_ASSERT_EQ(packet.path_length, BLE_DISCOVERY_MAX_PATH_LENGTH,
                   "Path should be at maximum length");

    // Try to add one more (should fail)
    bool result = ble_discovery_add_to_path(&packet, 999);
    TEST_ASSERT_EQ(result, false, "Should not add node when path is full");
    TEST_ASSERT_EQ(packet.path_length, BLE_DISCOVERY_MAX_PATH_LENGTH,
                   "Path length should not change");
}

/**
 * Test: GPS operations
 */
void test_gps_operations(void)
{
    ble_discovery_packet_t packet;
    ble_discovery_packet_init(&packet);

    // Set GPS location
    ble_discovery_set_gps(&packet, 37.7749, -122.4194, 50.0);

    TEST_ASSERT_EQ(packet.gps_available, true, "GPS should be available after setting");
    TEST_ASSERT_DOUBLE_EQ(packet.gps_location.x, 37.7749, "GPS X should match");
    TEST_ASSERT_DOUBLE_EQ(packet.gps_location.y, -122.4194, "GPS Y should match");
    TEST_ASSERT_DOUBLE_EQ(packet.gps_location.z, 50.0, "GPS Z should match");
}

/**
 * Test: Discovery packet serialization/deserialization
 */
void test_discovery_serialization(void)
{
    ble_discovery_packet_t original, deserialized;
    ble_discovery_packet_init(&original);

    // Configure packet
    original.sender_id = 12345;
    original.ttl = 7;
    ble_discovery_add_to_path(&original, 1);
    ble_discovery_add_to_path(&original, 2);
    ble_discovery_add_to_path(&original, 3);
    ble_discovery_set_gps(&original, 10.5, 20.5, 30.5);

    // Calculate size
    uint32_t size = ble_discovery_get_size(&original);
    TEST_ASSERT(size > 0, "Serialized size should be > 0");

    // Serialize
    uint8_t buffer[256];
    uint32_t bytes_written = ble_discovery_serialize(&original, buffer, sizeof(buffer));
    TEST_ASSERT_EQ(bytes_written, size, "Bytes written should match calculated size");

    // Deserialize
    ble_discovery_packet_init(&deserialized);
    uint32_t bytes_read = ble_discovery_deserialize(&deserialized, buffer, bytes_written);
    TEST_ASSERT_EQ(bytes_read, bytes_written, "Bytes read should match bytes written");

    // Verify fields
    TEST_ASSERT_EQ(deserialized.message_type, original.message_type, "Message type should match");
    TEST_ASSERT_EQ(deserialized.sender_id, original.sender_id, "Sender ID should match");
    TEST_ASSERT_EQ(deserialized.ttl, original.ttl, "TTL should match");
    TEST_ASSERT_EQ(deserialized.path_length, original.path_length, "Path length should match");

    for (uint16_t i = 0; i < original.path_length; i++)
    {
        TEST_ASSERT_EQ(deserialized.path[i], original.path[i], "Path node should match");
    }

    TEST_ASSERT_EQ(deserialized.gps_available, original.gps_available, "GPS availability should match");
    TEST_ASSERT_DOUBLE_EQ(deserialized.gps_location.x, original.gps_location.x, "GPS X should match");
    TEST_ASSERT_DOUBLE_EQ(deserialized.gps_location.y, original.gps_location.y, "GPS Y should match");
    TEST_ASSERT_DOUBLE_EQ(deserialized.gps_location.z, original.gps_location.z, "GPS Z should match");
}

/**
 * Test: Discovery packet serialization without GPS
 */
void test_discovery_serialization_no_gps(void)
{
    ble_discovery_packet_t original, deserialized;
    ble_discovery_packet_init(&original);

    // Configure packet (no GPS)
    original.sender_id = 54321;
    original.ttl = 5;
    ble_discovery_add_to_path(&original, 10);
    ble_discovery_add_to_path(&original, 20);

    // Calculate size (should be smaller without GPS)
    uint32_t size = ble_discovery_get_size(&original);
    uint32_t expected_size = 1 + 4 + 1 + 2 + (2 * 4) + 1; // no GPS coords
    TEST_ASSERT_EQ(size, expected_size, "Size should not include GPS coordinates");

    // Serialize
    uint8_t buffer[256];
    uint32_t bytes_written = ble_discovery_serialize(&original, buffer, sizeof(buffer));
    TEST_ASSERT_EQ(bytes_written, size, "Bytes written should match calculated size");

    // Deserialize
    uint32_t bytes_read = ble_discovery_deserialize(&deserialized, buffer, bytes_written);
    TEST_ASSERT_EQ(bytes_read, bytes_written, "Bytes read should match bytes written");

    // Verify GPS is not available
    TEST_ASSERT_EQ(deserialized.gps_available, false, "GPS should not be available");
}

/**
 * Test: Election packet serialization/deserialization
 */
void test_election_serialization(void)
{
    ble_election_packet_t original, deserialized;
    ble_election_packet_init(&original);

    // Configure packet
    original.base.sender_id = 67890;
    original.base.ttl = 8;
    ble_discovery_add_to_path(&original.base, 5);
    ble_discovery_add_to_path(&original.base, 6);
    ble_discovery_set_gps(&original.base, 40.7, -74.0, 10.0);

    original.election.class_id = 42;
    original.election.pdsf = 150;
    original.election.score = 0.87;
    original.election.hash = 0xDEADBEEF;

    // Calculate size
    uint32_t size = ble_election_get_size(&original);
    TEST_ASSERT(size > ble_discovery_get_size(&original.base),
                "Election size should be larger than base discovery size");

    // Serialize
    uint8_t buffer[512];
    uint32_t bytes_written = ble_election_serialize(&original, buffer, sizeof(buffer));
    TEST_ASSERT_EQ(bytes_written, size, "Bytes written should match calculated size");

    // Deserialize
    ble_election_packet_init(&deserialized);
    uint32_t bytes_read = ble_election_deserialize(&deserialized, buffer, bytes_written);
    TEST_ASSERT_EQ(bytes_read, bytes_written, "Bytes read should match bytes written");

    // Verify base fields
    TEST_ASSERT_EQ(deserialized.base.message_type, BLE_MSG_ELECTION_ANNOUNCEMENT,
                   "Message type should be ELECTION_ANNOUNCEMENT");
    TEST_ASSERT_EQ(deserialized.base.sender_id, original.base.sender_id, "Sender ID should match");
    TEST_ASSERT_EQ(deserialized.base.ttl, original.base.ttl, "TTL should match");
    TEST_ASSERT_EQ(deserialized.base.path_length, original.base.path_length, "Path length should match");

    // Verify election fields
    TEST_ASSERT_EQ(deserialized.election.class_id, original.election.class_id, "Class ID should match");
    TEST_ASSERT_EQ(deserialized.election.pdsf, original.election.pdsf, "PDSF should match");
    TEST_ASSERT_DOUBLE_EQ(deserialized.election.score, original.election.score, "Score should match");
    TEST_ASSERT_EQ(deserialized.election.hash, original.election.hash, "Hash should match");
}

/**
 * Test: Buffer overflow protection
 */
void test_buffer_overflow_protection(void)
{
    ble_discovery_packet_t packet;
    ble_discovery_packet_init(&packet);
    packet.sender_id = 123;

    // Try to serialize with insufficient buffer
    uint8_t small_buffer[5];
    uint32_t bytes_written = ble_discovery_serialize(&packet, small_buffer, sizeof(small_buffer));
    TEST_ASSERT_EQ(bytes_written, 0, "Should return 0 when buffer too small");
}

/**
 * Test: Invalid path length protection
 */
void test_invalid_path_length(void)
{
    // Manually create buffer with invalid path length
    uint8_t buffer[256];
    uint8_t *ptr = buffer;

    // Message type
    *ptr++ = BLE_MSG_DISCOVERY;

    // Sender ID
    *ptr++ = 0; *ptr++ = 0; *ptr++ = 0; *ptr++ = 1;

    // TTL
    *ptr++ = 10;

    // Invalid path length (> MAX)
    uint16_t invalid_length = BLE_DISCOVERY_MAX_PATH_LENGTH + 10;
    *ptr++ = (invalid_length >> 8) & 0xFF;
    *ptr++ = invalid_length & 0xFF;

    // Try to deserialize
    ble_discovery_packet_t packet;
    uint32_t bytes_read = ble_discovery_deserialize(&packet, buffer, sizeof(buffer));
    TEST_ASSERT_EQ(bytes_read, 0, "Should return 0 for invalid path length");
}

/**
 * Test: PDSF calculation
 */
void test_pdsf_calculation(void)
{
    uint32_t pi_term = 0;

    // First hop: start with Σ=0, Π=1, contribute 5 neighbors
    uint32_t pdsf = ble_election_calculate_pdsf(0, 1, 5, &pi_term);
    TEST_ASSERT_EQ(pdsf, 5, "First hop should contribute direct neighbor count");
    TEST_ASSERT_EQ(pi_term, 5, "Π term should equal hop product");

    // Second hop: Σ=5, Π=5, contribute 3 unique neighbors -> add 15
    pdsf = ble_election_calculate_pdsf(pdsf, pi_term, 3, &pi_term);
    TEST_ASSERT_EQ(pdsf, 20, "Second hop should add Π term to running sum");
    TEST_ASSERT_EQ(pi_term, 15, "Π term should multiply prior Π by new neighbors");

    // No new neighbors -> Π term zero, Σ unchanged
    pdsf = ble_election_calculate_pdsf(pdsf, pi_term, 0, &pi_term);
    TEST_ASSERT_EQ(pdsf, 20, "Zero neighbors should leave PDSF unchanged");
    TEST_ASSERT_EQ(pi_term, 0, "Π term should be zero when no neighbors contribute");

    // Large values saturate at UINT32_MAX
    uint32_t saturated = ble_election_calculate_pdsf(UINT32_MAX, UINT32_MAX, 10, &pi_term);
    TEST_ASSERT_EQ(saturated, UINT32_MAX, "PDSF should saturate at UINT32_MAX on overflow");
    TEST_ASSERT_EQ(pi_term, UINT32_MAX, "Π term should also saturate when overflowing");
}

/**
 * Test: Score calculation
 */
void test_score_calculation(void)
{
    // Score = direct connections + (connections / noise)
    double score = ble_election_calculate_score(10, 5.0);
    double expected = 10.0 + (10.0 / 5.0);
    TEST_ASSERT_DOUBLE_EQ(score, expected, "Score should equal direct connections plus connection ratio");

    // Zero noise should reduce to direct connections only
    score = ble_election_calculate_score(8, 0.0);
    TEST_ASSERT_DOUBLE_EQ(score, 8.0, "Zero noise should produce score equal to direct connections");

    // More neighbors and lower noise should yield higher score
    double score_high = ble_election_calculate_score(20, 2.0);
    TEST_ASSERT(score_high > score, "Better connectivity should result in higher score");
}

/**
 * Test: Hash generation
 */
void test_hash_generation(void)
{
    // Hash should be deterministic
    uint32_t hash1 = ble_election_generate_hash(12345);
    uint32_t hash2 = ble_election_generate_hash(12345);
    TEST_ASSERT_EQ(hash1, hash2, "Same input should produce same hash");

    // Different inputs should produce different hashes
    uint32_t hash3 = ble_election_generate_hash(54321);
    TEST_ASSERT(hash1 != hash3, "Different inputs should produce different hashes");

    // Hash should be non-zero for non-zero input
    uint32_t hash4 = ble_election_generate_hash(1);
    TEST_ASSERT(hash4 != 0, "Hash should be non-zero");
}

/**
 * Test: Large path serialization
 */
void test_large_path_serialization(void)
{
    ble_discovery_packet_t original, deserialized;
    ble_discovery_packet_init(&original);

    // Add many nodes to path (but not max, to keep test fast)
    for (uint32_t i = 0; i < 20; i++)
    {
        ble_discovery_add_to_path(&original, i * 100);
    }

    // Serialize
    uint8_t buffer[1024];
    uint32_t bytes_written = ble_discovery_serialize(&original, buffer, sizeof(buffer));
    TEST_ASSERT(bytes_written > 0, "Should serialize large path");

    // Deserialize
    uint32_t bytes_read = ble_discovery_deserialize(&deserialized, buffer, bytes_written);
    TEST_ASSERT_EQ(bytes_read, bytes_written, "Should deserialize large path");
    TEST_ASSERT_EQ(deserialized.path_length, 20, "Path length should be preserved");

    // Verify all path nodes
    for (uint16_t i = 0; i < deserialized.path_length; i++)
    {
        TEST_ASSERT_EQ(deserialized.path[i], i * 100, "Path node should match");
    }
}

/**
 * Main test runner
 */
int main(void)
{
    printf("=== BLE Discovery Protocol C Core Tests ===\n\n");

    test_packet_init();
    test_election_init();
    test_ttl_operations();
    test_path_operations();
    test_path_overflow();
    test_gps_operations();
    test_discovery_serialization();
    test_discovery_serialization_no_gps();
    test_election_serialization();
    test_buffer_overflow_protection();
    test_invalid_path_length();
    test_pdsf_calculation();
    test_pdsf_history_serialization();
    test_score_calculation();
    test_hash_generation();
    test_large_path_serialization();

    printf("\n=== Test Results ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);

    if (tests_failed == 0)
    {
        printf("\nAll tests PASSED!\n");
        return 0;
    }
    else
    {
        printf("\nSome tests FAILED!\n");
        return 1;
    }
}
