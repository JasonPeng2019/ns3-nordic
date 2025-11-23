/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025
 *
 * Task 8 Comprehensive Test: Message Forwarding Queue
 *
 * This simulation tests all subtasks for Task 8:
 * - Subtask 1: Message queue data structure for received discovery messages
 * - Subtask 2: Queue management (add, remove, prioritize)
 * - Subtask 3: Message deduplication logic (track seen messages)
 * - Subtask 4: PSF loop detection (message not forwarded if node already in PSF)
 * - Subtask 5: Queue size limits and overflow handling
 * - Subtask 6: Queue behavior under high message load
 *
 * Usage:
 *   ./waf --run "task-8-test --verbose=true"
 */

#include "ns3/core-module.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/ble-message-queue.h"
#include "ns3/ble-discovery-header-wrapper.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Task8Test");

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

std::string PathToString (const std::vector<uint32_t>& path)
{
    if (path.empty()) return "[]";
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < path.size(); i++)
    {
        ss << path[i];
        if (i < path.size() - 1) ss << " -> ";
    }
    ss << "]";
    return ss.str();
}

void PrintQueueState (Ptr<BleMessageQueue> queue, const std::string& label)
{
    uint32_t enqueued, dequeued, duplicates, loops, overflows;
    queue->GetStatistics(enqueued, dequeued, duplicates, loops, overflows);

    std::cout << COLOR_CYAN << "[QUEUE STATE: " << label << "]" << COLOR_RESET << "\n";
    std::cout << "  Size: " << queue->GetSize() << " | Empty: " << (queue->IsEmpty() ? "yes" : "no") << "\n";
    std::cout << "  Stats: enqueued=" << enqueued << ", dequeued=" << dequeued
              << ", duplicates=" << duplicates << ", loops=" << loops
              << ", overflows=" << overflows << "\n";
}

// ============================================================================
// SUBTASK 1: Message Queue Data Structure
// ============================================================================

class Subtask1Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 1: Message Queue Data Structure");

        bool allPassed = true;

        // Test 1.1: Queue Initialization
        PrintSubHeader("Test 1.1: Queue Initialization");
        {
            PrintInit("Creating BleMessageQueue object...");
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            PrintQueueState(queue, "After initialization");

            uint32_t size = queue->GetSize();
            bool isEmpty = queue->IsEmpty();

            PrintActual("size", std::to_string(size));
            PrintExpected("size", "0");
            PrintActual("isEmpty", isEmpty ? "true" : "false");
            PrintExpected("isEmpty", "true");

            bool sizePassed = (size == 0);
            bool emptyPassed = isEmpty;

            PrintCheck("Queue size is 0 after initialization", sizePassed);
            PrintCheck("Queue reports empty after initialization", emptyPassed);

            allPassed &= sizePassed && emptyPassed;
        }

        // Test 1.2: Statistics Initialization
        PrintSubHeader("Test 1.2: Statistics Counters Initialization");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            uint32_t enqueued, dequeued, duplicates, loops, overflows;
            queue->GetStatistics(enqueued, dequeued, duplicates, loops, overflows);

            PrintInit("Checking all statistics counters...");

            PrintActual("totalEnqueued", std::to_string(enqueued));
            PrintExpected("totalEnqueued", "0");
            PrintActual("totalDequeued", std::to_string(dequeued));
            PrintExpected("totalDequeued", "0");
            PrintActual("totalDuplicates", std::to_string(duplicates));
            PrintExpected("totalDuplicates", "0");
            PrintActual("totalLoops", std::to_string(loops));
            PrintExpected("totalLoops", "0");
            PrintActual("totalOverflows", std::to_string(overflows));
            PrintExpected("totalOverflows", "0");

            bool passed = (enqueued == 0) && (dequeued == 0) && (duplicates == 0)
                         && (loops == 0) && (overflows == 0);

            PrintCheck("All statistics counters initialized to 0", passed);
            allPassed &= passed;
        }

        // Test 1.3: Entry Structure Verification
        PrintSubHeader("Test 1.3: Queue Entry Stores All Required Fields");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            PrintInit("Creating discovery message with all fields...");

            BleDiscoveryHeaderWrapper header;
            header.SetSenderId(12345);
            header.SetTtl(10);
            header.AddToPath(12345);
            header.AddToPath(100);
            header.SetGpsLocation(Vector(37.7749, -122.4194, 50.0));
            header.SetGpsAvailable(true);

            PrintState("Message: sender=12345, TTL=10, path=[12345,100], GPS=(37.77,-122.42,50)");

            Ptr<Packet> packet = Create<Packet>();
            bool enqueueResult = queue->Enqueue(packet, header, 999); // Node 999 receiving

            PrintActual("enqueueResult", enqueueResult ? "true" : "false");
            PrintExpected("enqueueResult", "true");

            // Dequeue and verify fields preserved
            BleDiscoveryHeaderWrapper dequeuedHeader;
            Ptr<Packet> dequeuedPacket = queue->Dequeue(dequeuedHeader);

            PrintState("Dequeued message, verifying fields preserved...");

            PrintActual("sender_id", std::to_string(dequeuedHeader.GetSenderId()));
            PrintExpected("sender_id", "12345");
            PrintActual("TTL", std::to_string(dequeuedHeader.GetTtl()));
            PrintExpected("TTL", "10");
            PrintActual("path", PathToString(dequeuedHeader.GetPath()));
            PrintExpected("path", "[12345 -> 100]");

            bool senderOk = (dequeuedHeader.GetSenderId() == 12345);
            bool ttlOk = (dequeuedHeader.GetTtl() == 10);
            bool pathOk = (dequeuedHeader.GetPath().size() == 2);

            PrintCheck("Sender ID preserved", senderOk);
            PrintCheck("TTL preserved", ttlOk);
            PrintCheck("Path preserved", pathOk);

            allPassed &= senderOk && ttlOk && pathOk;
        }

        RecordResult("Subtask 1: Message Queue Data Structure", allPassed);
        return allPassed;
    }
};

// ============================================================================
// SUBTASK 2: Queue Management (Add, Remove, Prioritize)
// ============================================================================

class Subtask2Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 2: Queue Management (Add, Remove, Prioritize)");

        bool allPassed = true;

        // Test 2.1: Basic Enqueue/Dequeue
        PrintSubHeader("Test 2.1: Basic Enqueue and Dequeue");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            PrintInit("Creating message: sender=100, TTL=5");

            BleDiscoveryHeaderWrapper header;
            header.SetSenderId(100);
            header.SetTtl(5);
            header.AddToPath(100);

            Ptr<Packet> packet = Create<Packet>();
            bool enqueueResult = queue->Enqueue(packet, header, 999);

            PrintQueueState(queue, "After enqueue");

            PrintActual("enqueueResult", enqueueResult ? "true" : "false");
            PrintExpected("enqueueResult", "true");
            PrintActual("queueSize", std::to_string(queue->GetSize()));
            PrintExpected("queueSize", "1");

            BleDiscoveryHeaderWrapper dequeuedHeader;
            Ptr<Packet> dequeuedPacket = queue->Dequeue(dequeuedHeader);

            PrintQueueState(queue, "After dequeue");

            PrintActual("dequeuedPacket", dequeuedPacket != nullptr ? "valid" : "nullptr");
            PrintExpected("dequeuedPacket", "valid");
            PrintActual("queueSize", std::to_string(queue->GetSize()));
            PrintExpected("queueSize", "0");

            bool enqOk = enqueueResult;
            bool deqOk = (dequeuedPacket != nullptr);
            bool sizeOk = (queue->GetSize() == 0);

            PrintCheck("Enqueue succeeds", enqOk);
            PrintCheck("Dequeue returns valid packet", deqOk);
            PrintCheck("Queue empty after dequeue", sizeOk);

            allPassed &= enqOk && deqOk && sizeOk;
        }

        // Test 2.2: Priority Ordering (TTL-based)
        PrintSubHeader("Test 2.2: Priority Ordering (Higher TTL = Higher Priority)");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            PrintInit("Enqueueing 3 messages with different TTLs (out of order)...");

            // Enqueue in order: TTL=5, TTL=15, TTL=2
            BleDiscoveryHeaderWrapper h1, h2, h3;
            h1.SetSenderId(1); h1.SetTtl(5);  h1.AddToPath(1);
            h2.SetSenderId(2); h2.SetTtl(15); h2.AddToPath(2);
            h3.SetSenderId(3); h3.SetTtl(2);  h3.AddToPath(3);

            Ptr<Packet> p = Create<Packet>();

            PrintState("Enqueue order: sender=1 (TTL=5), sender=2 (TTL=15), sender=3 (TTL=2)");

            queue->Enqueue(p, h1, 999);
            queue->Enqueue(p, h2, 999);
            queue->Enqueue(p, h3, 999);

            PrintQueueState(queue, "After 3 enqueues");

            // Dequeue should return highest TTL first
            BleDiscoveryHeaderWrapper d1, d2, d3;
            queue->Dequeue(d1);
            queue->Dequeue(d2);
            queue->Dequeue(d3);

            PrintState("Dequeue order (expected: TTL 15, 5, 2):");
            PrintActual("1st dequeue TTL", std::to_string(d1.GetTtl()));
            PrintExpected("1st dequeue TTL", "15");
            PrintActual("2nd dequeue TTL", std::to_string(d2.GetTtl()));
            PrintExpected("2nd dequeue TTL", "5");
            PrintActual("3rd dequeue TTL", std::to_string(d3.GetTtl()));
            PrintExpected("3rd dequeue TTL", "2");

            bool firstOk = (d1.GetTtl() == 15);
            bool secondOk = (d2.GetTtl() == 5);
            bool thirdOk = (d3.GetTtl() == 2);

            PrintCheck("Highest TTL (15) dequeued first", firstOk);
            PrintCheck("Medium TTL (5) dequeued second", secondOk);
            PrintCheck("Lowest TTL (2) dequeued third", thirdOk);

            allPassed &= firstOk && secondOk && thirdOk;
        }

        // Test 2.3: Peek Without Remove
        PrintSubHeader("Test 2.3: Peek Without Removing");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            BleDiscoveryHeaderWrapper header;
            header.SetSenderId(555);
            header.SetTtl(8);
            header.AddToPath(555);

            Ptr<Packet> p = Create<Packet>();
            queue->Enqueue(p, header, 999);

            PrintInit("Queue has 1 message (sender=555, TTL=8)");

            uint32_t sizeBefore = queue->GetSize();

            BleDiscoveryHeaderWrapper peekHeader;
            bool peekResult = queue->Peek(peekHeader);

            uint32_t sizeAfter = queue->GetSize();

            PrintActual("sizeBefore", std::to_string(sizeBefore));
            PrintActual("peekResult", peekResult ? "true" : "false");
            PrintActual("peekedSenderId", std::to_string(peekHeader.GetSenderId()));
            PrintActual("sizeAfter", std::to_string(sizeAfter));
            PrintExpected("sizeAfter", "1 (unchanged)");

            bool peekOk = peekResult && (peekHeader.GetSenderId() == 555);
            bool sizeUnchanged = (sizeBefore == sizeAfter);

            PrintCheck("Peek returns correct message", peekOk);
            PrintCheck("Queue size unchanged after peek", sizeUnchanged);

            allPassed &= peekOk && sizeUnchanged;
        }

        // Test 2.4: Empty Queue Behavior
        PrintSubHeader("Test 2.4: Empty Queue Behavior");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            PrintInit("Attempting to dequeue from empty queue...");

            BleDiscoveryHeaderWrapper header;
            Ptr<Packet> result = queue->Dequeue(header);

            PrintActual("dequeueResult", result != nullptr ? "valid packet" : "nullptr");
            PrintExpected("dequeueResult", "nullptr");

            bool peekResult = queue->Peek(header);
            PrintActual("peekResult", peekResult ? "true" : "false");
            PrintExpected("peekResult", "false");

            bool deqOk = (result == nullptr);
            bool peekOk = !peekResult;

            PrintCheck("Dequeue from empty queue returns nullptr", deqOk);
            PrintCheck("Peek on empty queue returns false", peekOk);

            allPassed &= deqOk && peekOk;
        }

        RecordResult("Subtask 2: Queue Management (Add, Remove, Prioritize)", allPassed);
        return allPassed;
    }
};

// ============================================================================
// SUBTASK 3: Message Deduplication Logic
// ============================================================================

class Subtask3Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 3: Message Deduplication Logic");

        bool allPassed = true;

        // Test 3.1: First Message Accepted
        PrintSubHeader("Test 3.1: First Occurrence Accepted");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            BleDiscoveryHeaderWrapper header;
            header.SetSenderId(100);
            header.SetTtl(10);
            header.AddToPath(100);

            PrintInit("First message: sender=100, TTL=10");

            Ptr<Packet> p = Create<Packet>();
            bool result = queue->Enqueue(p, header, 999);

            PrintActual("enqueueResult", result ? "true" : "false");
            PrintExpected("enqueueResult", "true");

            PrintCheck("First message accepted", result);
            allPassed &= result;
        }

        // Test 3.2: Duplicate Rejected
        PrintSubHeader("Test 3.2: Duplicate Message Rejected");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            BleDiscoveryHeaderWrapper header;
            header.SetSenderId(200);
            header.SetTtl(8);
            header.AddToPath(200);

            Ptr<Packet> p = Create<Packet>();

            PrintInit("Enqueueing same message twice (sender=200, TTL=8)");

            bool firstResult = queue->Enqueue(p, header, 999);
            PrintState("First enqueue...");
            PrintActual("firstResult", firstResult ? "true" : "false");
            PrintExpected("firstResult", "true");

            bool secondResult = queue->Enqueue(p, header, 999);
            PrintState("Second enqueue (duplicate)...");
            PrintActual("secondResult", secondResult ? "true" : "false");
            PrintExpected("secondResult", "false (rejected as duplicate)");

            PrintQueueState(queue, "After duplicate attempt");

            uint32_t enq, deq, dups, loops, over;
            queue->GetStatistics(enq, deq, dups, loops, over);

            PrintActual("totalDuplicates", std::to_string(dups));
            PrintExpected("totalDuplicates", "1");

            bool firstOk = firstResult;
            bool secondRejected = !secondResult;
            bool statsOk = (dups == 1);

            PrintCheck("First message accepted", firstOk);
            PrintCheck("Duplicate message rejected", secondRejected);
            PrintCheck("Duplicate counter incremented", statsOk);

            allPassed &= firstOk && secondRejected && statsOk;
        }

        // Test 3.3: Different Senders OK
        PrintSubHeader("Test 3.3: Same Content, Different Senders Accepted");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            BleDiscoveryHeaderWrapper h1, h2;
            h1.SetSenderId(300); h1.SetTtl(5); h1.AddToPath(300);
            h2.SetSenderId(301); h2.SetTtl(5); h2.AddToPath(301);

            Ptr<Packet> p = Create<Packet>();

            PrintInit("Two messages with same TTL but different senders");
            PrintState("Message 1: sender=300, TTL=5");
            PrintState("Message 2: sender=301, TTL=5");

            bool r1 = queue->Enqueue(p, h1, 999);
            bool r2 = queue->Enqueue(p, h2, 999);

            PrintActual("message1 accepted", r1 ? "true" : "false");
            PrintExpected("message1 accepted", "true");
            PrintActual("message2 accepted", r2 ? "true" : "false");
            PrintExpected("message2 accepted", "true");
            PrintActual("queueSize", std::to_string(queue->GetSize()));
            PrintExpected("queueSize", "2");

            bool passed = r1 && r2 && (queue->GetSize() == 2);
            PrintCheck("Both messages from different senders accepted", passed);

            allPassed &= passed;
        }

        // Test 3.4: Cache Cleanup Allows Re-add
        PrintSubHeader("Test 3.4: Cache Cleanup Allows Re-adding Old Message");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            BleDiscoveryHeaderWrapper header;
            header.SetSenderId(400);
            header.SetTtl(7);
            header.AddToPath(400);

            Ptr<Packet> p = Create<Packet>();

            PrintInit("Enqueue message at t=0, advance time, clean cache, then try re-enqueue");

            bool first = queue->Enqueue(p, header, 999);
            PrintState("First enqueue at t=0ms: " + std::string(first ? "accepted" : "rejected"));

            // Dequeue to make room
            BleDiscoveryHeaderWrapper temp;
            queue->Dequeue(temp);

            // Advance simulation time so the cached entry becomes "old"
            // The entry was added at t=0ms. We advance to t=100ms.
            // Then cleaning with maxAge=50ms will remove entries older than 50ms.
            // Entry age = 100ms - 0ms = 100ms, which is > 50ms, so it gets removed.
            Simulator::Stop(MilliSeconds(100));
            Simulator::Run();
            PrintState("Advanced simulation time to t=100ms");

            // Clean old entries (50ms max age = remove entries older than 50ms)
            PrintState("Cleaning seen cache with maxAge=50ms (entry is 100ms old)...");
            queue->CleanOldEntries(MilliSeconds(50));

            bool second = queue->Enqueue(p, header, 999);
            PrintState("Second enqueue after cleanup: " + std::string(second ? "accepted" : "rejected"));

            PrintActual("firstEnqueue", first ? "true" : "false");
            PrintExpected("firstEnqueue", "true");
            PrintActual("secondEnqueue (after cleanup)", second ? "true" : "false");
            PrintExpected("secondEnqueue", "true");

            bool passed = first && second;
            PrintCheck("Message re-accepted after cache cleanup", passed);

            allPassed &= passed;
        }

        RecordResult("Subtask 3: Message Deduplication Logic", allPassed);
        return allPassed;
    }
};

// ============================================================================
// SUBTASK 4: PSF Loop Detection
// ============================================================================

class Subtask4Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 4: PSF Loop Detection");

        bool allPassed = true;

        // Test 4.1: No Loop - Empty Path
        PrintSubHeader("Test 4.1: No Loop - Empty Path");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            BleDiscoveryHeaderWrapper header;
            header.SetSenderId(100);
            header.SetTtl(10);
            // No path added - empty PSF

            PrintInit("Message with empty PSF, receiving node=5");
            PrintState("PSF: [] (empty)");

            Ptr<Packet> p = Create<Packet>();
            bool result = queue->Enqueue(p, header, 5);

            PrintActual("enqueueResult", result ? "true" : "false");
            PrintExpected("enqueueResult", "true (no loop)");

            PrintCheck("Message with empty path accepted", result);
            allPassed &= result;
        }

        // Test 4.2: No Loop - Node Not in Path
        PrintSubHeader("Test 4.2: No Loop - Receiving Node Not in Path");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            BleDiscoveryHeaderWrapper header;
            header.SetSenderId(100);
            header.SetTtl(10);
            header.AddToPath(100);
            header.AddToPath(101);
            header.AddToPath(102);

            PrintInit("Message with PSF=[100,101,102], receiving node=5");
            PrintState("Node 5 is NOT in path -> should be accepted");

            Ptr<Packet> p = Create<Packet>();
            bool result = queue->Enqueue(p, header, 5);

            PrintActual("enqueueResult", result ? "true" : "false");
            PrintExpected("enqueueResult", "true (node 5 not in path)");

            PrintCheck("Message accepted when node not in path", result);
            allPassed &= result;
        }

        // Test 4.3: Loop Detected - Node at Start of Path
        PrintSubHeader("Test 4.3: Loop Detected - Node at Start of Path");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            BleDiscoveryHeaderWrapper header;
            header.SetSenderId(5);
            header.SetTtl(10);
            header.AddToPath(5);   // Node 5 is first in path
            header.AddToPath(101);
            header.AddToPath(102);

            PrintInit("Message with PSF=[5,101,102], receiving node=5");
            PrintState("Node 5 IS in path (at start) -> LOOP DETECTED");

            Ptr<Packet> p = Create<Packet>();
            bool result = queue->Enqueue(p, header, 5);

            uint32_t enq, deq, dups, loops, over;
            queue->GetStatistics(enq, deq, dups, loops, over);

            PrintActual("enqueueResult", result ? "true" : "false");
            PrintExpected("enqueueResult", "false (loop detected)");
            PrintActual("totalLoops", std::to_string(loops));
            PrintExpected("totalLoops", "1");

            bool rejected = !result;
            bool statsOk = (loops == 1);

            PrintCheck("Message rejected due to loop", rejected);
            PrintCheck("Loop counter incremented", statsOk);

            allPassed &= rejected && statsOk;
        }

        // Test 4.4: Loop Detected - Node in Middle of Path
        PrintSubHeader("Test 4.4: Loop Detected - Node in Middle of Path");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            BleDiscoveryHeaderWrapper header;
            header.SetSenderId(100);
            header.SetTtl(10);
            header.AddToPath(100);
            header.AddToPath(5);   // Node 5 in middle
            header.AddToPath(102);

            PrintInit("Message with PSF=[100,5,102], receiving node=5");
            PrintState("Node 5 IS in path (middle) -> LOOP DETECTED");

            Ptr<Packet> p = Create<Packet>();
            bool result = queue->Enqueue(p, header, 5);

            PrintActual("enqueueResult", result ? "true" : "false");
            PrintExpected("enqueueResult", "false (loop detected)");

            PrintCheck("Message rejected when node in middle of path", !result);
            allPassed &= !result;
        }

        // Test 4.5: Loop Detected - Node at End of Path
        PrintSubHeader("Test 4.5: Loop Detected - Node at End of Path");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            BleDiscoveryHeaderWrapper header;
            header.SetSenderId(100);
            header.SetTtl(10);
            header.AddToPath(100);
            header.AddToPath(101);
            header.AddToPath(5);   // Node 5 at end

            PrintInit("Message with PSF=[100,101,5], receiving node=5");
            PrintState("Node 5 IS in path (at end) -> LOOP DETECTED");

            Ptr<Packet> p = Create<Packet>();
            bool result = queue->Enqueue(p, header, 5);

            PrintActual("enqueueResult", result ? "true" : "false");
            PrintExpected("enqueueResult", "false (loop detected)");

            PrintCheck("Message rejected when node at end of path", !result);
            allPassed &= !result;
        }

        // Test 4.6: Multiple Loop Detections Statistics
        PrintSubHeader("Test 4.6: Multiple Loop Detections Statistics");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            PrintInit("Sending 3 messages that will all cause loops at node 7");

            for (int i = 0; i < 3; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(100 + i);
                header.SetTtl(10);
                header.AddToPath(100 + i);
                header.AddToPath(7);  // Node 7 in all paths

                Ptr<Packet> p = Create<Packet>();
                queue->Enqueue(p, header, 7);
            }

            uint32_t enq, deq, dups, loops, over;
            queue->GetStatistics(enq, deq, dups, loops, over);

            PrintActual("totalLoops", std::to_string(loops));
            PrintExpected("totalLoops", "3");
            PrintActual("queueSize", std::to_string(queue->GetSize()));
            PrintExpected("queueSize", "0");

            bool passed = (loops == 3) && (queue->GetSize() == 0);
            PrintCheck("All 3 loop detections recorded", passed);

            allPassed &= passed;
        }

        RecordResult("Subtask 4: PSF Loop Detection", allPassed);
        return allPassed;
    }
};

// ============================================================================
// SUBTASK 5: Queue Size Limits and Overflow Handling
// ============================================================================

class Subtask5Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 5: Queue Size Limits and Overflow Handling");

        bool allPassed = true;

        // Test 5.1: Fill to Capacity
        PrintSubHeader("Test 5.1: Fill Queue to Capacity (100 messages)");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            PrintInit("Enqueueing 100 unique messages...");

            uint32_t accepted = 0;
            for (uint32_t i = 0; i < 100; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                if (queue->Enqueue(p, header, 999))
                {
                    accepted++;
                }
            }

            PrintActual("messagesAccepted", std::to_string(accepted));
            PrintExpected("messagesAccepted", "100");
            PrintActual("queueSize", std::to_string(queue->GetSize()));
            PrintExpected("queueSize", "100");

            bool passed = (accepted == 100) && (queue->GetSize() == 100);
            PrintCheck("All 100 messages accepted (queue at capacity)", passed);

            allPassed &= passed;
        }

        // Test 5.2: Overflow Rejection
        PrintSubHeader("Test 5.2: Overflow Rejection (101st Message)");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            PrintInit("Filling queue to 100, then attempting 101st...");

            // Fill to capacity
            for (uint32_t i = 0; i < 100; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                queue->Enqueue(p, header, 999);
            }

            PrintState("Queue now at capacity (100)");

            // Try to add 101st
            BleDiscoveryHeaderWrapper overflowHeader;
            overflowHeader.SetSenderId(1000);
            overflowHeader.SetTtl(10);
            overflowHeader.AddToPath(1000);

            Ptr<Packet> p = Create<Packet>();
            bool result = queue->Enqueue(p, overflowHeader, 999);

            PrintActual("101st enqueue result", result ? "true" : "false");
            PrintExpected("101st enqueue result", "false (overflow)");

            PrintCheck("101st message rejected (overflow)", !result);
            allPassed &= !result;
        }

        // Test 5.3: Overflow Statistics
        PrintSubHeader("Test 5.3: Overflow Statistics Counter");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            PrintInit("Filling queue, then attempting 50 more (overflow)...");

            // Fill to capacity
            for (uint32_t i = 0; i < 100; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                queue->Enqueue(p, header, 999);
            }

            // Try 50 more (all should overflow)
            for (uint32_t i = 100; i < 150; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                queue->Enqueue(p, header, 999);
            }

            uint32_t enq, deq, dups, loops, over;
            queue->GetStatistics(enq, deq, dups, loops, over);

            PrintQueueState(queue, "After overflow attempts");

            PrintActual("totalOverflows", std::to_string(over));
            PrintExpected("totalOverflows", "50");
            PrintActual("queueSize", std::to_string(queue->GetSize()));
            PrintExpected("queueSize", "100 (capped)");

            bool overflowOk = (over == 50);
            bool sizeOk = (queue->GetSize() == 100);

            PrintCheck("50 overflows recorded", overflowOk);
            PrintCheck("Queue size remains at 100", sizeOk);

            allPassed &= overflowOk && sizeOk;
        }

        // Test 5.4: Recovery After Dequeue
        PrintSubHeader("Test 5.4: Recovery After Dequeue");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            // Fill to capacity
            for (uint32_t i = 0; i < 100; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                queue->Enqueue(p, header, 999);
            }

            PrintInit("Queue full (100). Dequeuing 5, then adding 5 new...");

            // Dequeue 5
            for (int i = 0; i < 5; i++)
            {
                BleDiscoveryHeaderWrapper temp;
                queue->Dequeue(temp);
            }

            PrintState("After dequeue 5: size = " + std::to_string(queue->GetSize()));

            // Should be able to add 5 more
            uint32_t newAccepted = 0;
            for (uint32_t i = 200; i < 205; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                if (queue->Enqueue(p, header, 999))
                {
                    newAccepted++;
                }
            }

            PrintActual("newMessagesAccepted", std::to_string(newAccepted));
            PrintExpected("newMessagesAccepted", "5");
            PrintActual("finalQueueSize", std::to_string(queue->GetSize()));
            PrintExpected("finalQueueSize", "100");

            bool passed = (newAccepted == 5) && (queue->GetSize() == 100);
            PrintCheck("5 new messages accepted after dequeue", passed);

            allPassed &= passed;
        }

        // Test 5.5: Clear and Refill
        PrintSubHeader("Test 5.5: Clear Queue and Refill");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            // Fill to capacity
            for (uint32_t i = 0; i < 100; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                queue->Enqueue(p, header, 999);
            }

            PrintInit("Queue full (100). Clearing...");
            queue->Clear();

            PrintActual("sizeAfterClear", std::to_string(queue->GetSize()));
            PrintExpected("sizeAfterClear", "0");

            PrintState("Refilling with 100 new messages...");

            // Refill
            uint32_t refilled = 0;
            for (uint32_t i = 500; i < 600; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                if (queue->Enqueue(p, header, 999))
                {
                    refilled++;
                }
            }

            PrintActual("refillCount", std::to_string(refilled));
            PrintExpected("refillCount", "100");

            bool passed = (refilled == 100);
            PrintCheck("Queue can be cleared and refilled", passed);

            allPassed &= passed;
        }

        RecordResult("Subtask 5: Queue Size Limits and Overflow Handling", allPassed);
        return allPassed;
    }
};

// ============================================================================
// SUBTASK 6: High Load Behavior
// ============================================================================

class Subtask6Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 6: Queue Behavior Under High Message Load");

        bool allPassed = true;

        // Test 6.1: Burst Enqueue
        PrintSubHeader("Test 6.1: Burst Enqueue (100 messages rapidly)");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            PrintInit("Burst enqueueing 100 messages...");

            uint32_t accepted = 0;
            for (uint32_t i = 0; i < 100; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                if (queue->Enqueue(p, header, 999))
                {
                    accepted++;
                }
            }

            uint32_t enq, deq, dups, loops, over;
            queue->GetStatistics(enq, deq, dups, loops, over);

            PrintActual("accepted", std::to_string(accepted));
            PrintExpected("accepted", "100");
            PrintActual("totalEnqueued", std::to_string(enq));
            PrintExpected("totalEnqueued", "100");

            bool passed = (accepted == 100) && (enq == 100);
            PrintCheck("All 100 burst messages tracked correctly", passed);

            allPassed &= passed;
        }

        // Test 6.2: Interleaved Operations
        PrintSubHeader("Test 6.2: Interleaved Enqueue/Dequeue Operations");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            PrintInit("Enqueue 20, Dequeue 10, Enqueue 30, Dequeue 15...");

            // Enqueue 20
            for (uint32_t i = 0; i < 20; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                queue->Enqueue(p, header, 999);
            }
            PrintState("After +20: size = " + std::to_string(queue->GetSize()));

            // Dequeue 10
            for (int i = 0; i < 10; i++)
            {
                BleDiscoveryHeaderWrapper temp;
                queue->Dequeue(temp);
            }
            PrintState("After -10: size = " + std::to_string(queue->GetSize()));

            // Enqueue 30 more
            for (uint32_t i = 100; i < 130; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                queue->Enqueue(p, header, 999);
            }
            PrintState("After +30: size = " + std::to_string(queue->GetSize()));

            // Dequeue 15
            for (int i = 0; i < 15; i++)
            {
                BleDiscoveryHeaderWrapper temp;
                queue->Dequeue(temp);
            }
            PrintState("After -15: size = " + std::to_string(queue->GetSize()));

            uint32_t finalSize = queue->GetSize();
            uint32_t expectedSize = 20 - 10 + 30 - 15; // = 25

            PrintActual("finalSize", std::to_string(finalSize));
            PrintExpected("finalSize", std::to_string(expectedSize) + " (20-10+30-15)");

            uint32_t enq, deq, dups, loops, over;
            queue->GetStatistics(enq, deq, dups, loops, over);

            PrintActual("totalEnqueued", std::to_string(enq));
            PrintExpected("totalEnqueued", "50 (20+30)");
            PrintActual("totalDequeued", std::to_string(deq));
            PrintExpected("totalDequeued", "25 (10+15)");

            bool sizeOk = (finalSize == expectedSize);
            bool statsOk = (enq == 50) && (deq == 25);

            PrintCheck("Final queue size correct", sizeOk);
            PrintCheck("Statistics accurate", statsOk);

            allPassed &= sizeOk && statsOk;
        }

        // Test 6.3: Mixed Rejection Types
        PrintSubHeader("Test 6.3: Mixed Rejection Types Under Load");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            PrintInit("Testing: duplicates, loops, and overflows in one scenario");

            // Add 90 unique messages
            for (uint32_t i = 0; i < 90; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                queue->Enqueue(p, header, 999);
            }
            PrintState("Added 90 unique messages");

            // Try 5 duplicates (sender 0-4)
            for (uint32_t i = 0; i < 5; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);  // Same as existing
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                queue->Enqueue(p, header, 999);
            }
            PrintState("Attempted 5 duplicates");

            // Try 5 messages with loops (node 999 in path)
            for (uint32_t i = 200; i < 205; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);
                header.AddToPath(999);  // Our node ID in path

                Ptr<Packet> p = Create<Packet>();
                queue->Enqueue(p, header, 999);
            }
            PrintState("Attempted 5 messages with loops");

            // Add 10 more unique to fill queue
            for (uint32_t i = 300; i < 310; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                queue->Enqueue(p, header, 999);
            }
            PrintState("Added 10 more unique (queue should be at 100)");

            // Try 5 more to cause overflow
            for (uint32_t i = 400; i < 405; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                queue->Enqueue(p, header, 999);
            }
            PrintState("Attempted 5 overflow messages");

            uint32_t enq, deq, dups, loops, over;
            queue->GetStatistics(enq, deq, dups, loops, over);

            PrintQueueState(queue, "Final state");

            PrintActual("totalDuplicates", std::to_string(dups));
            PrintExpected("totalDuplicates", "5");
            PrintActual("totalLoops", std::to_string(loops));
            PrintExpected("totalLoops", "5");
            PrintActual("totalOverflows", std::to_string(over));
            PrintExpected("totalOverflows", "5");
            PrintActual("queueSize", std::to_string(queue->GetSize()));
            PrintExpected("queueSize", "100");

            bool dupsOk = (dups == 5);
            bool loopsOk = (loops == 5);
            bool overOk = (over == 5);
            bool sizeOk = (queue->GetSize() == 100);

            PrintCheck("5 duplicates recorded", dupsOk);
            PrintCheck("5 loops recorded", loopsOk);
            PrintCheck("5 overflows recorded", overOk);
            PrintCheck("Queue at max capacity", sizeOk);

            allPassed &= dupsOk && loopsOk && overOk && sizeOk;
        }

        // Test 6.4: Full Cycle Stress Test
        PrintSubHeader("Test 6.4: Full Cycle Stress Test");
        {
            Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue>();

            PrintInit("Fill -> Drain -> Refill cycle");

            // Fill
            for (uint32_t i = 0; i < 100; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                queue->Enqueue(p, header, 999);
            }
            PrintState("Filled: size = " + std::to_string(queue->GetSize()));

            // Drain completely
            while (!queue->IsEmpty())
            {
                BleDiscoveryHeaderWrapper temp;
                queue->Dequeue(temp);
            }
            PrintState("Drained: size = " + std::to_string(queue->GetSize()));

            // Refill
            uint32_t refilled = 0;
            for (uint32_t i = 1000; i < 1100; i++)
            {
                BleDiscoveryHeaderWrapper header;
                header.SetSenderId(i);
                header.SetTtl(10);
                header.AddToPath(i);

                Ptr<Packet> p = Create<Packet>();
                if (queue->Enqueue(p, header, 999))
                {
                    refilled++;
                }
            }
            PrintState("Refilled: size = " + std::to_string(queue->GetSize()));

            uint32_t enq, deq, dups, loops, over;
            queue->GetStatistics(enq, deq, dups, loops, over);

            PrintActual("totalEnqueued", std::to_string(enq));
            PrintExpected("totalEnqueued", "200 (100+100)");
            PrintActual("totalDequeued", std::to_string(deq));
            PrintExpected("totalDequeued", "100");
            PrintActual("finalSize", std::to_string(queue->GetSize()));
            PrintExpected("finalSize", "100");

            bool statsOk = (enq == 200) && (deq == 100);
            bool sizeOk = (queue->GetSize() == 100);

            PrintCheck("Statistics track full cycle correctly", statsOk);
            PrintCheck("Queue functional after full cycle", sizeOk);

            allPassed &= statsOk && sizeOk;
        }

        RecordResult("Subtask 6: High Load Behavior", allPassed);
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
                  << "*** ALL TASK 8 SUBTASKS VERIFIED SUCCESSFULLY ***"
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
    std::cout << "#   TASK 8: MESSAGE FORWARDING QUEUE - COMPREHENSIVE TEST      #\n";
    std::cout << "#                                                              #\n";
    std::cout << "################################################################" << COLOR_RESET << "\n";
    std::cout << "\nVerbose mode: " << (g_verbose ? "ON" : "OFF") << "\n";
    std::cout << COLOR_YELLOW << "Expected values shown in yellow" << COLOR_RESET << "\n";

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
