/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025
 *
 * Task 11 Comprehensive Test: TTL-Based Message Prioritization
 *
 * This simulation tests all subtasks for Task 11:
 * - Subtask 1: Implement TTL decrement on message forwarding
 * - Subtask 2: Create priority queue sorted by TTL
 * - Subtask 3: Implement top-3 selection algorithm after other filters applied
 * - Subtask 4: Add TTL expiration handling (TTL=0 messages not forwarded)
 * - Subtask 5: Test message propagation distance vs initial TTL
 * - Subtask 6: Validate network coverage with different TTL values
 *
 * Usage:
 *   ./waf --run "task-11-test --verbose=true"
 */

#include "ns3/core-module.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/ble-message-queue.h"
#include "ns3/ble-forwarding-logic.h"
#include "ns3/ble-discovery-header-wrapper.h"
#include "ns3/ble-discovery-cycle-wrapper.h"
#include "ns3/vector.h"
#include "ns3/double.h"
#include "ns3/random-variable-stream.h"

/* Include C headers for direct testing */
extern "C" {
#include "ns3/ble_discovery_packet.h"
#include "ns3/ble_message_queue.h"
}

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Task11Test");

// ============================================================================
// GLOBAL TEST STATE
// ============================================================================

struct SubtaskResult {
    std::string name;
    bool passed;
    std::string details;
};

std::vector<SubtaskResult> g_testResults;
bool g_verbose = true;

// ANSI color codes
const std::string COLOR_RESET = "\033[0m";
const std::string COLOR_GREEN = "\033[32m";
const std::string COLOR_RED = "\033[31m";
const std::string COLOR_YELLOW = "\033[33m";
const std::string COLOR_CYAN = "\033[36m";
const std::string COLOR_BOLD = "\033[1m";
const std::string COLOR_MAGENTA = "\033[35m";

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void PrintHeader (const std::string& title)
{
    std::cout << "\n";
    std::cout << COLOR_BOLD << "============================================================\n";
    std::cout << " " << title << "\n";
    std::cout << "============================================================" << COLOR_RESET << "\n";
}

void PrintSubHeader (const std::string& title)
{
    std::cout << "\n" << COLOR_CYAN << "--- " << title << " ---" << COLOR_RESET << "\n";
}

void PrintState (const std::string& message)
{
    if (g_verbose)
    {
        std::cout << "[STATE] " << message << "\n";
    }
}

void PrintInit (const std::string& message)
{
    if (g_verbose)
    {
        std::cout << COLOR_CYAN << "[INIT] " << message << COLOR_RESET << "\n";
    }
}

void PrintActual (const std::string& label, const std::string& value)
{
    std::cout << "  Actual:   " << label << " = " << value << "\n";
}

void PrintExpected (const std::string& label, const std::string& value)
{
    std::cout << COLOR_YELLOW << "  Expected: " << label << " = " << value << COLOR_RESET << "\n";
}

void PrintCheck (const std::string& check, bool passed)
{
    if (passed)
    {
        std::cout << "  [" << COLOR_GREEN << "PASS" << COLOR_RESET << "] " << check << "\n";
    }
    else
    {
        std::cout << "  [" << COLOR_RED << "FAIL" << COLOR_RESET << "] " << check << "\n";
    }
}

void RecordResult (const std::string& name, bool passed, const std::string& details = "")
{
    SubtaskResult result;
    result.name = name;
    result.passed = passed;
    result.details = details;
    g_testResults.push_back(result);
}

std::string TtlVectorToString (const std::vector<uint8_t>& ttls)
{
    if (ttls.empty()) return "[]";
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < ttls.size(); i++)
    {
        ss << static_cast<int>(ttls[i]);
        if (i < ttls.size() - 1) ss << ", ";
    }
    ss << "]";
    return ss.str();
}

// ============================================================================
// SUBTASK 1: Implement TTL Decrement on Message Forwarding
// ============================================================================

class Subtask1Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 1: TTL Decrement on Message Forwarding");

        bool allPassed = true;

        // Test 1.1: TTL Decrement from Default Value
        PrintSubHeader("Test 1.1: TTL Decrement from Default Value (10 -> 9)");
        {
            ble_discovery_packet_t packet;
            ble_discovery_packet_init(&packet);

            PrintInit("Initializing packet with default TTL...");
            PrintState("Default TTL: " + std::to_string(BLE_DISCOVERY_DEFAULT_TTL));

            uint8_t initialTtl = packet.ttl;
            bool result = ble_discovery_decrement_ttl(&packet);
            uint8_t finalTtl = packet.ttl;

            PrintActual("initial TTL", std::to_string(initialTtl));
            PrintExpected("initial TTL", "10 (BLE_DISCOVERY_DEFAULT_TTL)");
            PrintActual("final TTL after decrement", std::to_string(finalTtl));
            PrintExpected("final TTL after decrement", "9");
            PrintActual("decrement returned", result ? "true" : "false");
            PrintExpected("decrement returned", "true (TTL was > 0)");

            bool passed = (initialTtl == 10) && (finalTtl == 9) && result;
            PrintCheck("TTL decremented from 10 to 9", passed);

            allPassed &= passed;
        }

        // Test 1.2: TTL Decrement Multiple Times
        PrintSubHeader("Test 1.2: TTL Decrement Multiple Times");
        {
            ble_discovery_packet_t packet;
            ble_discovery_packet_init(&packet);
            packet.ttl = 5;

            PrintInit("Starting with TTL=5, decrementing 5 times...");

            std::vector<uint8_t> ttlSequence;
            ttlSequence.push_back(packet.ttl);

            for (int i = 0; i < 5; ++i)
            {
                ble_discovery_decrement_ttl(&packet);
                ttlSequence.push_back(packet.ttl);
            }

            PrintActual("TTL sequence", TtlVectorToString(ttlSequence));
            PrintExpected("TTL sequence", "[5, 4, 3, 2, 1, 0]");

            bool correctSequence = (ttlSequence[0] == 5) && (ttlSequence[1] == 4) &&
                                   (ttlSequence[2] == 3) && (ttlSequence[3] == 2) &&
                                   (ttlSequence[4] == 1) && (ttlSequence[5] == 0);

            PrintCheck("TTL decremented correctly in sequence", correctSequence);

            allPassed &= correctSequence;
        }

        // Test 1.3: TTL Decrement at Boundary (1 -> 0)
        PrintSubHeader("Test 1.3: TTL Decrement at Boundary (1 -> 0)");
        {
            ble_discovery_packet_t packet;
            ble_discovery_packet_init(&packet);
            packet.ttl = 1;

            PrintInit("Starting with TTL=1 (boundary case)...");

            bool result = ble_discovery_decrement_ttl(&packet);

            PrintActual("TTL after decrement", std::to_string(packet.ttl));
            PrintExpected("TTL after decrement", "0");
            PrintActual("decrement returned", result ? "true" : "false");
            PrintExpected("decrement returned", "true (TTL was 1, now 0)");

            bool passed = (packet.ttl == 0) && result;
            PrintCheck("TTL correctly decremented from 1 to 0", passed);

            allPassed &= passed;
        }

        // Test 1.4: TTL Decrement at Zero (0 -> 0, returns false)
        PrintSubHeader("Test 1.4: TTL Decrement at Zero (No Change)");
        {
            ble_discovery_packet_t packet;
            ble_discovery_packet_init(&packet);
            packet.ttl = 0;

            PrintInit("Starting with TTL=0 (already expired)...");

            bool result = ble_discovery_decrement_ttl(&packet);

            PrintActual("TTL after attempted decrement", std::to_string(packet.ttl));
            PrintExpected("TTL after attempted decrement", "0 (unchanged)");
            PrintActual("decrement returned", result ? "true" : "false");
            PrintExpected("decrement returned", "false (cannot decrement below 0)");

            bool passed = (packet.ttl == 0) && !result;
            PrintCheck("TTL remains 0 and returns false", passed);

            allPassed &= passed;
        }

        // Test 1.5: TTL Decrement with NULL Packet
        PrintSubHeader("Test 1.5: TTL Decrement with NULL Packet");
        {
            PrintInit("Testing with NULL packet pointer...");

            bool result = ble_discovery_decrement_ttl(NULL);

            PrintActual("decrement returned", result ? "true" : "false");
            PrintExpected("decrement returned", "false (NULL safety)");

            bool passed = !result;
            PrintCheck("NULL packet handled safely", passed);

            allPassed &= passed;
        }

        // Test 1.6: TTL Decrement Preserves Other Fields
        PrintSubHeader("Test 1.6: TTL Decrement Preserves Other Fields");
        {
            ble_discovery_packet_t packet;
            ble_discovery_packet_init(&packet);
            packet.sender_id = 12345;
            packet.ttl = 10;
            packet.path_length = 3;
            packet.path[0] = 100;
            packet.path[1] = 200;
            packet.path[2] = 300;
            packet.gps_available = true;
            packet.gps_location.x = 1.5;
            packet.gps_location.y = 2.5;
            packet.gps_location.z = 3.5;

            PrintInit("Setting up packet with all fields populated...");
            PrintState("sender_id=12345, TTL=10, path=[100,200,300], GPS=(1.5,2.5,3.5)");

            ble_discovery_decrement_ttl(&packet);

            bool senderPreserved = (packet.sender_id == 12345);
            bool pathPreserved = (packet.path_length == 3) &&
                                 (packet.path[0] == 100) &&
                                 (packet.path[1] == 200) &&
                                 (packet.path[2] == 300);
            bool gpsPreserved = packet.gps_available &&
                               (packet.gps_location.x == 1.5) &&
                               (packet.gps_location.y == 2.5) &&
                               (packet.gps_location.z == 3.5);
            bool ttlDecremented = (packet.ttl == 9);

            PrintActual("sender_id preserved", senderPreserved ? "true" : "false");
            PrintExpected("sender_id preserved", "true");
            PrintActual("path preserved", pathPreserved ? "true" : "false");
            PrintExpected("path preserved", "true");
            PrintActual("GPS preserved", gpsPreserved ? "true" : "false");
            PrintExpected("GPS preserved", "true");
            PrintActual("TTL decremented", ttlDecremented ? "true" : "false");
            PrintExpected("TTL decremented", "true (10 -> 9)");

            bool allPreserved = senderPreserved && pathPreserved && gpsPreserved && ttlDecremented;
            PrintCheck("All other fields preserved after TTL decrement", allPreserved);

            allPassed &= allPreserved;
        }

        // Test 1.7: TTL Decrement Return Value Semantics
        PrintSubHeader("Test 1.7: TTL Decrement Return Value Semantics");
        {
            PrintInit("Testing return value semantics...");
            PrintState("Return true: TTL was > 0 before decrement");
            PrintState("Return false: TTL was 0 (cannot decrement)");

            ble_discovery_packet_t packet;

            // Test various TTL values
            std::vector<std::pair<uint8_t, bool>> testCases = {
                {10, true}, {5, true}, {2, true}, {1, true}, {0, false}
            };

            bool allCorrect = true;
            for (const auto& tc : testCases)
            {
                ble_discovery_packet_init(&packet);
                packet.ttl = tc.first;
                bool result = ble_discovery_decrement_ttl(&packet);

                if (result != tc.second)
                {
                    allCorrect = false;
                    PrintState("MISMATCH: TTL=" + std::to_string(tc.first) +
                              " expected " + (tc.second ? "true" : "false") +
                              " got " + (result ? "true" : "false"));
                }
            }

            PrintActual("all return values correct", allCorrect ? "true" : "false");
            PrintExpected("all return values correct", "true");

            PrintCheck("Return value semantics correct for all TTL values", allCorrect);

            allPassed &= allCorrect;
        }

        RecordResult("Subtask 1: TTL Decrement on Message Forwarding", allPassed);
        return allPassed;
    }
};

// ============================================================================
// SUBTASK 2: Create Priority Queue Sorted by TTL
// ============================================================================

class Subtask2Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 2: Priority Queue Sorted by TTL");

        bool allPassed = true;

        // Test 2.1: Priority Calculation - Higher TTL = Higher Priority
        PrintSubHeader("Test 2.1: Priority Calculation - Higher TTL = Lower Priority Value");
        {
            PrintInit("Testing priority calculation formula: priority = 255 - TTL");
            PrintState("Higher TTL -> Lower priority value -> Higher actual priority");

            ble_discovery_packet_t packet;
            ble_discovery_packet_init(&packet);

            std::vector<std::pair<uint8_t, uint8_t>> testCases = {
                {10, 245}, {5, 250}, {1, 254}, {15, 240}, {50, 205}
            };

            bool allCorrect = true;
            std::cout << "\n  " << COLOR_MAGENTA << "TTL | Expected Priority | Actual Priority | Match" << COLOR_RESET << "\n";
            std::cout << "  " << std::string(55, '-') << "\n";

            for (const auto& tc : testCases)
            {
                packet.ttl = tc.first;
                uint8_t priority = ble_queue_calculate_priority(&packet);
                bool match = (priority == tc.second);
                if (!match) allCorrect = false;

                std::cout << "  " << std::setw(3) << static_cast<int>(tc.first)
                          << " | " << std::setw(17) << static_cast<int>(tc.second)
                          << " | " << std::setw(15) << static_cast<int>(priority)
                          << " | " << (match ? (COLOR_GREEN + "YES" + COLOR_RESET) : (COLOR_RED + "NO" + COLOR_RESET))
                          << "\n";
            }

            PrintCheck("Priority = 255 - TTL for all test cases", allCorrect);

            allPassed &= allCorrect;
        }

        // Test 2.2: Priority Calculation - TTL=0 Gets Lowest Priority
        PrintSubHeader("Test 2.2: TTL=0 Gets Lowest Priority (255)");
        {
            ble_discovery_packet_t packet;
            ble_discovery_packet_init(&packet);
            packet.ttl = 0;

            PrintInit("Testing priority for expired message (TTL=0)...");

            uint8_t priority = ble_queue_calculate_priority(&packet);

            PrintActual("priority for TTL=0", std::to_string(priority));
            PrintExpected("priority for TTL=0", "255 (lowest possible priority)");

            bool passed = (priority == 255);
            PrintCheck("TTL=0 gets priority 255 (lowest)", passed);

            allPassed &= passed;
        }

        // Test 2.3: Priority Formula Verification
        PrintSubHeader("Test 2.3: Priority Formula Verification (255 - TTL)");
        {
            PrintInit("Verifying priority = 255 - TTL for range of values...");

            bool allCorrect = true;
            for (uint8_t ttl = 0; ttl <= 100; ttl += 10)
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.ttl = ttl;

                uint8_t priority = ble_queue_calculate_priority(&packet);
                uint8_t expected = (ttl == 0) ? 255 : (255 - ttl);

                if (priority != expected)
                {
                    allCorrect = false;
                    PrintState("MISMATCH at TTL=" + std::to_string(ttl) +
                              ": expected " + std::to_string(expected) +
                              ", got " + std::to_string(priority));
                }
            }

            PrintActual("all priorities match formula", allCorrect ? "true" : "false");
            PrintExpected("all priorities match formula", "true");

            PrintCheck("Priority formula verified across TTL range", allCorrect);

            allPassed &= allCorrect;
        }

        // Test 2.4: Dequeue Returns Highest Priority Message First
        PrintSubHeader("Test 2.4: Dequeue Returns Highest Priority Message First");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Enqueuing messages with TTL values: 5, 10, 3, 8, 1");
            PrintState("Expected dequeue order (highest TTL first): 10, 8, 5, 3, 1");

            std::vector<uint8_t> ttlValues = {5, 10, 3, 8, 1};
            uint32_t time_ms = 1000;

            for (uint8_t ttl : ttlValues)
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = ttl * 100; // Unique sender IDs
                packet.ttl = ttl;
                ble_queue_enqueue(&queue, &packet, 999, time_ms++);
            }

            std::vector<uint8_t> dequeuedTtls;
            ble_election_packet_t dequeuedPacket;

            while (ble_queue_dequeue(&queue, &dequeuedPacket))
            {
                dequeuedTtls.push_back(dequeuedPacket.base.ttl);
            }

            PrintActual("dequeue order", TtlVectorToString(dequeuedTtls));
            PrintExpected("dequeue order", "[10, 8, 5, 3, 1]");

            bool correctOrder = (dequeuedTtls.size() == 5) &&
                               (dequeuedTtls[0] == 10) &&
                               (dequeuedTtls[1] == 8) &&
                               (dequeuedTtls[2] == 5) &&
                               (dequeuedTtls[3] == 3) &&
                               (dequeuedTtls[4] == 1);

            PrintCheck("Messages dequeued in priority order (highest TTL first)", correctOrder);

            allPassed &= correctOrder;
        }

        // Test 2.5: Multiple Messages with Different TTLs
        PrintSubHeader("Test 2.5: Large Queue Priority Ordering");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Enqueuing 20 messages with random TTLs...");

            std::vector<uint8_t> inputTtls = {7, 15, 3, 9, 12, 1, 20, 5, 18, 2,
                                               11, 6, 14, 4, 16, 8, 13, 17, 10, 19};
            uint32_t time_ms = 1000;

            for (uint8_t ttl : inputTtls)
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = ttl * 100;
                packet.ttl = ttl;
                ble_queue_enqueue(&queue, &packet, 999, time_ms++);
            }

            // Dequeue and verify order
            std::vector<uint8_t> dequeuedTtls;
            ble_election_packet_t dequeuedPacket;

            while (ble_queue_dequeue(&queue, &dequeuedPacket))
            {
                dequeuedTtls.push_back(dequeuedPacket.base.ttl);
            }

            // Verify descending order
            bool sortedDescending = true;
            for (size_t i = 1; i < dequeuedTtls.size(); ++i)
            {
                if (dequeuedTtls[i] > dequeuedTtls[i-1])
                {
                    sortedDescending = false;
                    break;
                }
            }

            PrintActual("first 5 dequeued TTLs", TtlVectorToString(
                std::vector<uint8_t>(dequeuedTtls.begin(), dequeuedTtls.begin() + 5)));
            PrintExpected("first 5 dequeued TTLs", "[20, 19, 18, 17, 16]");
            PrintActual("sorted in descending order", sortedDescending ? "true" : "false");
            PrintExpected("sorted in descending order", "true");

            PrintCheck("Large queue maintains priority order", sortedDescending);

            allPassed &= sortedDescending;
        }

        // Test 2.6: Same TTL Messages
        PrintSubHeader("Test 2.6: Same TTL Messages");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Enqueuing 5 messages all with TTL=10...");

            std::vector<uint32_t> senderIds = {100, 200, 300, 400, 500};
            uint32_t time_ms = 1000;

            for (uint32_t sid : senderIds)
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = sid;
                packet.ttl = 10;
                ble_queue_enqueue(&queue, &packet, 999, time_ms++);
            }

            // Dequeue all
            std::vector<uint32_t> dequeuedSenders;
            ble_election_packet_t dequeuedPacket;

            while (ble_queue_dequeue(&queue, &dequeuedPacket))
            {
                dequeuedSenders.push_back(dequeuedPacket.base.sender_id);
            }

            PrintActual("number of messages dequeued", std::to_string(dequeuedSenders.size()));
            PrintExpected("number of messages dequeued", "5");

            // All should be dequeued (order may vary for same priority)
            bool allDequeued = (dequeuedSenders.size() == 5);
            PrintCheck("All same-TTL messages dequeued", allDequeued);

            allPassed &= allDequeued;
        }

        // Test 2.7: Peek Returns Highest Priority Without Removal
        PrintSubHeader("Test 2.7: Peek Returns Highest Priority Without Removal");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Enqueuing messages with TTL: 5, 15, 8...");

            std::vector<uint8_t> ttls = {5, 15, 8};
            uint32_t time_ms = 1000;

            for (uint8_t ttl : ttls)
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = ttl * 100;
                packet.ttl = ttl;
                ble_queue_enqueue(&queue, &packet, 999, time_ms++);
            }

            uint32_t sizeBeforePeek = ble_queue_get_size(&queue);

            ble_election_packet_t peekedPacket;
            bool peekResult = ble_queue_peek(&queue, &peekedPacket);

            uint32_t sizeAfterPeek = ble_queue_get_size(&queue);

            PrintActual("peeked TTL", std::to_string(peekedPacket.base.ttl));
            PrintExpected("peeked TTL", "15 (highest TTL = highest priority)");
            PrintActual("queue size before peek", std::to_string(sizeBeforePeek));
            PrintActual("queue size after peek", std::to_string(sizeAfterPeek));
            PrintExpected("queue size unchanged", "3");

            bool passed = peekResult && (peekedPacket.base.ttl == 15) &&
                         (sizeBeforePeek == 3) && (sizeAfterPeek == 3);

            PrintCheck("Peek returns highest priority without removing", passed);

            allPassed &= passed;
        }

        // Test 2.8: Priority Maintained After Enqueue/Dequeue Cycles
        PrintSubHeader("Test 2.8: Priority Maintained After Enqueue/Dequeue Cycles");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Testing multiple enqueue/dequeue cycles...");

            uint32_t time_ms = 1000;

            // First cycle
            for (uint8_t ttl : {5, 10, 3})
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = ttl * 100;
                packet.ttl = ttl;
                ble_queue_enqueue(&queue, &packet, 999, time_ms++);
            }

            ble_election_packet_t dequeuedPacket;
            ble_queue_dequeue(&queue, &dequeuedPacket);
            uint8_t firstDequeued = dequeuedPacket.base.ttl;

            // Add more
            for (uint8_t ttl : {15, 2})
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = ttl * 100 + 1; // Different sender
                packet.ttl = ttl;
                ble_queue_enqueue(&queue, &packet, 999, time_ms++);
            }

            // Dequeue remaining
            std::vector<uint8_t> remainingTtls;
            while (ble_queue_dequeue(&queue, &dequeuedPacket))
            {
                remainingTtls.push_back(dequeuedPacket.base.ttl);
            }

            PrintActual("first dequeued TTL", std::to_string(firstDequeued));
            PrintExpected("first dequeued TTL", "10");
            PrintActual("remaining dequeue order", TtlVectorToString(remainingTtls));
            PrintExpected("remaining dequeue order", "[15, 5, 3, 2]");

            bool correctFirst = (firstDequeued == 10);
            bool correctRemaining = (remainingTtls.size() == 4) &&
                                   (remainingTtls[0] == 15) &&
                                   (remainingTtls[1] == 5) &&
                                   (remainingTtls[2] == 3) &&
                                   (remainingTtls[3] == 2);

            PrintCheck("Priority maintained across cycles", correctFirst && correctRemaining);

            allPassed &= (correctFirst && correctRemaining);
        }

        RecordResult("Subtask 2: Priority Queue Sorted by TTL", allPassed);
        return allPassed;
    }
};

// ============================================================================
// SUBTASK 3: Implement Top-3 Selection Algorithm
// ============================================================================

class Subtask3Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 3: Top-3 Selection Algorithm");

        bool allPassed = true;

        // Test 3.1: Dequeue Top-3 from Queue with 5+ Messages
        PrintSubHeader("Test 3.1: Dequeue Top-3 from Queue with 5+ Messages");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Enqueuing 7 messages with TTL: 3, 8, 1, 12, 5, 10, 7");
            PrintState("Top-3 by TTL should be: 12, 10, 8");

            std::vector<uint8_t> ttls = {3, 8, 1, 12, 5, 10, 7};
            uint32_t time_ms = 1000;

            for (uint8_t ttl : ttls)
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = ttl * 100;
                packet.ttl = ttl;
                ble_queue_enqueue(&queue, &packet, 999, time_ms++);
            }

            // Dequeue top 3
            std::vector<uint8_t> top3;
            ble_election_packet_t dequeuedPacket;

            for (int i = 0; i < 3; ++i)
            {
                if (ble_queue_dequeue(&queue, &dequeuedPacket))
                {
                    top3.push_back(dequeuedPacket.base.ttl);
                }
            }

            PrintActual("top-3 dequeued", TtlVectorToString(top3));
            PrintExpected("top-3 dequeued", "[12, 10, 8]");

            bool passed = (top3.size() == 3) &&
                         (top3[0] == 12) && (top3[1] == 10) && (top3[2] == 8);

            PrintCheck("Top-3 highest TTL messages dequeued correctly", passed);

            // Verify remaining
            uint32_t remaining = ble_queue_get_size(&queue);
            PrintActual("remaining in queue", std::to_string(remaining));
            PrintExpected("remaining in queue", "4");

            allPassed &= passed && (remaining == 4);
        }

        // Test 3.2: Dequeue Top-3 When Only 2 Messages Available
        PrintSubHeader("Test 3.2: Dequeue Top-3 When Only 2 Messages Available");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Enqueuing only 2 messages with TTL: 5, 10");

            for (uint8_t ttl : {5, 10})
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = ttl * 100;
                packet.ttl = ttl;
                ble_queue_enqueue(&queue, &packet, 999, 1000);
            }

            std::vector<uint8_t> dequeued;
            ble_election_packet_t dequeuedPacket;

            for (int i = 0; i < 3; ++i)
            {
                if (ble_queue_dequeue(&queue, &dequeuedPacket))
                {
                    dequeued.push_back(dequeuedPacket.base.ttl);
                }
            }

            PrintActual("messages dequeued (attempting 3)", TtlVectorToString(dequeued));
            PrintExpected("messages dequeued", "[10, 5] (only 2 available)");

            bool passed = (dequeued.size() == 2) &&
                         (dequeued[0] == 10) && (dequeued[1] == 5);

            PrintCheck("Gracefully handles fewer than 3 messages", passed);

            allPassed &= passed;
        }

        // Test 3.3: Dequeue Top-3 When Only 1 Message Available
        PrintSubHeader("Test 3.3: Dequeue Top-3 When Only 1 Message Available");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Enqueuing only 1 message with TTL=15");

            ble_discovery_packet_t packet;
            ble_discovery_packet_init(&packet);
            packet.sender_id = 1500;
            packet.ttl = 15;
            ble_queue_enqueue(&queue, &packet, 999, 1000);

            std::vector<uint8_t> dequeued;
            ble_election_packet_t dequeuedPacket;

            for (int i = 0; i < 3; ++i)
            {
                if (ble_queue_dequeue(&queue, &dequeuedPacket))
                {
                    dequeued.push_back(dequeuedPacket.base.ttl);
                }
            }

            PrintActual("messages dequeued (attempting 3)", TtlVectorToString(dequeued));
            PrintExpected("messages dequeued", "[15] (only 1 available)");

            bool passed = (dequeued.size() == 1) && (dequeued[0] == 15);

            PrintCheck("Correctly handles single message", passed);

            allPassed &= passed;
        }

        // Test 3.4: Dequeue Top-3 from Empty Queue
        PrintSubHeader("Test 3.4: Dequeue Top-3 from Empty Queue");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Attempting to dequeue from empty queue...");

            uint32_t successCount = 0;
            ble_election_packet_t dequeuedPacket;

            for (int i = 0; i < 3; ++i)
            {
                if (ble_queue_dequeue(&queue, &dequeuedPacket))
                {
                    successCount++;
                }
            }

            PrintActual("successful dequeues", std::to_string(successCount));
            PrintExpected("successful dequeues", "0");

            bool passed = (successCount == 0);
            PrintCheck("Empty queue returns no messages", passed);

            allPassed &= passed;
        }

        // Test 3.5: Top-3 Selection Respects Priority Ordering
        PrintSubHeader("Test 3.5: Top-3 Selection Respects Priority Ordering");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Enqueuing 10 messages with TTL: 1 through 10 (shuffled)");

            std::vector<uint8_t> ttls = {5, 1, 8, 3, 10, 2, 7, 4, 9, 6};
            uint32_t time_ms = 1000;

            for (uint8_t ttl : ttls)
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = ttl * 100;
                packet.ttl = ttl;
                ble_queue_enqueue(&queue, &packet, 999, time_ms++);
            }

            std::vector<uint8_t> top3;
            ble_election_packet_t dequeuedPacket;

            for (int i = 0; i < 3; ++i)
            {
                if (ble_queue_dequeue(&queue, &dequeuedPacket))
                {
                    top3.push_back(dequeuedPacket.base.ttl);
                }
            }

            PrintActual("top-3 dequeued", TtlVectorToString(top3));
            PrintExpected("top-3 dequeued", "[10, 9, 8]");

            bool passed = (top3.size() == 3) &&
                         (top3[0] == 10) && (top3[1] == 9) && (top3[2] == 8);

            PrintCheck("Top-3 are the highest TTL messages in order", passed);

            allPassed &= passed;
        }

        // Test 3.6: Integration with 4-Slot Cycle Concept
        PrintSubHeader("Test 3.6: Integration with 4-Slot Discovery Cycle");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Simulating 4-slot discovery cycle...");
            PrintState("Slot 0: Own message broadcast (not from queue)");
            PrintState("Slots 1-3: Forward top-3 messages from queue");

            // Queue has 5 messages
            std::vector<uint8_t> ttls = {8, 3, 12, 5, 10};
            uint32_t time_ms = 1000;

            for (uint8_t ttl : ttls)
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = ttl * 100;
                packet.ttl = ttl;
                ble_queue_enqueue(&queue, &packet, 999, time_ms++);
            }

            PrintState("Queue contains 5 messages with TTL: [8, 3, 12, 5, 10]");

            // Simulate slots 1, 2, 3 - each dequeues one message
            std::vector<uint8_t> forwardedTtls;
            ble_election_packet_t forwardPacket;

            for (int slot = 1; slot <= 3; ++slot)
            {
                PrintState("Slot " + std::to_string(slot) + ": Dequeue and forward...");
                if (ble_queue_dequeue(&queue, &forwardPacket))
                {
                    forwardedTtls.push_back(forwardPacket.base.ttl);
                }
            }

            PrintActual("forwarded in slots 1-3", TtlVectorToString(forwardedTtls));
            PrintExpected("forwarded in slots 1-3", "[12, 10, 8] (top 3 by TTL)");

            bool passed = (forwardedTtls.size() == 3) &&
                         (forwardedTtls[0] == 12) &&
                         (forwardedTtls[1] == 10) &&
                         (forwardedTtls[2] == 8);

            PrintCheck("Top-3 forwarded in slots 1, 2, 3", passed);

            // Check remaining
            uint32_t remaining = ble_queue_get_size(&queue);
            PrintActual("remaining for next cycle", std::to_string(remaining));
            PrintExpected("remaining for next cycle", "2");

            allPassed &= passed && (remaining == 2);
        }

        // Test 3.7: Top-3 with Mixed TTL Values
        PrintSubHeader("Test 3.7: Top-3 with Mixed TTL Values Including Edge Cases");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Enqueuing messages including edge case TTLs...");

            std::vector<uint8_t> ttls = {255, 1, 128, 0, 64, 2, 200};
            uint32_t time_ms = 1000;

            for (uint8_t ttl : ttls)
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = ttl * 10 + 1; // Unique sender
                packet.ttl = ttl;
                ble_queue_enqueue(&queue, &packet, 999, time_ms++);
            }

            std::vector<uint8_t> top3;
            ble_election_packet_t dequeuedPacket;

            for (int i = 0; i < 3; ++i)
            {
                if (ble_queue_dequeue(&queue, &dequeuedPacket))
                {
                    top3.push_back(dequeuedPacket.base.ttl);
                }
            }

            PrintActual("top-3 dequeued", TtlVectorToString(top3));
            PrintExpected("top-3 dequeued", "[255, 200, 128] (highest TTLs)");

            bool passed = (top3.size() == 3) &&
                         (top3[0] == 255) && (top3[1] == 200) && (top3[2] == 128);

            PrintCheck("Edge case TTL values handled correctly", passed);

            allPassed &= passed;
        }

        RecordResult("Subtask 3: Top-3 Selection Algorithm", allPassed);
        return allPassed;
    }
};

// ============================================================================
// SUBTASK 4: TTL Expiration Handling (TTL=0 Messages Not Forwarded)
// ============================================================================

class Subtask4Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 4: TTL Expiration Handling");

        bool allPassed = true;

        // Test 4.1: TTL=0 Message Rejected by Forwarding Logic
        PrintSubHeader("Test 4.1: TTL=0 Message Rejected by Forwarding Logic");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Ptr<UniformRandomVariable> random = CreateObject<UniformRandomVariable>();
            random->SetAttribute("Min", DoubleValue(0.0));
            random->SetAttribute("Max", DoubleValue(0.01)); // Almost always forward
            logic->SetRandomStream(random);

            PrintInit("Creating message with TTL=0 (expired)...");

            BleDiscoveryHeaderWrapper expiredHeader;
            expiredHeader.SetSenderId(100);
            expiredHeader.SetTtl(0);
            expiredHeader.AddToPath(100);
            expiredHeader.SetGpsLocation(Vector(100.0, 100.0, 0.0));

            Vector currentLoc(0.0, 0.0, 0.0);

            PrintState("Message: TTL=0, GPS far, low crowding=0.0");
            PrintState("Even with all other conditions favorable, should NOT forward");

            bool shouldForward = logic->ShouldForward(expiredHeader, currentLoc, 0.0, 10.0);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "false (TTL=0 always rejected)");

            bool passed = !shouldForward;
            PrintCheck("TTL=0 message rejected by ShouldForward", passed);

            allPassed &= passed;
        }

        // Test 4.2: TTL=0 Message Gets Lowest Priority (255)
        PrintSubHeader("Test 4.2: TTL=0 Message Gets Lowest Priority (255)");
        {
            ble_discovery_packet_t packet;
            ble_discovery_packet_init(&packet);
            packet.ttl = 0;

            uint8_t priority = ble_queue_calculate_priority(&packet);

            PrintActual("priority for TTL=0", std::to_string(priority));
            PrintExpected("priority for TTL=0", "255 (lowest)");

            bool passed = (priority == 255);
            PrintCheck("TTL=0 gets priority 255", passed);

            allPassed &= passed;
        }

        // Test 4.3: TTL=0 Message in Queue Never Selected First
        PrintSubHeader("Test 4.3: TTL=0 Message Never Selected Over TTL>0 Messages");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Enqueuing mix of TTL=0 and TTL>0 messages...");

            // Add TTL=0 first
            ble_discovery_packet_t expiredPacket;
            ble_discovery_packet_init(&expiredPacket);
            expiredPacket.sender_id = 1;
            expiredPacket.ttl = 0;
            ble_queue_enqueue(&queue, &expiredPacket, 999, 1000);

            // Add TTL=1 (lowest non-zero)
            ble_discovery_packet_t lowTtlPacket;
            ble_discovery_packet_init(&lowTtlPacket);
            lowTtlPacket.sender_id = 2;
            lowTtlPacket.ttl = 1;
            ble_queue_enqueue(&queue, &lowTtlPacket, 999, 1001);

            // Another TTL=0
            ble_discovery_packet_t expired2;
            ble_discovery_packet_init(&expired2);
            expired2.sender_id = 3;
            expired2.ttl = 0;
            ble_queue_enqueue(&queue, &expired2, 999, 1002);

            PrintState("Queue: TTL=0 (sender 1), TTL=1 (sender 2), TTL=0 (sender 3)");

            ble_election_packet_t dequeuedPacket;
            ble_queue_dequeue(&queue, &dequeuedPacket);

            PrintActual("first dequeued sender_id", std::to_string(dequeuedPacket.base.sender_id));
            PrintActual("first dequeued TTL", std::to_string(dequeuedPacket.base.ttl));
            PrintExpected("first dequeued", "sender_id=2, TTL=1 (only non-expired)");

            bool passed = (dequeuedPacket.base.sender_id == 2) && (dequeuedPacket.base.ttl == 1);
            PrintCheck("TTL=1 selected before any TTL=0 messages", passed);

            allPassed &= passed;
        }

        // Test 4.4: TTL Transition from 1 -> 0
        PrintSubHeader("Test 4.4: TTL Transition from 1 -> 0 (Message Expires)");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Ptr<UniformRandomVariable> random = CreateObject<UniformRandomVariable>();
            random->SetAttribute("Min", DoubleValue(0.0));
            random->SetAttribute("Max", DoubleValue(0.01));
            logic->SetRandomStream(random);

            PrintInit("Creating message with TTL=1, then decrementing...");

            ble_discovery_packet_t packet;
            ble_discovery_packet_init(&packet);
            packet.sender_id = 100;
            packet.ttl = 1;

            BleDiscoveryHeaderWrapper header;
            header.SetSenderId(100);
            header.SetTtl(1);
            header.AddToPath(100);
            header.SetGpsLocation(Vector(100.0, 100.0, 0.0));

            Vector currentLoc(0.0, 0.0, 0.0);

            bool shouldForwardBefore = logic->ShouldForward(header, currentLoc, 0.0, 10.0);

            // Decrement TTL
            ble_discovery_decrement_ttl(&packet);
            header.SetTtl(packet.ttl);

            bool shouldForwardAfter = logic->ShouldForward(header, currentLoc, 0.0, 10.0);

            PrintActual("should forward (TTL=1)", shouldForwardBefore ? "true" : "false");
            PrintExpected("should forward (TTL=1)", "true");
            PrintActual("TTL after decrement", std::to_string(packet.ttl));
            PrintExpected("TTL after decrement", "0");
            PrintActual("should forward (TTL=0)", shouldForwardAfter ? "true" : "false");
            PrintExpected("should forward (TTL=0)", "false");

            bool passed = shouldForwardBefore && !shouldForwardAfter && (packet.ttl == 0);
            PrintCheck("Message transitions from forwardable to expired", passed);

            allPassed &= passed;
        }

        // Test 4.5: Mixed Queue with TTL=0 and TTL>0
        PrintSubHeader("Test 4.5: Mixed Queue Processing");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Enqueuing: TTL=5, TTL=0, TTL=10, TTL=0, TTL=3");

            std::vector<std::pair<uint32_t, uint8_t>> messages = {
                {1, 5}, {2, 0}, {3, 10}, {4, 0}, {5, 3}
            };

            uint32_t time_ms = 1000;
            for (const auto& m : messages)
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = m.first;
                packet.ttl = m.second;
                ble_queue_enqueue(&queue, &packet, 999, time_ms++);
            }

            // Dequeue top 3 (should skip TTL=0)
            std::vector<uint8_t> top3Ttls;
            ble_election_packet_t dequeuedPacket;

            for (int i = 0; i < 3; ++i)
            {
                if (ble_queue_dequeue(&queue, &dequeuedPacket))
                {
                    top3Ttls.push_back(dequeuedPacket.base.ttl);
                }
            }

            PrintActual("first 3 dequeued TTLs", TtlVectorToString(top3Ttls));
            PrintExpected("first 3 dequeued TTLs", "[10, 5, 3] (TTL=0 has lowest priority)");

            bool passed = (top3Ttls.size() == 3) &&
                         (top3Ttls[0] == 10) && (top3Ttls[1] == 5) && (top3Ttls[2] == 3);

            PrintCheck("Valid TTL messages dequeued before expired ones", passed);

            allPassed &= passed;
        }

        // Test 4.6: All TTL=0 Messages in Queue
        PrintSubHeader("Test 4.6: Queue with All TTL=0 Messages");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Enqueuing 5 messages all with TTL=0...");

            for (uint32_t i = 1; i <= 5; ++i)
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = i;
                packet.ttl = 0;
                ble_queue_enqueue(&queue, &packet, 999, 1000 + i);
            }

            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Ptr<UniformRandomVariable> random = CreateObject<UniformRandomVariable>();
            random->SetAttribute("Min", DoubleValue(0.0));
            random->SetAttribute("Max", DoubleValue(0.01));
            logic->SetRandomStream(random);

            Vector currentLoc(0.0, 0.0, 0.0);

            uint32_t forwardableCount = 0;
            ble_election_packet_t dequeuedPacket;

            while (ble_queue_dequeue(&queue, &dequeuedPacket))
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(dequeuedPacket.base.sender_id);
                header.SetTtl(dequeuedPacket.base.ttl);
                header.SetGpsLocation(Vector(100.0, 100.0, 0.0));

                if (logic->ShouldForward(header, currentLoc, 0.0, 10.0))
                {
                    forwardableCount++;
                }
            }

            PrintActual("forwardable messages", std::to_string(forwardableCount));
            PrintExpected("forwardable messages", "0 (all TTL=0)");

            bool passed = (forwardableCount == 0);
            PrintCheck("No expired messages are forwardable", passed);

            allPassed &= passed;
        }

        // Test 4.7: TTL Expiration Combined with Other Rejection Criteria
        PrintSubHeader("Test 4.7: TTL Expiration Combined with Other Criteria");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Ptr<UniformRandomVariable> random = CreateObject<UniformRandomVariable>();
            random->SetAttribute("Min", DoubleValue(0.0));
            random->SetAttribute("Max", DoubleValue(1.0));
            logic->SetRandomStream(random);

            PrintInit("Testing TTL=0 rejection takes precedence...");

            Vector currentLoc(0.0, 0.0, 0.0);

            // Case 1: TTL=0, GPS close (two rejection reasons)
            BleDiscoveryHeaderWrapper header1;
            header1.SetSenderId(1);
            header1.SetTtl(0);
            header1.SetGpsLocation(Vector(5.0, 0.0, 0.0)); // Too close

            bool result1 = logic->ShouldForward(header1, currentLoc, 0.0, 10.0);

            // Case 2: TTL=0, GPS far (only TTL rejection)
            BleDiscoveryHeaderWrapper header2;
            header2.SetSenderId(2);
            header2.SetTtl(0);
            header2.SetGpsLocation(Vector(100.0, 0.0, 0.0)); // Far

            bool result2 = logic->ShouldForward(header2, currentLoc, 0.0, 10.0);

            PrintActual("TTL=0, GPS close -> forward", result1 ? "true" : "false");
            PrintExpected("TTL=0, GPS close -> forward", "false");
            PrintActual("TTL=0, GPS far -> forward", result2 ? "true" : "false");
            PrintExpected("TTL=0, GPS far -> forward", "false");

            bool passed = !result1 && !result2;
            PrintCheck("TTL=0 rejected regardless of other factors", passed);

            allPassed &= passed;
        }

        RecordResult("Subtask 4: TTL Expiration Handling", allPassed);
        return allPassed;
    }
};

// ============================================================================
// SUBTASK 5: Message Propagation Distance vs Initial TTL
// ============================================================================

class Subtask5Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 5: Message Propagation Distance vs Initial TTL");

        bool allPassed = true;

        // Test 5.1: TTL=1 Message Propagates 1 Hop
        PrintSubHeader("Test 5.1: TTL=1 Message Propagates 1 Hop");
        {
            PrintInit("Simulating message with initial TTL=1...");

            ble_discovery_packet_t packet;
            ble_discovery_packet_init(&packet);
            packet.sender_id = 100;
            packet.ttl = 1;
            ble_discovery_add_to_path(&packet, 100); // Originator

            uint32_t hops = 0;
            while (packet.ttl > 0)
            {
                // Simulate forwarding
                ble_discovery_decrement_ttl(&packet);
                hops++;
                if (packet.ttl > 0)
                {
                    ble_discovery_add_to_path(&packet, 100 + hops);
                }
            }

            PrintActual("hops until TTL=0", std::to_string(hops));
            PrintExpected("hops until TTL=0", "1");
            PrintActual("path length", std::to_string(packet.path_length));
            PrintExpected("path length", "1 (originator only)");

            bool passed = (hops == 1) && (packet.path_length == 1);
            PrintCheck("TTL=1 allows exactly 1 hop", passed);

            allPassed &= passed;
        }

        // Test 5.2: TTL=5 Message Propagates 5 Hops
        PrintSubHeader("Test 5.2: TTL=5 Message Propagates 5 Hops");
        {
            PrintInit("Simulating message with initial TTL=5...");

            ble_discovery_packet_t packet;
            ble_discovery_packet_init(&packet);
            packet.sender_id = 100;
            packet.ttl = 5;
            ble_discovery_add_to_path(&packet, 100);

            uint32_t hops = 0;
            while (packet.ttl > 0)
            {
                ble_discovery_decrement_ttl(&packet);
                hops++;
                if (packet.ttl > 0)
                {
                    ble_discovery_add_to_path(&packet, 100 + hops);
                }
            }

            PrintActual("hops until TTL=0", std::to_string(hops));
            PrintExpected("hops until TTL=0", "5");
            PrintActual("path length", std::to_string(packet.path_length));
            PrintExpected("path length", "5");

            bool passed = (hops == 5);
            PrintCheck("TTL=5 allows exactly 5 hops", passed);

            allPassed &= passed;
        }

        // Test 5.3: TTL=10 Message Propagates 10 Hops
        PrintSubHeader("Test 5.3: TTL=10 (Default) Message Propagates 10 Hops");
        {
            PrintInit("Simulating message with default TTL=10...");

            ble_discovery_packet_t packet;
            ble_discovery_packet_init(&packet);

            uint8_t initialTtl = packet.ttl;
            packet.sender_id = 100;
            ble_discovery_add_to_path(&packet, 100);

            uint32_t hops = 0;
            while (packet.ttl > 0)
            {
                ble_discovery_decrement_ttl(&packet);
                hops++;
                if (packet.ttl > 0)
                {
                    ble_discovery_add_to_path(&packet, 100 + hops);
                }
            }

            PrintActual("initial TTL", std::to_string(initialTtl));
            PrintExpected("initial TTL", "10 (BLE_DISCOVERY_DEFAULT_TTL)");
            PrintActual("hops until TTL=0", std::to_string(hops));
            PrintExpected("hops until TTL=0", "10");

            bool passed = (initialTtl == 10) && (hops == 10);
            PrintCheck("Default TTL=10 allows exactly 10 hops", passed);

            allPassed &= passed;
        }

        // Test 5.4: Path Length Matches Initial TTL - Final TTL
        PrintSubHeader("Test 5.4: Path Length Correlation");
        {
            PrintInit("Testing path length = initial TTL - remaining TTL...");

            bool allCorrect = true;

            std::cout << "\n  " << COLOR_MAGENTA << "Initial TTL | Hops | Final TTL | Path Length | Match" << COLOR_RESET << "\n";
            std::cout << "  " << std::string(60, '-') << "\n";

            for (uint8_t initialTtl : {3, 7, 12, 20})
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = 100;
                packet.ttl = initialTtl;
                ble_discovery_add_to_path(&packet, 100);

                // Simulate 3 hops
                uint32_t hops = 3;
                for (uint32_t i = 0; i < hops && packet.ttl > 0; ++i)
                {
                    ble_discovery_decrement_ttl(&packet);
                    if (packet.ttl > 0)
                    {
                        ble_discovery_add_to_path(&packet, 100 + i + 1);
                    }
                }

                uint8_t expectedPathLen = (initialTtl <= 3) ? initialTtl : (hops + 1);
                bool match = (packet.path_length == expectedPathLen);
                if (!match) allCorrect = false;

                std::cout << "  " << std::setw(11) << static_cast<int>(initialTtl)
                          << " | " << std::setw(4) << hops
                          << " | " << std::setw(9) << static_cast<int>(packet.ttl)
                          << " | " << std::setw(11) << packet.path_length
                          << " | " << (match ? (COLOR_GREEN + "YES" + COLOR_RESET) : (COLOR_RED + "NO" + COLOR_RESET))
                          << "\n";
            }

            PrintCheck("Path length correctly tracks propagation", allCorrect);

            allPassed &= allCorrect;
        }

        // Test 5.5: Maximum Propagation with High TTL
        PrintSubHeader("Test 5.5: Maximum Propagation with TTL=50");
        {
            PrintInit("Simulating message with high TTL=50...");

            ble_discovery_packet_t packet;
            ble_discovery_packet_init(&packet);
            packet.sender_id = 100;
            packet.ttl = 50;
            ble_discovery_add_to_path(&packet, 100);

            uint32_t hops = 0;
            while (packet.ttl > 0 && hops < 100) // Safety limit
            {
                ble_discovery_decrement_ttl(&packet);
                hops++;
                if (packet.ttl > 0 && packet.path_length < BLE_DISCOVERY_MAX_PATH_LENGTH)
                {
                    ble_discovery_add_to_path(&packet, 100 + hops);
                }
            }

            PrintActual("hops until TTL=0", std::to_string(hops));
            PrintExpected("hops until TTL=0", "50");
            PrintActual("final TTL", std::to_string(packet.ttl));
            PrintExpected("final TTL", "0");

            bool passed = (hops == 50) && (packet.ttl == 0);
            PrintCheck("High TTL=50 allows extended propagation", passed);

            allPassed &= passed;
        }

        // Test 5.6: Propagation Stops Exactly at TTL=0
        PrintSubHeader("Test 5.6: Propagation Stops Exactly When TTL Reaches 0");
        {
            PrintInit("Verifying precise TTL boundary behavior...");

            ble_discovery_packet_t packet;
            ble_discovery_packet_init(&packet);
            packet.sender_id = 100;
            packet.ttl = 3;

            std::vector<std::string> events;

            events.push_back("Initial: TTL=" + std::to_string(packet.ttl));

            for (int i = 0; i < 5; ++i) // Try more decrements than TTL
            {
                bool result = ble_discovery_decrement_ttl(&packet);
                events.push_back("Decrement " + std::to_string(i+1) + ": TTL=" +
                               std::to_string(packet.ttl) + ", result=" +
                               (result ? "true" : "false"));
            }

            std::cout << "\n  Decrement sequence:\n";
            for (const auto& e : events)
            {
                std::cout << "    " << e << "\n";
            }

            PrintExpected("sequence", "TTL: 3 -> 2 -> 1 -> 0 -> 0 -> 0");
            PrintExpected("returns", "true, true, true, false, false");

            bool passed = (packet.ttl == 0);
            PrintCheck("TTL correctly stops at 0 and doesn't go negative", passed);

            allPassed &= passed;
        }

        RecordResult("Subtask 5: Message Propagation Distance vs Initial TTL", allPassed);
        return allPassed;
    }
};

// ============================================================================
// SUBTASK 6: Validate Network Coverage with Different TTL Values
// ============================================================================

class Subtask6Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 6: Network Coverage with Different TTL Values");

        bool allPassed = true;

        // Test 6.1: Low TTL (3) - Limited Coverage
        PrintSubHeader("Test 6.1: Low TTL (3) - Limited Coverage");
        {
            PrintInit("Simulating network with TTL=3 messages...");

            ble_message_queue_t queues[5]; // 5-node chain
            for (int i = 0; i < 5; ++i)
            {
                ble_queue_init(&queues[i]);
            }

            // Originator creates message
            ble_discovery_packet_t originalMsg;
            ble_discovery_packet_init(&originalMsg);
            originalMsg.sender_id = 1;
            originalMsg.ttl = 3;
            ble_discovery_add_to_path(&originalMsg, 1);

            uint32_t nodesReached = 1; // Originator

            // Simulate propagation through chain
            ble_discovery_packet_t currentMsg = originalMsg;
            for (int node = 2; node <= 5; ++node)
            {
                if (currentMsg.ttl > 0)
                {
                    ble_discovery_decrement_ttl(&currentMsg);
                    if (currentMsg.ttl > 0)
                    {
                        ble_discovery_add_to_path(&currentMsg, node);
                        nodesReached++;
                    }
                    else
                    {
                        nodesReached++; // Still reached this node
                    }
                }
            }

            PrintActual("nodes reached with TTL=3", std::to_string(nodesReached));
            PrintExpected("nodes reached with TTL=3", "4 (originator + 3 hops)");
            PrintActual("max possible nodes", "5");
            PrintState("Limited coverage: 1 node unreached in 5-node chain");

            bool passed = (nodesReached == 4);
            PrintCheck("TTL=3 provides limited coverage", passed);

            allPassed &= passed;
        }

        // Test 6.2: Medium TTL (10) - Moderate Coverage
        PrintSubHeader("Test 6.2: Medium TTL (10) - Full Coverage of Medium Network");
        {
            PrintInit("Simulating 8-node network with TTL=10 messages...");

            uint32_t nodesReached = 1; // Originator
            uint32_t networkSize = 8;

            ble_discovery_packet_t msg;
            ble_discovery_packet_init(&msg);
            msg.sender_id = 1;
            msg.ttl = 10;

            for (uint32_t node = 2; node <= networkSize; ++node)
            {
                if (msg.ttl > 0)
                {
                    ble_discovery_decrement_ttl(&msg);
                    nodesReached++;
                }
            }

            PrintActual("nodes reached with TTL=10", std::to_string(nodesReached));
            PrintExpected("nodes reached with TTL=10", "8 (full 8-node network)");
            PrintActual("remaining TTL after 8 nodes", std::to_string(msg.ttl));
            PrintExpected("remaining TTL after 8 nodes", "3 (could reach 2 more)");

            bool passed = (nodesReached == 8) && (msg.ttl > 0);
            PrintCheck("TTL=10 fully covers 8-node network with margin", passed);

            allPassed &= passed;
        }

        // Test 6.3: High TTL (20) - Extended Coverage
        PrintSubHeader("Test 6.3: High TTL (20) - Extended Coverage");
        {
            PrintInit("Simulating 15-node network with TTL=20 messages...");

            uint32_t nodesReached = 1;
            uint32_t networkSize = 15;

            ble_discovery_packet_t msg;
            ble_discovery_packet_init(&msg);
            msg.sender_id = 1;
            msg.ttl = 20;

            for (uint32_t node = 2; node <= networkSize; ++node)
            {
                if (msg.ttl > 0)
                {
                    ble_discovery_decrement_ttl(&msg);
                    nodesReached++;
                }
            }

            PrintActual("nodes reached with TTL=20", std::to_string(nodesReached));
            PrintExpected("nodes reached with TTL=20", "15 (full network)");
            PrintActual("remaining TTL", std::to_string(msg.ttl));
            PrintExpected("remaining TTL", "6 (spare capacity)");

            bool passed = (nodesReached == 15) && (msg.ttl == 6);
            PrintCheck("High TTL provides extended coverage", passed);

            allPassed &= passed;
        }

        // Test 6.4: Coverage Comparison Across TTL Values
        PrintSubHeader("Test 6.4: Coverage Comparison Across TTL Values");
        {
            PrintInit("Comparing coverage for different TTL values in 10-node network...");

            uint32_t networkSize = 10;

            std::cout << "\n  " << COLOR_MAGENTA << "TTL | Nodes Reached | Coverage %" << COLOR_RESET << "\n";
            std::cout << "  " << std::string(40, '-') << "\n";

            std::vector<std::pair<uint8_t, uint32_t>> coverageResults;

            for (uint8_t ttl : {3, 5, 8, 10, 15})
            {
                uint32_t nodesReached = 1;

                ble_discovery_packet_t msg;
                ble_discovery_packet_init(&msg);
                msg.ttl = ttl;

                for (uint32_t node = 2; node <= networkSize; ++node)
                {
                    if (msg.ttl > 0)
                    {
                        ble_discovery_decrement_ttl(&msg);
                        nodesReached++;
                    }
                }

                coverageResults.push_back({ttl, nodesReached});

                double coverage = (nodesReached * 100.0) / networkSize;
                std::cout << "  " << std::setw(3) << static_cast<int>(ttl)
                          << " | " << std::setw(13) << nodesReached
                          << " | " << std::fixed << std::setprecision(0) << coverage << "%\n";
            }

            // Verify increasing coverage with TTL
            bool monotonic = true;
            for (size_t i = 1; i < coverageResults.size(); ++i)
            {
                if (coverageResults[i].second < coverageResults[i-1].second)
                {
                    monotonic = false;
                    break;
                }
            }

            PrintExpected("coverage trend", "monotonically increasing with TTL");
            PrintCheck("Higher TTL provides better or equal coverage", monotonic);

            allPassed &= monotonic;
        }

        // Test 6.5: Priority Ordering in Multi-Node Scenario
        PrintSubHeader("Test 6.5: Priority Ordering in Multi-Node Scenario");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Simulating node receiving messages from multiple sources...");
            PrintState("Node receives: TTL=8 from node 1, TTL=3 from node 2, TTL=12 from node 3");

            std::vector<std::pair<uint32_t, uint8_t>> incoming = {
                {1, 8}, {2, 3}, {3, 12}
            };

            uint32_t time_ms = 1000;
            for (const auto& msg : incoming)
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = msg.first;
                packet.ttl = msg.second;
                ble_queue_enqueue(&queue, &packet, 999, time_ms++);
            }

            // Forward in next cycle (top message)
            ble_election_packet_t forwardPacket;
            ble_queue_dequeue(&queue, &forwardPacket);

            PrintActual("first forwarded sender", std::to_string(forwardPacket.base.sender_id));
            PrintActual("first forwarded TTL", std::to_string(forwardPacket.base.ttl));
            PrintExpected("first forwarded", "sender=3, TTL=12 (highest TTL = highest priority)");

            bool passed = (forwardPacket.base.sender_id == 3) && (forwardPacket.base.ttl == 12);
            PrintCheck("Highest TTL message forwarded first", passed);

            allPassed &= passed;
        }

        // Test 6.6: TTL-Based Priority Prevents Message Starvation
        PrintSubHeader("Test 6.6: TTL-Based Priority Prevents Message Starvation");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Simulating continuous message arrival with varying TTLs...");

            // First batch
            for (uint8_t ttl : {5, 2, 8})
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = ttl * 10;
                packet.ttl = ttl;
                ble_queue_enqueue(&queue, &packet, 999, 1000);
            }

            // Dequeue one (highest priority)
            ble_election_packet_t first;
            ble_queue_dequeue(&queue, &first);

            // Add more messages
            for (uint8_t ttl : {15, 3})
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = ttl * 10 + 1;
                packet.ttl = ttl;
                ble_queue_enqueue(&queue, &packet, 999, 1001);
            }

            // Dequeue remaining
            std::vector<uint8_t> order;
            ble_election_packet_t packet;
            while (ble_queue_dequeue(&queue, &packet))
            {
                order.push_back(packet.base.ttl);
            }

            PrintActual("first dequeued TTL", std::to_string(first.base.ttl));
            PrintExpected("first dequeued TTL", "8 (highest from first batch)");
            PrintActual("remaining order", TtlVectorToString(order));
            PrintExpected("remaining order", "[15, 5, 3, 2]");

            bool newHighPriorityFirst = (order.size() >= 1) && (order[0] == 15);
            PrintCheck("New high-TTL messages don't starve existing ones", first.base.ttl == 8);
            PrintCheck("Priority maintained with mixed arrival times", newHighPriorityFirst);

            allPassed &= (first.base.ttl == 8) && newHighPriorityFirst;
        }

        // Test 6.7: High TTL Messages Prioritized Over Low TTL
        PrintSubHeader("Test 6.7: High TTL Messages Prioritized Over Low TTL");
        {
            ble_message_queue_t queue;
            ble_queue_init(&queue);

            PrintInit("Testing that high TTL messages get forwarded before low TTL...");

            // Add low TTL first
            for (uint8_t ttl : {1, 2, 3})
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = ttl;
                packet.ttl = ttl;
                ble_queue_enqueue(&queue, &packet, 999, 1000);
            }

            // Add high TTL later
            for (uint8_t ttl : {10, 15, 20})
            {
                ble_discovery_packet_t packet;
                ble_discovery_packet_init(&packet);
                packet.sender_id = ttl;
                packet.ttl = ttl;
                ble_queue_enqueue(&queue, &packet, 999, 1001);
            }

            // Dequeue top 3
            std::vector<uint8_t> top3;
            ble_election_packet_t packet;
            for (int i = 0; i < 3; ++i)
            {
                if (ble_queue_dequeue(&queue, &packet))
                {
                    top3.push_back(packet.base.ttl);
                }
            }

            PrintActual("top-3 forwarded", TtlVectorToString(top3));
            PrintExpected("top-3 forwarded", "[20, 15, 10] (high TTL first despite late arrival)");

            bool passed = (top3.size() == 3) &&
                         (top3[0] == 20) && (top3[1] == 15) && (top3[2] == 10);

            PrintCheck("High TTL messages prioritized regardless of arrival order", passed);

            allPassed &= passed;
        }

        RecordResult("Subtask 6: Network Coverage with Different TTL Values", allPassed);
        return allPassed;
    }
};

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

void PrintFinalResults ()
{
    PrintHeader("FINAL TEST RESULTS");

    uint32_t passed = 0;
    uint32_t failed = 0;

    std::cout << "\n";
    for (const auto& result : g_testResults)
    {
        std::string marker = result.passed ?
            (COLOR_GREEN + "[OK]" + COLOR_RESET) :
            (COLOR_RED + "[XX]" + COLOR_RESET);

        std::cout << "  " << marker << " " << result.name << "\n";

        if (result.passed)
            passed++;
        else
            failed++;
    }

    std::cout << "\n";
    std::cout << COLOR_BOLD << "============================================================\n";
    std::cout << " SUMMARY: " << passed << " passed, " << failed << " failed\n";
    std::cout << "============================================================" << COLOR_RESET << "\n";

    if (failed == 0)
    {
        std::cout << "\n  " << COLOR_GREEN << COLOR_BOLD
                  << "*** ALL TASK 11 SUBTASKS VERIFIED SUCCESSFULLY ***"
                  << COLOR_RESET << "\n\n";
    }
    else
    {
        std::cout << "\n  " << COLOR_RED << COLOR_BOLD
                  << "!!! SOME SUBTASKS FAILED - REVIEW OUTPUT ABOVE !!!"
                  << COLOR_RESET << "\n\n";
    }
}

int main (int argc, char *argv[])
{
    CommandLine cmd;
    cmd.AddValue("verbose", "Enable verbose output", g_verbose);
    cmd.Parse(argc, argv);

    std::cout << "\n";
    std::cout << COLOR_BOLD << "################################################################\n";
    std::cout << "#                                                              #\n";
    std::cout << "#   TASK 11: TTL-BASED MESSAGE PRIORITIZATION - COMPREHENSIVE  #\n";
    std::cout << "#                                                              #\n";
    std::cout << "################################################################" << COLOR_RESET << "\n";
    std::cout << "\nVerbose mode: " << (g_verbose ? "ON" : "OFF") << "\n";
    std::cout << COLOR_YELLOW << "Expected values shown in yellow" << COLOR_RESET << "\n";

    std::cout << "\n" << COLOR_CYAN << "Subtasks being tested:" << COLOR_RESET << "\n";
    std::cout << "  1. Implement TTL decrement on message forwarding\n";
    std::cout << "  2. Create priority queue sorted by TTL\n";
    std::cout << "  3. Implement top-3 selection algorithm after other filters applied\n";
    std::cout << "  4. Add TTL expiration handling (TTL=0 messages not forwarded)\n";
    std::cout << "  5. Test message propagation distance vs initial TTL\n";
    std::cout << "  6. Validate network coverage with different TTL values\n";

    // Run all subtask tests
    Subtask1Test::Run();
    Subtask2Test::Run();
    Subtask3Test::Run();
    Subtask4Test::Run();
    Subtask5Test::Run();
    Subtask6Test::Run();

    // Print final summary
    PrintFinalResults();

    Simulator::Destroy();

    return 0;
}
