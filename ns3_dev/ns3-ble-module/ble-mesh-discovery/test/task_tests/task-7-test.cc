#include "ns3/core-module.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/ble-discovery-cycle-wrapper.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Task7Test");






struct SubtaskResult {
    std::string name;
    bool passed;
    std::string details;
};

std::vector<SubtaskResult> g_testResults;


bool g_verbose = true;





void PrintHeader (const std::string& title)
{
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << " " << title << "\n";
    std::cout << "============================================================\n";
}

void PrintSubHeader (const std::string& title)
{
    std::cout << "\n--- " << title << " ---\n";
}

void PrintState (const std::string& message)
{
    if (g_verbose)
    {
        std::cout << "[STATE] " << message << "\n";
    }
}

void PrintEvent (const std::string& message)
{
    if (g_verbose)
    {
        std::cout << "[" << std::setw(6) << Simulator::Now().GetMilliSeconds() << "ms] " << message << "\n";
    }
}

void PrintCheck (const std::string& check, bool passed)
{
    std::cout << "  [" << (passed ? "PASS" : "FAIL") << "] " << check << "\n";
}

void RecordResult (const std::string& name, bool passed, const std::string& details = "")
{
    SubtaskResult result;
    result.name = name;
    result.passed = passed;
    result.details = details;
    g_testResults.push_back(result);
}





class Subtask1Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 1: Discovery Cycle Timing Structure (4 Message Slots)");

        bool allPassed = true;

        
        PrintSubHeader("Test 1.1: Default Slot Duration");
        {
            Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper>();
            Time slotDuration = cycle->GetSlotDuration();

            PrintState("Created BleDiscoveryCycleWrapper object");
            PrintState("Default slot duration: " + std::to_string(slotDuration.GetMilliSeconds()) + "ms");

            bool passed = (slotDuration == MilliSeconds(100));
            PrintCheck("Default slot duration is 100ms", passed);
            allPassed &= passed;
        }

        
        PrintSubHeader("Test 1.2: Cycle Duration = 4 x Slot Duration");
        {
            Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper>();

            Time slotDuration = cycle->GetSlotDuration();
            Time cycleDuration = cycle->GetCycleDuration();

            PrintState("Slot duration: " + std::to_string(slotDuration.GetMilliSeconds()) + "ms");
            PrintState("Cycle duration: " + std::to_string(cycleDuration.GetMilliSeconds()) + "ms");
            PrintState("Expected cycle duration: " + std::to_string(slotDuration.GetMilliSeconds() * 4) + "ms");

            bool passed = (cycleDuration == slotDuration * 4);
            PrintCheck("Cycle duration equals 4 x slot duration", passed);
            allPassed &= passed;
        }

        
        PrintSubHeader("Test 1.3: Configurable Slot Duration");
        {
            Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper>();

            PrintState("Setting slot duration to 50ms...");
            cycle->SetSlotDuration(MilliSeconds(50));

            Time newSlotDuration = cycle->GetSlotDuration();
            Time newCycleDuration = cycle->GetCycleDuration();

            PrintState("New slot duration: " + std::to_string(newSlotDuration.GetMilliSeconds()) + "ms");
            PrintState("New cycle duration: " + std::to_string(newCycleDuration.GetMilliSeconds()) + "ms");

            bool slotPassed = (newSlotDuration == MilliSeconds(50));
            bool cyclePassed = (newCycleDuration == MilliSeconds(200));

            PrintCheck("Slot duration changed to 50ms", slotPassed);
            PrintCheck("Cycle duration updated to 200ms (4 x 50ms)", cyclePassed);

            allPassed &= slotPassed && cyclePassed;
        }

        
        PrintSubHeader("Test 1.4: Different Time Units");
        {
            Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper>();

            PrintState("Setting slot duration to 1 second...");
            cycle->SetSlotDuration(Seconds(1));

            Time slotDuration = cycle->GetSlotDuration();
            Time cycleDuration = cycle->GetCycleDuration();

            PrintState("Slot duration: " + std::to_string(slotDuration.GetSeconds()) + "s");
            PrintState("Cycle duration: " + std::to_string(cycleDuration.GetSeconds()) + "s");

            bool passed = (cycleDuration == Seconds(4));
            PrintCheck("Cycle duration is 4 seconds", passed);
            allPassed &= passed;
        }

        RecordResult("Subtask 1: Discovery Cycle Timing Structure", allPassed);
        return allPassed;
    }
};





class Subtask2Test
{
public:
    static uint32_t s_slot0Count;
    static uint32_t s_slot1Count;
    static uint32_t s_slot2Count;
    static uint32_t s_slot3Count;

    static void OnSlot0 () { s_slot0Count++; PrintEvent("Slot 0 callback fired (OWN MESSAGE)"); }
    static void OnSlot1 () { s_slot1Count++; PrintEvent("Slot 1 callback fired (FORWARDING)"); }
    static void OnSlot2 () { s_slot2Count++; PrintEvent("Slot 2 callback fired (FORWARDING)"); }
    static void OnSlot3 () { s_slot3Count++; PrintEvent("Slot 3 callback fired (FORWARDING)"); }

    static bool Run ()
    {
        PrintHeader("SUBTASK 2: Slot Timing Mechanism (1 Own + 3 Forwarding)");

        bool allPassed = true;

        
        PrintSubHeader("Test 2.1: Slot 0 Callback (Own Message)");
        {
            s_slot0Count = 0;

            Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper>();
            cycle->SetSlotDuration(MilliSeconds(25));
            cycle->SetSlot0Callback(MakeCallback(&Subtask2Test::OnSlot0));

            PrintState("Configured slot 0 callback for own message transmission");
            PrintState("Starting cycle with 25ms slots...");

            cycle->Start();
            Simulator::Stop(MilliSeconds(30)); 
            Simulator::Run();
            cycle->Stop();
            Simulator::Destroy();

            PrintState("Slot 0 callback count: " + std::to_string(s_slot0Count));

            bool passed = (s_slot0Count == 1);
            PrintCheck("Slot 0 callback fired exactly once", passed);
            allPassed &= passed;
        }

        
        PrintSubHeader("Test 2.2: Slots 1-3 Callbacks (Forwarding)");
        {
            s_slot0Count = 0;
            s_slot1Count = 0;
            s_slot2Count = 0;
            s_slot3Count = 0;

            Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper>();
            cycle->SetSlotDuration(MilliSeconds(25));

            cycle->SetSlot0Callback(MakeCallback(&Subtask2Test::OnSlot0));
            cycle->SetForwardingSlotCallback(1, MakeCallback(&Subtask2Test::OnSlot1));
            cycle->SetForwardingSlotCallback(2, MakeCallback(&Subtask2Test::OnSlot2));
            cycle->SetForwardingSlotCallback(3, MakeCallback(&Subtask2Test::OnSlot3));

            PrintState("Configured all slot callbacks:");
            PrintState("  - Slot 0: Own message transmission");
            PrintState("  - Slot 1: Forwarding slot #1");
            PrintState("  - Slot 2: Forwarding slot #2");
            PrintState("  - Slot 3: Forwarding slot #3");
            PrintState("Starting cycle...");

            cycle->Start();
            Simulator::Stop(MilliSeconds(99)); 
            Simulator::Run();
            cycle->Stop();
            Simulator::Destroy();

            PrintState("\nCallback counts after one cycle:");
            PrintState("  Slot 0 (own): " + std::to_string(s_slot0Count));
            PrintState("  Slot 1 (fwd): " + std::to_string(s_slot1Count));
            PrintState("  Slot 2 (fwd): " + std::to_string(s_slot2Count));
            PrintState("  Slot 3 (fwd): " + std::to_string(s_slot3Count));

            bool slot0Passed = (s_slot0Count == 1);
            bool slot1Passed = (s_slot1Count == 1);
            bool slot2Passed = (s_slot2Count == 1);
            bool slot3Passed = (s_slot3Count == 1);

            PrintCheck("Slot 0 fired once (own message)", slot0Passed);
            PrintCheck("Slot 1 fired once (forwarding)", slot1Passed);
            PrintCheck("Slot 2 fired once (forwarding)", slot2Passed);
            PrintCheck("Slot 3 fired once (forwarding)", slot3Passed);

            bool ratioCorrect = (s_slot0Count == 1) && (s_slot1Count + s_slot2Count + s_slot3Count == 3);
            PrintCheck("Ratio is 1:3 (1 own message, 3 forwarding)", ratioCorrect);

            allPassed &= slot0Passed && slot1Passed && slot2Passed && slot3Passed && ratioCorrect;
        }

        
        PrintSubHeader("Test 2.3: Current Slot Tracking");
        {
            Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper>();

            uint8_t initialSlot = cycle->GetCurrentSlot();
            PrintState("Initial current slot: " + std::to_string(initialSlot));

            bool passed = (initialSlot == 0);
            PrintCheck("Initial slot is 0", passed);
            allPassed &= passed;
        }

        RecordResult("Subtask 2: Slot Timing Mechanism (1+3)", allPassed);
        return allPassed;
    }
};

uint32_t Subtask2Test::s_slot0Count = 0;
uint32_t Subtask2Test::s_slot1Count = 0;
uint32_t Subtask2Test::s_slot2Count = 0;
uint32_t Subtask2Test::s_slot3Count = 0;





class Subtask3Test
{
public:
    static uint32_t s_slotExecutions;
    static uint32_t s_cycleCompletions;
    static std::vector<Time> s_cycleCompleteTimes;

    static void OnSlot () { s_slotExecutions++; }
    static void OnCycleComplete ()
    {
        s_cycleCompletions++;
        s_cycleCompleteTimes.push_back(Simulator::Now());
        PrintEvent("Cycle " + std::to_string(s_cycleCompletions) + " completed");
    }

    static bool Run ()
    {
        PrintHeader("SUBTASK 3: Discovery Cycle Scheduler");

        bool allPassed = true;

        
        PrintSubHeader("Test 3.1: Multiple Cycle Execution");
        {
            s_slotExecutions = 0;
            s_cycleCompletions = 0;
            s_cycleCompleteTimes.clear();

            Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper>();
            cycle->SetSlotDuration(MilliSeconds(50));

            cycle->SetSlot0Callback(MakeCallback(&Subtask3Test::OnSlot));
            cycle->SetForwardingSlotCallback(1, MakeCallback(&Subtask3Test::OnSlot));
            cycle->SetForwardingSlotCallback(2, MakeCallback(&Subtask3Test::OnSlot));
            cycle->SetForwardingSlotCallback(3, MakeCallback(&Subtask3Test::OnSlot));
            cycle->SetCycleCompleteCallback(MakeCallback(&Subtask3Test::OnCycleComplete));

            PrintState("Configured cycle with 50ms slots (200ms per cycle)");
            PrintState("Running for 3 complete cycles (600ms)...\n");

            cycle->Start();
            
            
            
            
            
            Simulator::Stop(MilliSeconds(601));
            Simulator::Run();
            cycle->Stop();
            Simulator::Destroy();

            PrintState("\nResults:");
            PrintState("  Total slot executions: " + std::to_string(s_slotExecutions));
            PrintState("  Expected: 13 (4 slots x 3 cycles + cycle 4 slot 0 at completion time)");
            PrintState("  Cycle completions: " + std::to_string(s_cycleCompletions));
            PrintState("  Expected: 3");

            
            
            
            bool slotsPassed = (s_slotExecutions == 13);
            bool cyclesPassed = (s_cycleCompletions == 3);

            PrintCheck("13 slot executions (3 complete cycles + cycle 4 slot 0)", slotsPassed);
            PrintCheck("3 cycle completions", cyclesPassed);

            allPassed &= slotsPassed && cyclesPassed;
        }

        
        PrintSubHeader("Test 3.2: Cycle Completion Timing");
        {
            if (s_cycleCompleteTimes.size() >= 3)
            {
                PrintState("Cycle completion times:");
                PrintState("  Cycle 1: " + std::to_string(s_cycleCompleteTimes[0].GetMilliSeconds()) + "ms (expected 200ms)");
                PrintState("  Cycle 2: " + std::to_string(s_cycleCompleteTimes[1].GetMilliSeconds()) + "ms (expected 400ms)");
                PrintState("  Cycle 3: " + std::to_string(s_cycleCompleteTimes[2].GetMilliSeconds()) + "ms (expected 600ms)");

                bool timing1 = (s_cycleCompleteTimes[0] == MilliSeconds(200));
                bool timing2 = (s_cycleCompleteTimes[1] == MilliSeconds(400));
                bool timing3 = (s_cycleCompleteTimes[2] == MilliSeconds(600));

                PrintCheck("Cycle 1 completed at 200ms", timing1);
                PrintCheck("Cycle 2 completed at 400ms", timing2);
                PrintCheck("Cycle 3 completed at 600ms", timing3);

                allPassed &= timing1 && timing2 && timing3;
            }
        }

        
        PrintSubHeader("Test 3.3: Start/Stop Behavior");
        {
            Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper>();

            PrintState("Initial state: IsRunning = " + std::string(cycle->IsRunning() ? "true" : "false"));
            bool initialState = !cycle->IsRunning();
            PrintCheck("Initially not running", initialState);

            cycle->Start();
            PrintState("After Start(): IsRunning = " + std::string(cycle->IsRunning() ? "true" : "false"));
            bool startedState = cycle->IsRunning();
            PrintCheck("Running after Start()", startedState);

            cycle->Stop();
            PrintState("After Stop(): IsRunning = " + std::string(cycle->IsRunning() ? "true" : "false"));
            bool stoppedState = !cycle->IsRunning();
            PrintCheck("Not running after Stop()", stoppedState);

            Simulator::Destroy();

            allPassed &= initialState && startedState && stoppedState;
        }

        
        PrintSubHeader("Test 3.4: Cycle Count Tracking");
        {
            s_cycleCompletions = 0;

            Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper>();
            cycle->SetSlotDuration(MilliSeconds(25));
            cycle->SetCycleCompleteCallback(MakeCallback(&Subtask3Test::OnCycleComplete));

            PrintState("Initial cycle count: " + std::to_string(cycle->GetCycleCount()));

            cycle->Start();
            Simulator::Stop(MilliSeconds(250)); 
            Simulator::Run();

            uint32_t finalCount = cycle->GetCycleCount();
            PrintState("Final cycle count: " + std::to_string(finalCount));

            cycle->Stop();
            Simulator::Destroy();

            bool passed = (finalCount == 2);
            PrintCheck("Cycle count is 2 after 2.5 cycles worth of time", passed);
            allPassed &= passed;
        }

        RecordResult("Subtask 3: Discovery Cycle Scheduler", allPassed);
        return allPassed;
    }
};

uint32_t Subtask3Test::s_slotExecutions = 0;
uint32_t Subtask3Test::s_cycleCompletions = 0;
std::vector<Time> Subtask3Test::s_cycleCompleteTimes;





class Subtask4Test
{
public:
    static uint32_t s_ownMessageCount;
    static uint32_t s_forwardAttemptCount;
    static std::vector<std::pair<uint8_t, Time>> s_slotLog;

    static void OnOwnMessage ()
    {
        s_ownMessageCount++;
        s_slotLog.push_back(std::make_pair(0, Simulator::Now()));
        PrintEvent("SLOT 0: Sending own discovery message");
    }

    static void OnForward (uint8_t slot)
    {
        s_forwardAttemptCount++;
        s_slotLog.push_back(std::make_pair(slot, Simulator::Now()));
        PrintEvent("SLOT " + std::to_string(slot) + ": Attempting to forward queued message");
    }

    static bool Run ()
    {
        PrintHeader("SUBTASK 4: Slot Allocation Algorithm");

        bool allPassed = true;

        
        PrintSubHeader("Test 4.1: Slot 0 - Own Message Transmission");
        {
            s_ownMessageCount = 0;
            s_forwardAttemptCount = 0;
            s_slotLog.clear();

            Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper>();
            cycle->SetSlotDuration(MilliSeconds(50));

            cycle->SetSlot0Callback(MakeCallback(&Subtask4Test::OnOwnMessage));
            cycle->SetForwardingSlotCallback(1, MakeBoundCallback(&Subtask4Test::OnForward, (uint8_t)1));
            cycle->SetForwardingSlotCallback(2, MakeBoundCallback(&Subtask4Test::OnForward, (uint8_t)2));
            cycle->SetForwardingSlotCallback(3, MakeBoundCallback(&Subtask4Test::OnForward, (uint8_t)3));

            PrintState("Simulating node behavior over 2 cycles...\n");

            cycle->Start();
            Simulator::Stop(MilliSeconds(399)); 
            Simulator::Run();
            cycle->Stop();
            Simulator::Destroy();

            PrintState("\nSlot execution log:");
            for (const auto& entry : s_slotLog)
            {
                std::string slotType = (entry.first == 0) ? "OWN" : "FWD";
                PrintState("  " + std::to_string(entry.second.GetMilliSeconds()) + "ms - Slot " +
                          std::to_string(entry.first) + " (" + slotType + ")");
            }

            PrintState("\nSummary:");
            PrintState("  Own messages sent: " + std::to_string(s_ownMessageCount));
            PrintState("  Forward attempts: " + std::to_string(s_forwardAttemptCount));

            bool ownPassed = (s_ownMessageCount == 2);
            bool fwdPassed = (s_forwardAttemptCount == 6);
            bool ratioPassed = (s_forwardAttemptCount == s_ownMessageCount * 3);

            PrintCheck("2 own messages sent (slot 0, 2 cycles)", ownPassed);
            PrintCheck("6 forward attempts (slots 1-3, 2 cycles)", fwdPassed);
            PrintCheck("Ratio maintained: 3 forwards per own message", ratioPassed);

            allPassed &= ownPassed && fwdPassed && ratioPassed;
        }

        
        PrintSubHeader("Test 4.2: Null Callback Handling");
        {
            Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper>();
            cycle->SetSlotDuration(MilliSeconds(10));

            
            
            cycle->SetForwardingSlotCallback(1, MakeCallback(&Subtask4Test::OnOwnMessage));

            PrintState("Running with only slot 1 callback set (others null)...");

            bool crashed = false;
            try
            {
                cycle->Start();
                Simulator::Stop(MilliSeconds(50));
                Simulator::Run();
                cycle->Stop();
            }
            catch (...)
            {
                crashed = true;
            }
            Simulator::Destroy();

            PrintCheck("No crash with null callbacks", !crashed);
            allPassed &= !crashed;
        }

        
        PrintSubHeader("Test 4.3: Slot Execution Order");
        {
            s_slotLog.clear();

            Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper>();
            cycle->SetSlotDuration(MilliSeconds(25));

            cycle->SetSlot0Callback(MakeCallback(&Subtask4Test::OnOwnMessage));
            cycle->SetForwardingSlotCallback(1, MakeBoundCallback(&Subtask4Test::OnForward, (uint8_t)1));
            cycle->SetForwardingSlotCallback(2, MakeBoundCallback(&Subtask4Test::OnForward, (uint8_t)2));
            cycle->SetForwardingSlotCallback(3, MakeBoundCallback(&Subtask4Test::OnForward, (uint8_t)3));

            PrintState("Verifying slot order: 0 -> 1 -> 2 -> 3...\n");

            cycle->Start();
            Simulator::Stop(MilliSeconds(110)); 
            Simulator::Run();
            cycle->Stop();
            Simulator::Destroy();

            bool orderCorrect = true;
            if (s_slotLog.size() >= 4)
            {
                orderCorrect = (s_slotLog[0].first == 0) &&
                               (s_slotLog[1].first == 1) &&
                               (s_slotLog[2].first == 2) &&
                               (s_slotLog[3].first == 3);

                PrintState("Execution order: " +
                          std::to_string(s_slotLog[0].first) + " -> " +
                          std::to_string(s_slotLog[1].first) + " -> " +
                          std::to_string(s_slotLog[2].first) + " -> " +
                          std::to_string(s_slotLog[3].first));
            }

            PrintCheck("Slots execute in order: 0 -> 1 -> 2 -> 3", orderCorrect);
            allPassed &= orderCorrect;
        }

        RecordResult("Subtask 4: Slot Allocation Algorithm", allPassed);
        return allPassed;
    }
};

uint32_t Subtask4Test::s_ownMessageCount = 0;
uint32_t Subtask4Test::s_forwardAttemptCount = 0;
std::vector<std::pair<uint8_t, Time>> Subtask4Test::s_slotLog;





class Subtask5Test
{
public:
    static std::vector<Time> s_node1Slot0Times;
    static std::vector<Time> s_node2Slot0Times;
    static std::vector<Time> s_node3Slot0Times;

    static void RecordNode1Slot0 ()
    {
        s_node1Slot0Times.push_back(Simulator::Now());
        PrintEvent("Node 1 - Slot 0 executed");
    }

    static void RecordNode2Slot0 ()
    {
        s_node2Slot0Times.push_back(Simulator::Now());
        PrintEvent("Node 2 - Slot 0 executed");
    }

    static void RecordNode3Slot0 ()
    {
        s_node3Slot0Times.push_back(Simulator::Now());
        PrintEvent("Node 3 - Slot 0 executed");
    }

    static bool Run ()
    {
        PrintHeader("SUBTASK 5: Synchronization Mechanism for Network-Wide Cycles");

        bool allPassed = true;

        
        PrintSubHeader("Test 5.1: Synchronized Start (3 Nodes)");
        {
            s_node1Slot0Times.clear();
            s_node2Slot0Times.clear();
            s_node3Slot0Times.clear();

            
            Ptr<BleDiscoveryCycleWrapper> node1 = CreateObject<BleDiscoveryCycleWrapper>();
            Ptr<BleDiscoveryCycleWrapper> node2 = CreateObject<BleDiscoveryCycleWrapper>();
            Ptr<BleDiscoveryCycleWrapper> node3 = CreateObject<BleDiscoveryCycleWrapper>();

            Time slotDuration = MilliSeconds(50);
            node1->SetSlotDuration(slotDuration);
            node2->SetSlotDuration(slotDuration);
            node3->SetSlotDuration(slotDuration);

            node1->SetSlot0Callback(MakeCallback(&Subtask5Test::RecordNode1Slot0));
            node2->SetSlot0Callback(MakeCallback(&Subtask5Test::RecordNode2Slot0));
            node3->SetSlot0Callback(MakeCallback(&Subtask5Test::RecordNode3Slot0));

            PrintState("Created 3 nodes with 50ms slot duration (200ms cycle)");
            PrintState("Starting all nodes simultaneously at t=0...\n");

            
            node1->Start();
            node2->Start();
            node3->Start();

            
            Simulator::Stop(MilliSeconds(650));
            Simulator::Run();

            node1->Stop();
            node2->Stop();
            node3->Stop();
            Simulator::Destroy();

            PrintState("\nSlot 0 execution times:");
            PrintState("  Node 1: ");
            for (const auto& t : s_node1Slot0Times)
            {
                std::cout << t.GetMilliSeconds() << "ms ";
            }
            std::cout << "\n";

            PrintState("  Node 2: ");
            for (const auto& t : s_node2Slot0Times)
            {
                std::cout << t.GetMilliSeconds() << "ms ";
            }
            std::cout << "\n";

            PrintState("  Node 3: ");
            for (const auto& t : s_node3Slot0Times)
            {
                std::cout << t.GetMilliSeconds() << "ms ";
            }
            std::cout << "\n";

            
            bool syncPassed = true;
            size_t minSize = std::min({s_node1Slot0Times.size(), s_node2Slot0Times.size(), s_node3Slot0Times.size()});

            for (size_t i = 0; i < minSize; i++)
            {
                bool match = (s_node1Slot0Times[i] == s_node2Slot0Times[i]) &&
                             (s_node2Slot0Times[i] == s_node3Slot0Times[i]);
                if (!match)
                {
                    syncPassed = false;
                    PrintState("  MISMATCH at cycle " + std::to_string(i));
                }
            }

            PrintCheck("All nodes execute slot 0 at identical times", syncPassed);
            allPassed &= syncPassed;
        }

        
        PrintSubHeader("Test 5.2: Independent State Tracking");
        {
            Ptr<BleDiscoveryCycleWrapper> nodeA = CreateObject<BleDiscoveryCycleWrapper>();
            Ptr<BleDiscoveryCycleWrapper> nodeB = CreateObject<BleDiscoveryCycleWrapper>();

            nodeA->SetSlotDuration(MilliSeconds(100));
            nodeB->SetSlotDuration(MilliSeconds(50)); 

            PrintState("Node A: 100ms slots, Node B: 50ms slots");

            Time cycleA = nodeA->GetCycleDuration();
            Time cycleB = nodeB->GetCycleDuration();

            PrintState("Node A cycle duration: " + std::to_string(cycleA.GetMilliSeconds()) + "ms");
            PrintState("Node B cycle duration: " + std::to_string(cycleB.GetMilliSeconds()) + "ms");

            bool independent = (cycleA != cycleB);
            PrintCheck("Nodes maintain independent configuration", independent);
            allPassed &= independent;
        }

        
        PrintSubHeader("Test 5.3: Timing Consistency Across Cycles");
        {
            if (s_node1Slot0Times.size() >= 3)
            {
                Time expectedCycle = MilliSeconds(200);

                bool timing1 = (s_node1Slot0Times[0] == MilliSeconds(0));
                bool timing2 = (s_node1Slot0Times[1] == MilliSeconds(200));
                bool timing3 = (s_node1Slot0Times[2] == MilliSeconds(400));

                PrintState("Node 1 slot 0 times: 0ms, 200ms, 400ms expected");
                PrintState("  Actual: " +
                          std::to_string(s_node1Slot0Times[0].GetMilliSeconds()) + "ms, " +
                          std::to_string(s_node1Slot0Times[1].GetMilliSeconds()) + "ms, " +
                          std::to_string(s_node1Slot0Times[2].GetMilliSeconds()) + "ms");

                PrintCheck("Cycle 1 slot 0 at 0ms", timing1);
                PrintCheck("Cycle 2 slot 0 at 200ms", timing2);
                PrintCheck("Cycle 3 slot 0 at 400ms", timing3);

                allPassed &= timing1 && timing2 && timing3;
            }
        }

        RecordResult("Subtask 5: Synchronization Mechanism", allPassed);
        return allPassed;
    }
};

std::vector<Time> Subtask5Test::s_node1Slot0Times;
std::vector<Time> Subtask5Test::s_node2Slot0Times;
std::vector<Time> Subtask5Test::s_node3Slot0Times;





class Subtask6Test
{
public:
    static std::vector<std::pair<uint8_t, Time>> s_allSlotTimes;

    static void RecordSlot (uint8_t slot)
    {
        s_allSlotTimes.push_back(std::make_pair(slot, Simulator::Now()));
    }

    static bool Run ()
    {
        PrintHeader("SUBTASK 6: Cycle Timing Accuracy and Consistency");

        bool allPassed = true;

        
        PrintSubHeader("Test 6.1: Timing Accuracy Over 5 Cycles");
        {
            s_allSlotTimes.clear();

            Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper>();
            Time slotDuration = MilliSeconds(25);
            cycle->SetSlotDuration(slotDuration);

            cycle->SetSlot0Callback(MakeBoundCallback(&Subtask6Test::RecordSlot, (uint8_t)0));
            cycle->SetForwardingSlotCallback(1, MakeBoundCallback(&Subtask6Test::RecordSlot, (uint8_t)1));
            cycle->SetForwardingSlotCallback(2, MakeBoundCallback(&Subtask6Test::RecordSlot, (uint8_t)2));
            cycle->SetForwardingSlotCallback(3, MakeBoundCallback(&Subtask6Test::RecordSlot, (uint8_t)3));

            PrintState("Running 5 cycles with 25ms slots (100ms cycle)");
            PrintState("Expected 20 slot executions...\n");

            cycle->Start();
            Simulator::Stop(MilliSeconds(520)); 
            Simulator::Run();
            cycle->Stop();
            Simulator::Destroy();

            PrintState("Total slot executions: " + std::to_string(s_allSlotTimes.size()));

            
            bool timingCorrect = true;
            Time cycleDuration = slotDuration * 4;

            PrintState("\nVerifying timing accuracy:");
            for (size_t i = 0; i < s_allSlotTimes.size() && i < 20; i++)
            {
                uint8_t slotNum = i % 4;
                uint32_t cycleNum = i / 4;
                Time expectedTime = cycleDuration * cycleNum + slotDuration * slotNum;
                Time actualTime = s_allSlotTimes[i].second;

                bool match = (actualTime == expectedTime);
                if (!match)
                {
                    timingCorrect = false;
                    PrintState("  DRIFT at slot " + std::to_string(i) +
                              ": expected " + std::to_string(expectedTime.GetMilliSeconds()) +
                              "ms, got " + std::to_string(actualTime.GetMilliSeconds()) + "ms");
                }
            }

            if (timingCorrect)
            {
                PrintState("  All 20 slots executed at exact expected times!");
            }

            PrintCheck("No timing drift over 5 cycles", timingCorrect);
            allPassed &= timingCorrect;
        }

        
        PrintSubHeader("Test 6.2: Consistent Slot Intervals");
        {
            bool intervalsCorrect = true;
            Time expectedInterval = MilliSeconds(25);

            PrintState("Checking intervals between consecutive slots (should all be 25ms):");

            for (size_t i = 1; i < s_allSlotTimes.size() && i < 20; i++)
            {
                Time interval = s_allSlotTimes[i].second - s_allSlotTimes[i-1].second;

                if (interval != expectedInterval)
                {
                    intervalsCorrect = false;
                    PrintState("  Interval " + std::to_string(i-1) + "->" + std::to_string(i) +
                              ": " + std::to_string(interval.GetMilliSeconds()) + "ms (expected 25ms)");
                }
            }

            if (intervalsCorrect)
            {
                PrintState("  All intervals are exactly 25ms!");
            }

            PrintCheck("All slot intervals are consistent", intervalsCorrect);
            allPassed &= intervalsCorrect;
        }

        
        PrintSubHeader("Test 6.3: Extended Accuracy Test (10 Cycles)");
        {
            s_allSlotTimes.clear();

            Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper>();
            Time slotDuration = MilliSeconds(10);
            cycle->SetSlotDuration(slotDuration);

            cycle->SetSlot0Callback(MakeBoundCallback(&Subtask6Test::RecordSlot, (uint8_t)0));
            cycle->SetForwardingSlotCallback(1, MakeBoundCallback(&Subtask6Test::RecordSlot, (uint8_t)1));
            cycle->SetForwardingSlotCallback(2, MakeBoundCallback(&Subtask6Test::RecordSlot, (uint8_t)2));
            cycle->SetForwardingSlotCallback(3, MakeBoundCallback(&Subtask6Test::RecordSlot, (uint8_t)3));

            PrintState("Running 10 cycles with 10ms slots (40ms cycle)");
            PrintState("Expected: 40 slot executions, final slot at 390ms");

            cycle->Start();
            Simulator::Stop(MilliSeconds(420));
            Simulator::Run();
            cycle->Stop();
            Simulator::Destroy();

            
            bool lastSlotCorrect = false;
            if (s_allSlotTimes.size() >= 40)
            {
                Time lastExpected = MilliSeconds(390); 
                Time lastActual = s_allSlotTimes[39].second;
                lastSlotCorrect = (lastActual == lastExpected);

                PrintState("Last slot (40th) timing:");
                PrintState("  Expected: 390ms");
                PrintState("  Actual: " + std::to_string(lastActual.GetMilliSeconds()) + "ms");
            }

            PrintCheck("40 slot executions recorded", s_allSlotTimes.size() >= 40);
            PrintCheck("Last slot at correct time (no accumulated drift)", lastSlotCorrect);

            allPassed &= (s_allSlotTimes.size() >= 40) && lastSlotCorrect;
        }

        RecordResult("Subtask 6: Cycle Timing Accuracy", allPassed);
        return allPassed;
    }
};

std::vector<std::pair<uint8_t, Time>> Subtask6Test::s_allSlotTimes;





void PrintFinalResults ()
{
    PrintHeader("FINAL TEST RESULTS");

    uint32_t passed = 0;
    uint32_t failed = 0;

    std::cout << "\n";
    for (const auto& result : g_testResults)
    {
        std::string status = result.passed ? "PASSED" : "FAILED";
        std::string marker = result.passed ? "[OK]" : "[XX]";

        std::cout << "  " << marker << " " << result.name << "\n";

        if (result.passed)
            passed++;
        else
            failed++;
    }

    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << " SUMMARY: " << passed << " passed, " << failed << " failed\n";
    std::cout << "============================================================\n";

    if (failed == 0)
    {
        std::cout << "\n  *** ALL TASK 7 SUBTASKS VERIFIED SUCCESSFULLY ***\n\n";
    }
    else
    {
        std::cout << "\n  !!! SOME SUBTASKS FAILED - REVIEW OUTPUT ABOVE !!!\n\n";
    }
}

int main (int argc, char *argv[])
{
    
    CommandLine cmd;
    cmd.AddValue("verbose", "Enable verbose output", g_verbose);
    cmd.Parse(argc, argv);

    std::cout << "\n";
    std::cout << "################################################################\n";
    std::cout << "#                                                              #\n";
    std::cout << "#  TASK 7: 4-SLOT DISCOVERY CYCLE - COMPREHENSIVE TEST SUITE   #\n";
    std::cout << "#                                                              #\n";
    std::cout << "################################################################\n";
    std::cout << "\nVerbose mode: " << (g_verbose ? "ON" : "OFF") << "\n";

    
    Subtask1Test::Run();
    Subtask2Test::Run();
    Subtask3Test::Run();
    Subtask4Test::Run();
    Subtask5Test::Run();
    Subtask6Test::Run();

    
    PrintFinalResults();

    return 0;
}
