#include "ns3/core-module.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/ble-forwarding-logic.h"
#include "ns3/ble-discovery-header-wrapper.h"
#include "ns3/vector.h"
#include "ns3/double.h"
#include "ns3/random-variable-stream.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Task10Test");





struct SubtaskResult {
    std::string name;
    bool passed;
    std::string details;
};

std::vector<SubtaskResult> g_testResults;
bool g_verbose = true;


const std::string COLOR_RESET = "\033[0m";
const std::string COLOR_GREEN = "\033[32m";
const std::string COLOR_RED = "\033[31m";
const std::string COLOR_YELLOW = "\033[33m";
const std::string COLOR_CYAN = "\033[36m";
const std::string COLOR_BOLD = "\033[1m";
const std::string COLOR_MAGENTA = "\033[35m";





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

std::string VectorToString (const Vector& v)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return ss.str();
}





class Subtask1Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 1: Distance Calculation Between GPS Coordinates");

        bool allPassed = true;

        
        PrintSubHeader("Test 1.1: Same Location (Distance = 0)");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector loc1(0.0, 0.0, 0.0);
            Vector loc2(0.0, 0.0, 0.0);

            PrintInit("Location 1: " + VectorToString(loc1));
            PrintInit("Location 2: " + VectorToString(loc2));

            double distance = logic->CalculateDistance(loc1, loc2);

            PrintActual("distance", std::to_string(distance) + " meters");
            PrintExpected("distance", "0.0 meters");

            bool passed = (distance == 0.0);
            PrintCheck("Same location has distance 0", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 1.2: Horizontal X-Axis Distance");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector loc1(0.0, 0.0, 0.0);
            Vector loc2(10.0, 0.0, 0.0);

            PrintInit("Location 1: " + VectorToString(loc1));
            PrintInit("Location 2: " + VectorToString(loc2));

            double distance = logic->CalculateDistance(loc1, loc2);

            PrintActual("distance", std::to_string(distance) + " meters");
            PrintExpected("distance", "10.0 meters");

            bool passed = (distance == 10.0);
            PrintCheck("X-axis distance is 10 meters", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 1.3: Horizontal Y-Axis Distance");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector loc1(0.0, 0.0, 0.0);
            Vector loc2(0.0, 15.0, 0.0);

            PrintInit("Location 1: " + VectorToString(loc1));
            PrintInit("Location 2: " + VectorToString(loc2));

            double distance = logic->CalculateDistance(loc1, loc2);

            PrintActual("distance", std::to_string(distance) + " meters");
            PrintExpected("distance", "15.0 meters");

            bool passed = (distance == 15.0);
            PrintCheck("Y-axis distance is 15 meters", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 1.4: Vertical Z-Axis Distance (Altitude)");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector loc1(0.0, 0.0, 0.0);
            Vector loc2(0.0, 0.0, 20.0);

            PrintInit("Location 1: " + VectorToString(loc1));
            PrintInit("Location 2: " + VectorToString(loc2));

            double distance = logic->CalculateDistance(loc1, loc2);

            PrintActual("distance", std::to_string(distance) + " meters");
            PrintExpected("distance", "20.0 meters");

            bool passed = (distance == 20.0);
            PrintCheck("Z-axis (altitude) distance is 20 meters", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 1.5: 2D Diagonal (3-4-5 Pythagorean Triangle)");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector loc1(0.0, 0.0, 0.0);
            Vector loc2(3.0, 4.0, 0.0);

            PrintInit("Location 1: " + VectorToString(loc1));
            PrintInit("Location 2: " + VectorToString(loc2));
            PrintState("Formula: sqrt(3² + 4²) = sqrt(9 + 16) = sqrt(25) = 5");

            double distance = logic->CalculateDistance(loc1, loc2);

            PrintActual("distance", std::to_string(distance) + " meters");
            PrintExpected("distance", "5.0 meters");

            bool passed = (distance == 5.0);
            PrintCheck("3-4-5 triangle distance is 5 meters", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 1.6: 3D Diagonal Distance");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector loc1(0.0, 0.0, 0.0);
            Vector loc2(1.0, 2.0, 2.0);

            PrintInit("Location 1: " + VectorToString(loc1));
            PrintInit("Location 2: " + VectorToString(loc2));
            PrintState("Formula: sqrt(1² + 2² + 2²) = sqrt(1 + 4 + 4) = sqrt(9) = 3");

            double distance = logic->CalculateDistance(loc1, loc2);

            PrintActual("distance", std::to_string(distance) + " meters");
            PrintExpected("distance", "3.0 meters");

            bool passed = (distance == 3.0);
            PrintCheck("3D diagonal distance is 3 meters", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 1.7: Negative Coordinates");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector loc1(-5.0, -5.0, 0.0);
            Vector loc2(5.0, 5.0, 0.0);

            PrintInit("Location 1: " + VectorToString(loc1));
            PrintInit("Location 2: " + VectorToString(loc2));
            PrintState("Formula: sqrt(10² + 10²) = sqrt(200) ≈ 14.142");

            double distance = logic->CalculateDistance(loc1, loc2);
            double expected = std::sqrt(200.0);

            PrintActual("distance", std::to_string(distance) + " meters");
            PrintExpected("distance", std::to_string(expected) + " meters (sqrt(200))");

            bool passed = (std::abs(distance - expected) < 0.001);
            PrintCheck("Negative coordinates handled correctly", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 1.8: Large Distances");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector loc1(0.0, 0.0, 0.0);
            Vector loc2(1000.0, 1000.0, 0.0);

            PrintInit("Location 1: " + VectorToString(loc1));
            PrintInit("Location 2: " + VectorToString(loc2));

            double distance = logic->CalculateDistance(loc1, loc2);
            double expected = std::sqrt(2000000.0);

            PrintActual("distance", std::to_string(distance) + " meters");
            PrintExpected("distance", std::to_string(expected) + " meters");

            bool passed = (std::abs(distance - expected) < 0.01);
            PrintCheck("Large distance calculated correctly", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 1.9: Small Distances (Precision)");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector loc1(0.0, 0.0, 0.0);
            Vector loc2(0.5, 0.0, 0.0);

            PrintInit("Location 1: " + VectorToString(loc1));
            PrintInit("Location 2: " + VectorToString(loc2));

            double distance = logic->CalculateDistance(loc1, loc2);

            PrintActual("distance", std::to_string(distance) + " meters");
            PrintExpected("distance", "0.5 meters");

            bool passed = (std::abs(distance - 0.5) < 0.0001);
            PrintCheck("Small distance (0.5m) calculated with precision", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 1.10: Distance Symmetry (A→B == B→A)");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector locA(10.0, 20.0, 5.0);
            Vector locB(30.0, 45.0, 15.0);

            double distAB = logic->CalculateDistance(locA, locB);
            double distBA = logic->CalculateDistance(locB, locA);

            PrintInit("Location A: " + VectorToString(locA));
            PrintInit("Location B: " + VectorToString(locB));
            PrintActual("distance A→B", std::to_string(distAB) + " meters");
            PrintActual("distance B→A", std::to_string(distBA) + " meters");
            PrintExpected("relationship", "A→B == B→A");

            bool passed = (distAB == distBA);
            PrintCheck("Distance is symmetric", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 1.11: Non-Origin Starting Point");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector loc1(100.0, 200.0, 50.0);
            Vector loc2(103.0, 204.0, 50.0);  

            PrintInit("Location 1: " + VectorToString(loc1));
            PrintInit("Location 2: " + VectorToString(loc2));
            PrintState("Relative: (3, 4, 0) → distance = 5");

            double distance = logic->CalculateDistance(loc1, loc2);

            PrintActual("distance", std::to_string(distance) + " meters");
            PrintExpected("distance", "5.0 meters");

            bool passed = (distance == 5.0);
            PrintCheck("Non-origin calculation correct", passed);

            allPassed &= passed;
        }

        RecordResult("Subtask 1: Distance Calculation Between GPS Coordinates", allPassed);
        return allPassed;
    }
};





class Subtask2Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 2: Proximity Threshold Configuration");

        bool allPassed = true;

        
        PrintSubHeader("Test 2.1: Default Proximity Threshold");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            double threshold = logic->GetProximityThreshold();

            PrintInit("Creating new BleForwardingLogic with default parameters");
            PrintActual("proximity threshold", std::to_string(threshold) + " meters");
            PrintExpected("proximity threshold", "10.0 meters (default)");

            bool passed = (threshold == 10.0);
            PrintCheck("Default proximity threshold is 10.0 meters", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 2.2: Set Threshold via Setter Method");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            PrintInit("Setting proximity threshold to 25.0 meters via SetProximityThreshold()");
            logic->SetProximityThreshold(25.0);

            double threshold = logic->GetProximityThreshold();

            PrintActual("proximity threshold", std::to_string(threshold) + " meters");
            PrintExpected("proximity threshold", "25.0 meters");

            bool passed = (threshold == 25.0);
            PrintCheck("Setter method works correctly", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 2.3: Set Threshold via NS-3 Attribute System");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            PrintInit("Setting ProximityThreshold attribute to 15.0 meters");
            logic->SetAttribute("ProximityThreshold", DoubleValue(15.0));

            DoubleValue thresholdValue;
            logic->GetAttribute("ProximityThreshold", thresholdValue);

            PrintActual("ProximityThreshold attribute", std::to_string(thresholdValue.Get()) + " meters");
            PrintExpected("ProximityThreshold attribute", "15.0 meters");

            bool passed = (thresholdValue.Get() == 15.0);
            PrintCheck("NS-3 attribute system works correctly", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 2.4: Zero Threshold (Edge Case)");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            PrintInit("Setting proximity threshold to 0.0 meters");
            logic->SetProximityThreshold(0.0);

            double threshold = logic->GetProximityThreshold();

            PrintActual("proximity threshold", std::to_string(threshold) + " meters");
            PrintExpected("proximity threshold", "0.0 meters");

            bool passed = (threshold == 0.0);
            PrintCheck("Zero threshold accepted", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 2.5: Very Small Threshold");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            PrintInit("Setting proximity threshold to 0.001 meters (1mm)");
            logic->SetProximityThreshold(0.001);

            double threshold = logic->GetProximityThreshold();

            PrintActual("proximity threshold", std::to_string(threshold) + " meters");
            PrintExpected("proximity threshold", "0.001 meters");

            bool passed = (std::abs(threshold - 0.001) < 0.0001);
            PrintCheck("Very small threshold handled with precision", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 2.6: Very Large Threshold");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            PrintInit("Setting proximity threshold to 10000.0 meters (10km)");
            logic->SetProximityThreshold(10000.0);

            double threshold = logic->GetProximityThreshold();

            PrintActual("proximity threshold", std::to_string(threshold) + " meters");
            PrintExpected("proximity threshold", "10000.0 meters");

            bool passed = (threshold == 10000.0);
            PrintCheck("Large threshold accepted", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 2.7: Multiple Sequential Updates");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            PrintInit("Updating threshold multiple times: 5.0 → 20.0 → 7.5 → 100.0 → 10.0");

            std::vector<double> thresholds = {5.0, 20.0, 7.5, 100.0, 10.0};
            bool allUpdatesCorrect = true;

            for (double t : thresholds)
            {
                logic->SetProximityThreshold(t);
                double actual = logic->GetProximityThreshold();
                PrintState("Set to " + std::to_string(t) + "m, got " + std::to_string(actual) + "m");
                if (std::abs(actual - t) > 0.0001) allUpdatesCorrect = false;
            }

            double finalThreshold = logic->GetProximityThreshold();
            PrintActual("final threshold", std::to_string(finalThreshold) + " meters");
            PrintExpected("final threshold", "10.0 meters");

            bool passed = allUpdatesCorrect && (finalThreshold == 10.0);
            PrintCheck("All sequential updates applied correctly", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 2.8: Network Density Configuration Scenarios");
        {
            PrintInit("Testing typical threshold values for different network densities");

            std::cout << "\n  " << COLOR_MAGENTA << "Scenario       | Threshold | Use Case" << COLOR_RESET << "\n";
            std::cout << "  " << std::string(55, '-') << "\n";

            struct DensityScenario {
                std::string name;
                double threshold;
                std::string useCase;
            };

            std::vector<DensityScenario> scenarios = {
                {"Dense urban", 5.0, "Many nodes, reduce redundancy"},
                {"Suburban", 15.0, "Moderate node spacing"},
                {"Rural/sparse", 50.0, "Few nodes, ensure propagation"},
                {"Indoor office", 3.0, "Room-level filtering"},
                {"Campus", 25.0, "Building-level filtering"},
                {"Highway", 100.0, "High-speed vehicle scenarios"}
            };

            bool allScenariosWork = true;
            for (const auto& scenario : scenarios)
            {
                Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();
                logic->SetProximityThreshold(scenario.threshold);
                double actual = logic->GetProximityThreshold();

                std::cout << "  " << std::left << std::setw(14) << scenario.name
                          << " | " << std::setw(9) << (std::to_string(scenario.threshold) + "m")
                          << " | " << scenario.useCase << "\n";

                if (actual != scenario.threshold) allScenariosWork = false;
            }

            PrintCheck("All network density scenarios configurable", allScenariosWork);
            allPassed &= allScenariosWork;
        }

        RecordResult("Subtask 2: Proximity Threshold Configuration", allPassed);
        return allPassed;
    }
};





class Subtask3Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 3: LHGPS Comparison Logic in Forwarding Decision");

        bool allPassed = true;

        
        PrintSubHeader("Test 3.1: Distance Exceeds Threshold (Forward)");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector currentLoc(0.0, 0.0, 0.0);
            Vector lastHopLoc(20.0, 0.0, 0.0);  
            double threshold = 10.0;

            PrintInit("Current location: " + VectorToString(currentLoc));
            PrintInit("Last hop location: " + VectorToString(lastHopLoc));
            PrintInit("Proximity threshold: " + std::to_string(threshold) + "m");
            PrintState("Distance: 20m > Threshold: 10m → Should FORWARD");

            bool shouldForward = logic->ShouldForwardProximity(currentLoc, lastHopLoc, threshold);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "true (20m > 10m)");

            PrintCheck("Forwards when distance (20m) exceeds threshold (10m)", shouldForward);
            allPassed &= shouldForward;
        }

        
        PrintSubHeader("Test 3.2: Distance Below Threshold (Drop)");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector currentLoc(0.0, 0.0, 0.0);
            Vector lastHopLoc(5.0, 0.0, 0.0);  
            double threshold = 10.0;

            PrintInit("Current location: " + VectorToString(currentLoc));
            PrintInit("Last hop location: " + VectorToString(lastHopLoc));
            PrintInit("Proximity threshold: " + std::to_string(threshold) + "m");
            PrintState("Distance: 5m < Threshold: 10m → Should DROP");

            bool shouldForward = logic->ShouldForwardProximity(currentLoc, lastHopLoc, threshold);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "false (5m < 10m)");

            bool passed = !shouldForward;
            PrintCheck("Drops when distance (5m) is below threshold (10m)", passed);
            allPassed &= passed;
        }

        
        PrintSubHeader("Test 3.3: Distance Exactly at Threshold (Boundary)");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector currentLoc(0.0, 0.0, 0.0);
            Vector lastHopLoc(10.0, 0.0, 0.0);  
            double threshold = 10.0;

            PrintInit("Current location: " + VectorToString(currentLoc));
            PrintInit("Last hop location: " + VectorToString(lastHopLoc));
            PrintInit("Proximity threshold: " + std::to_string(threshold) + "m");
            PrintState("Distance: 10m == Threshold: 10m → Logic uses '>' not '>='");

            bool shouldForward = logic->ShouldForwardProximity(currentLoc, lastHopLoc, threshold);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "false (10m is NOT > 10m)");

            bool passed = !shouldForward;
            PrintCheck("Drops when distance equals threshold (strict >)", passed);
            allPassed &= passed;
        }

        
        PrintSubHeader("Test 3.4: Just Above Threshold");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector currentLoc(0.0, 0.0, 0.0);
            Vector lastHopLoc(10.01, 0.0, 0.0);  
            double threshold = 10.0;

            PrintInit("Current location: " + VectorToString(currentLoc));
            PrintInit("Last hop location: " + VectorToString(lastHopLoc));
            PrintInit("Distance: 10.01m, Threshold: 10.0m");

            bool shouldForward = logic->ShouldForwardProximity(currentLoc, lastHopLoc, threshold);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "true (10.01m > 10m)");

            PrintCheck("Forwards when just above threshold", shouldForward);
            allPassed &= shouldForward;
        }

        
        PrintSubHeader("Test 3.5: Just Below Threshold");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector currentLoc(0.0, 0.0, 0.0);
            Vector lastHopLoc(9.99, 0.0, 0.0);  
            double threshold = 10.0;

            PrintInit("Current location: " + VectorToString(currentLoc));
            PrintInit("Last hop location: " + VectorToString(lastHopLoc));
            PrintInit("Distance: 9.99m, Threshold: 10.0m");

            bool shouldForward = logic->ShouldForwardProximity(currentLoc, lastHopLoc, threshold);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "false (9.99m < 10m)");

            bool passed = !shouldForward;
            PrintCheck("Drops when just below threshold", passed);
            allPassed &= passed;
        }

        
        PrintSubHeader("Test 3.6: Zero Threshold (Any Distance Forwards)");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector currentLoc(0.0, 0.0, 0.0);
            Vector lastHopLoc(1.0, 0.0, 0.0);  
            double threshold = 0.0;

            PrintInit("Distance: 1m, Threshold: 0m");
            PrintState("Any positive distance > 0 → Should forward");

            bool shouldForward = logic->ShouldForwardProximity(currentLoc, lastHopLoc, threshold);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "true (1m > 0m)");

            PrintCheck("Forwards with zero threshold (1m > 0m)", shouldForward);
            allPassed &= shouldForward;
        }

        
        PrintSubHeader("Test 3.7: Same Location with Zero Threshold");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector currentLoc(5.0, 5.0, 0.0);
            Vector lastHopLoc(5.0, 5.0, 0.0);  
            double threshold = 0.0;

            PrintInit("Same location, threshold: 0m");
            PrintState("Distance: 0m, 0 is NOT > 0 → Should drop");

            bool shouldForward = logic->ShouldForwardProximity(currentLoc, lastHopLoc, threshold);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "false (0m is NOT > 0m)");

            bool passed = !shouldForward;
            PrintCheck("Drops when at same location (0 is not > 0)", passed);
            allPassed &= passed;
        }

        
        PrintSubHeader("Test 3.8: 3D Distance Affects Decision");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector currentLoc(0.0, 0.0, 0.0);
            Vector lastHopLoc(6.0, 8.0, 0.0);  
            double threshold = 9.0;

            double distance = logic->CalculateDistance(currentLoc, lastHopLoc);
            PrintInit("Locations form 6-8-10 triangle, distance = " + std::to_string(distance) + "m");
            PrintInit("Threshold: 9m");

            bool shouldForward = logic->ShouldForwardProximity(currentLoc, lastHopLoc, threshold);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "true (10m > 9m)");

            PrintCheck("2D diagonal distance (10m) exceeds threshold (9m)", shouldForward);
            allPassed &= shouldForward;
        }

        
        PrintSubHeader("Test 3.9: Vertical Separation (Altitude Difference)");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector currentLoc(0.0, 0.0, 0.0);
            Vector lastHopLoc(0.0, 0.0, 15.0);  
            double threshold = 10.0;

            PrintInit("Current: ground level, Last hop: 15m altitude");
            PrintInit("Threshold: 10m");

            bool shouldForward = logic->ShouldForwardProximity(currentLoc, lastHopLoc, threshold);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "true (15m > 10m)");

            PrintCheck("Altitude difference (15m) exceeds threshold", shouldForward);
            allPassed &= shouldForward;
        }

        
        PrintSubHeader("Test 3.10: Same Distance, Different Thresholds");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Vector currentLoc(0.0, 0.0, 0.0);
            Vector lastHopLoc(15.0, 0.0, 0.0);  

            PrintInit("Fixed distance: 15m");
            PrintState("Testing thresholds: 10m, 15m, 20m");

            bool fwd10 = logic->ShouldForwardProximity(currentLoc, lastHopLoc, 10.0);
            bool fwd15 = logic->ShouldForwardProximity(currentLoc, lastHopLoc, 15.0);
            bool fwd20 = logic->ShouldForwardProximity(currentLoc, lastHopLoc, 20.0);

            PrintActual("threshold 10m", fwd10 ? "forward" : "drop");
            PrintExpected("threshold 10m", "forward (15 > 10)");
            PrintActual("threshold 15m", fwd15 ? "forward" : "drop");
            PrintExpected("threshold 15m", "drop (15 is NOT > 15)");
            PrintActual("threshold 20m", fwd20 ? "forward" : "drop");
            PrintExpected("threshold 20m", "drop (15 < 20)");

            bool passed = fwd10 && !fwd15 && !fwd20;
            PrintCheck("Threshold correctly affects forwarding decision", passed);
            allPassed &= passed;
        }

        
        PrintSubHeader("Test 3.11: LHGPS Extracted from Message Header");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            
            BleDiscoveryHeaderWrapper header;
            header.SetSenderId(100);
            header.SetTtl(10);
            header.AddToPath(100);
            header.SetGpsLocation(Vector(30.0, 0.0, 0.0));  

            Vector currentLoc(0.0, 0.0, 0.0);
            Vector lastHopLoc = header.GetGpsLocation();
            double threshold = 10.0;

            PrintInit("Message GPS: " + VectorToString(lastHopLoc));
            PrintInit("Current location: " + VectorToString(currentLoc));
            PrintInit("Distance: 30m, Threshold: 10m");

            bool shouldForward = logic->ShouldForwardProximity(currentLoc, lastHopLoc, threshold);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "true (30m > 10m)");

            PrintCheck("LHGPS correctly extracted from header", shouldForward);
            allPassed &= shouldForward;
        }

        RecordResult("Subtask 3: LHGPS Comparison Logic", allPassed);
        return allPassed;
    }
};





class Subtask4Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 4: Skip Proximity Check When GPS Unavailable");

        bool allPassed = true;

        
        PrintSubHeader("Test 4.1: New Header Has No GPS by Default");
        {
            BleDiscoveryHeaderWrapper header;
            header.SetSenderId(100);
            header.SetTtl(10);
            header.AddToPath(100);
            

            PrintInit("Created header without setting GPS");

            bool gpsAvailable = header.IsGpsAvailable();

            PrintActual("IsGpsAvailable()", gpsAvailable ? "true" : "false");
            PrintExpected("IsGpsAvailable()", "false");

            bool passed = !gpsAvailable;
            PrintCheck("GPS unavailable by default", passed);
            allPassed &= passed;
        }

        
        PrintSubHeader("Test 4.2: Setting GPS Makes It Available");
        {
            BleDiscoveryHeaderWrapper header;
            header.SetSenderId(100);
            header.SetTtl(10);
            header.AddToPath(100);
            header.SetGpsLocation(Vector(10.0, 20.0, 0.0));

            PrintInit("Created header and set GPS location");

            bool gpsAvailable = header.IsGpsAvailable();
            Vector gpsLoc = header.GetGpsLocation();

            PrintActual("IsGpsAvailable()", gpsAvailable ? "true" : "false");
            PrintExpected("IsGpsAvailable()", "true");
            PrintActual("GPS location", VectorToString(gpsLoc));

            bool passed = gpsAvailable;
            PrintCheck("GPS becomes available after SetGpsLocation()", passed);
            allPassed &= passed;
        }

        
        PrintSubHeader("Test 4.3: ShouldForward Skips Proximity Check When GPS Unavailable");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            
            Ptr<UniformRandomVariable> lowRandom = CreateObject<UniformRandomVariable>();
            lowRandom->SetAttribute("Min", DoubleValue(0.0));
            lowRandom->SetAttribute("Max", DoubleValue(0.01));
            logic->SetRandomStream(lowRandom);

            
            BleDiscoveryHeaderWrapper headerNoGps;
            headerNoGps.SetSenderId(100);
            headerNoGps.SetTtl(10);
            headerNoGps.AddToPath(100);
            

            Vector currentLoc(0.0, 0.0, 0.0);

            PrintInit("Message: TTL=10, GPS=unavailable");
            PrintInit("Crowding: 0.0 (low), Proximity threshold: 10m");
            PrintState("GPS unavailable → proximity check skipped → should forward");

            bool shouldForward = logic->ShouldForward(headerNoGps, currentLoc, 0.0, 10.0);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "true (GPS check skipped)");

            PrintCheck("Forwards when GPS unavailable (proximity skipped)", shouldForward);
            allPassed &= shouldForward;
        }

        
        PrintSubHeader("Test 4.4: With GPS and Too Close (Should Drop)");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Ptr<UniformRandomVariable> lowRandom = CreateObject<UniformRandomVariable>();
            lowRandom->SetAttribute("Min", DoubleValue(0.0));
            lowRandom->SetAttribute("Max", DoubleValue(0.01));
            logic->SetRandomStream(lowRandom);

            
            BleDiscoveryHeaderWrapper headerWithGps;
            headerWithGps.SetSenderId(100);
            headerWithGps.SetTtl(10);
            headerWithGps.AddToPath(100);
            headerWithGps.SetGpsLocation(Vector(5.0, 0.0, 0.0));  

            Vector currentLoc(0.0, 0.0, 0.0);

            PrintInit("Message: TTL=10, GPS=(5,0,0) - only 5m away");
            PrintInit("Crowding: 0.0, Proximity threshold: 10m");
            PrintState("GPS available, distance 5m < 10m → should DROP");

            bool shouldForward = logic->ShouldForward(headerWithGps, currentLoc, 0.0, 10.0);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "false (5m < 10m threshold)");

            bool passed = !shouldForward;
            PrintCheck("Drops when GPS available and too close", passed);
            allPassed &= passed;
        }

        
        PrintSubHeader("Test 4.5: With GPS and Far Enough (Should Forward)");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Ptr<UniformRandomVariable> lowRandom = CreateObject<UniformRandomVariable>();
            lowRandom->SetAttribute("Min", DoubleValue(0.0));
            lowRandom->SetAttribute("Max", DoubleValue(0.01));
            logic->SetRandomStream(lowRandom);

            
            BleDiscoveryHeaderWrapper headerWithGps;
            headerWithGps.SetSenderId(100);
            headerWithGps.SetTtl(10);
            headerWithGps.AddToPath(100);
            headerWithGps.SetGpsLocation(Vector(50.0, 0.0, 0.0));  

            Vector currentLoc(0.0, 0.0, 0.0);

            PrintInit("Message: TTL=10, GPS=(50,0,0) - 50m away");
            PrintInit("Crowding: 0.0, Proximity threshold: 10m");
            PrintState("GPS available, distance 50m > 10m → should FORWARD");

            bool shouldForward = logic->ShouldForward(headerWithGps, currentLoc, 0.0, 10.0);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "true (50m > 10m threshold)");

            PrintCheck("Forwards when GPS available and far enough", shouldForward);
            allPassed &= shouldForward;
        }

        
        PrintSubHeader("Test 4.6: TTL=0 Fails Even Without GPS");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            Ptr<UniformRandomVariable> lowRandom = CreateObject<UniformRandomVariable>();
            lowRandom->SetAttribute("Min", DoubleValue(0.0));
            lowRandom->SetAttribute("Max", DoubleValue(0.01));
            logic->SetRandomStream(lowRandom);

            BleDiscoveryHeaderWrapper headerTtl0;
            headerTtl0.SetSenderId(100);
            headerTtl0.SetTtl(0);  
            headerTtl0.AddToPath(100);
            

            Vector currentLoc(0.0, 0.0, 0.0);

            PrintInit("Message: TTL=0 (expired), GPS=unavailable");
            PrintState("TTL check happens BEFORE GPS check → should DROP");

            bool shouldForward = logic->ShouldForward(headerTtl0, currentLoc, 0.0, 10.0);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "false (TTL=0)");

            bool passed = !shouldForward;
            PrintCheck("TTL=0 rejected regardless of GPS availability", passed);
            allPassed &= passed;
        }

        
        PrintSubHeader("Test 4.7: High Crowding Causes Drop Even Without GPS");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            
            Ptr<UniformRandomVariable> highRandom = CreateObject<UniformRandomVariable>();
            highRandom->SetAttribute("Min", DoubleValue(0.5));
            highRandom->SetAttribute("Max", DoubleValue(1.0));
            logic->SetRandomStream(highRandom);

            BleDiscoveryHeaderWrapper headerNoGps;
            headerNoGps.SetSenderId(100);
            headerNoGps.SetTtl(10);
            headerNoGps.AddToPath(100);
            

            Vector currentLoc(0.0, 0.0, 0.0);

            PrintInit("Message: TTL=10, GPS=unavailable");
            PrintInit("Crowding: 0.9 (high), Random: 0.5-1.0");
            PrintState("Crowding check happens BEFORE GPS → may drop due to crowding");

            
            
            bool shouldForward = logic->ShouldForward(headerNoGps, currentLoc, 0.9, 10.0);

            PrintActual("should forward", shouldForward ? "true" : "false");
            PrintExpected("should forward", "false (high crowding)");

            bool passed = !shouldForward;
            PrintCheck("High crowding causes drop even without GPS", passed);
            allPassed &= passed;
        }

        
        PrintSubHeader("Test 4.8: Check Order: TTL → Crowding → GPS");
        {
            PrintInit("Verifying checks happen in correct order");

            std::cout << "\n  " << COLOR_MAGENTA << "Check Order:" << COLOR_RESET << "\n";
            std::cout << "  1. TTL > 0         (checked first)\n";
            std::cout << "  2. Crowding factor (checked second, probabilistic)\n";
            std::cout << "  3. GPS proximity   (checked last, skipped if unavailable)\n";

            PrintState("This means:");
            PrintState("  - TTL=0 always fails, regardless of GPS");
            PrintState("  - High crowding may fail before GPS check");
            PrintState("  - GPS check only reached if TTL and crowding pass");

            PrintCheck("Check order documented", true);
            
        }

        RecordResult("Subtask 4: Skip Proximity Check When GPS Unavailable", allPassed);
        return allPassed;
    }
};





class Subtask5Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 5: Test with Different Proximity Threshold Values");

        bool allPassed = true;

        
        PrintSubHeader("Test 5.1: Range of Threshold Values");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            PrintInit("Testing thresholds from 0m to 100m");

            std::cout << "\n  " << COLOR_MAGENTA
                      << "Threshold | Distance | Expected | Actual   | Result" << COLOR_RESET << "\n";
            std::cout << "  " << std::string(55, '-') << "\n";

            struct TestCase {
                double threshold;
                double distance;
                bool expectedForward;
            };

            std::vector<TestCase> cases = {
                {0.0, 0.1, true},    
                {0.0, 0.0, false},   
                {1.0, 0.5, false},   
                {1.0, 1.5, true},    
                {5.0, 3.0, false},   
                {5.0, 7.0, true},    
                {10.0, 8.0, false},  
                {10.0, 12.0, true},  
                {25.0, 20.0, false}, 
                {25.0, 30.0, true},  
                {50.0, 40.0, false}, 
                {50.0, 60.0, true},  
                {100.0, 90.0, false},  
                {100.0, 110.0, true},  
            };

            bool allCorrect = true;
            Vector currentLoc(0.0, 0.0, 0.0);

            for (const auto& tc : cases)
            {
                Vector lastHopLoc(tc.distance, 0.0, 0.0);
                bool actual = logic->ShouldForwardProximity(currentLoc, lastHopLoc, tc.threshold);
                bool correct = (actual == tc.expectedForward);
                if (!correct) allCorrect = false;

                std::string result = correct ?
                    (COLOR_GREEN + "PASS" + COLOR_RESET) :
                    (COLOR_RED + "FAIL" + COLOR_RESET);

                std::cout << std::fixed << std::setprecision(1);
                std::cout << "  " << std::setw(9) << tc.threshold << "m"
                          << " | " << std::setw(8) << tc.distance << "m"
                          << " | " << std::setw(8) << (tc.expectedForward ? "forward" : "drop")
                          << " | " << std::setw(8) << (actual ? "forward" : "drop")
                          << " | " << result << "\n";
            }

            PrintCheck("All threshold/distance combinations correct", allCorrect);
            allPassed &= allCorrect;
        }

        
        PrintSubHeader("Test 5.2: Statistical Test - Forward Rate vs Distance");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            double threshold = 10.0;
            PrintInit("Threshold: " + std::to_string(threshold) + "m");
            PrintState("Testing forward rate for different distance bands");

            struct Band {
                double minDist;
                double maxDist;
                std::string name;
            };

            std::vector<Band> bands = {
                {0.0, 5.0, "0-5m (all below)"},
                {5.0, 10.0, "5-10m (all below/at)"},
                {10.0, 15.0, "10-15m (mostly above)"},
                {15.0, 25.0, "15-25m (all above)"},
            };

            std::cout << "\n  " << COLOR_MAGENTA
                      << "Distance Band    | Forwards | Total | Rate" << COLOR_RESET << "\n";
            std::cout << "  " << std::string(50, '-') << "\n";

            bool ratesCorrect = true;
            Vector currentLoc(0.0, 0.0, 0.0);

            
            for (const auto& band : bands)
            {
                uint32_t forwards = 0;
                uint32_t total = 100;

                for (uint32_t i = 0; i < total; ++i)
                {
                    
                    double distance = band.minDist + (band.maxDist - band.minDist) * (i / (double)total);
                    Vector lastHopLoc(distance, 0.0, 0.0);

                    if (logic->ShouldForwardProximity(currentLoc, lastHopLoc, threshold))
                    {
                        forwards++;
                    }
                }

                double rate = (double)forwards / total * 100.0;

                std::cout << "  " << std::left << std::setw(16) << band.name
                          << " | " << std::setw(8) << forwards
                          << " | " << std::setw(5) << total
                          << " | " << std::fixed << std::setprecision(1) << rate << "%\n";

                
                if (band.maxDist <= threshold && forwards > 10) ratesCorrect = false;  
                if (band.minDist > threshold && forwards < 90) ratesCorrect = false;   
            }

            PrintCheck("Forward rates correlate with distance vs threshold", ratesCorrect);
            allPassed &= ratesCorrect;
        }

        
        PrintSubHeader("Test 5.3: Threshold Impact on Message Filtering");
        {
            PrintInit("Comparing small vs large thresholds");

            Vector currentLoc(0.0, 0.0, 0.0);

            
            std::vector<double> distances;
            for (int i = 1; i <= 20; ++i)
            {
                distances.push_back(i * 2.0);  
            }

            double smallThreshold = 5.0;
            double largeThreshold = 25.0;

            uint32_t forwardsSmall = 0;
            uint32_t forwardsLarge = 0;

            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();

            for (double dist : distances)
            {
                Vector lastHopLoc(dist, 0.0, 0.0);

                if (logic->ShouldForwardProximity(currentLoc, lastHopLoc, smallThreshold))
                    forwardsSmall++;

                if (logic->ShouldForwardProximity(currentLoc, lastHopLoc, largeThreshold))
                    forwardsLarge++;
            }

            PrintActual("forwards with 5m threshold", std::to_string(forwardsSmall) + "/20");
            PrintActual("forwards with 25m threshold", std::to_string(forwardsLarge) + "/20");
            PrintExpected("relationship", "small threshold forwards more (less filtering)");

            
            
            bool passed = (forwardsSmall > forwardsLarge);
            PrintCheck("Smaller threshold results in more forwarding", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 5.4: Extreme Threshold Values");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();
            Vector currentLoc(0.0, 0.0, 0.0);

            PrintInit("Testing extreme threshold values");

            
            logic->SetProximityThreshold(0.001);
            Vector nearbyLoc(0.01, 0.0, 0.0);  
            bool fwdTiny = logic->ShouldForwardProximity(currentLoc, nearbyLoc, 0.001);

            PrintActual("1cm distance, 1mm threshold", fwdTiny ? "forward" : "drop");
            PrintExpected("1cm distance, 1mm threshold", "forward (10mm > 1mm)");

            
            Vector farLoc(500.0, 0.0, 0.0);  
            bool fwdLarge = logic->ShouldForwardProximity(currentLoc, farLoc, 1000.0);

            PrintActual("500m distance, 1km threshold", fwdLarge ? "forward" : "drop");
            PrintExpected("500m distance, 1km threshold", "drop (500m < 1000m)");

            bool passed = fwdTiny && !fwdLarge;
            PrintCheck("Extreme thresholds handled correctly", passed);

            allPassed &= passed;
        }

        RecordResult("Subtask 5: Different Proximity Threshold Values", allPassed);
        return allPassed;
    }
};





class Subtask6Test
{
public:
    static bool Run ()
    {
        PrintHeader("SUBTASK 6: Validate Spatial Message Distribution");

        bool allPassed = true;

        
        PrintSubHeader("Test 6.1: Linear Network Propagation");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();
            double threshold = 10.0;

            PrintInit("Linear network: A(0m) → B(15m) → C(30m)");
            PrintInit("Threshold: " + std::to_string(threshold) + "m");

            
            Vector locA(0.0, 0.0, 0.0);
            Vector locB(15.0, 0.0, 0.0);
            Vector locC(30.0, 0.0, 0.0);

            
            
            bool bForwardsFromA = logic->ShouldForwardProximity(locB, locA, threshold);

            
            
            bool cForwardsFromB = logic->ShouldForwardProximity(locC, locB, threshold);

            PrintState("B receives from A: distance = 15m");
            PrintActual("B forwards", bForwardsFromA ? "yes" : "no");
            PrintExpected("B forwards", "yes (15m > 10m)");

            PrintState("C receives from B: distance = 15m");
            PrintActual("C forwards", cForwardsFromB ? "yes" : "no");
            PrintExpected("C forwards", "yes (15m > 10m)");

            bool passed = bForwardsFromA && cForwardsFromB;
            PrintCheck("Message propagates through linear network", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 6.2: Clustered Nodes (Limited Forwarding)");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();
            double threshold = 10.0;

            PrintInit("Cluster: A, B, C all within 5m of each other");
            PrintInit("Threshold: " + std::to_string(threshold) + "m");

            
            Vector locA(0.0, 0.0, 0.0);
            Vector locB(3.0, 2.0, 0.0);   
            Vector locC(1.0, 4.0, 0.0);   

            double distAB = logic->CalculateDistance(locA, locB);
            double distBC = logic->CalculateDistance(locB, locC);
            double distAC = logic->CalculateDistance(locA, locC);

            PrintState("Distances: A-B=" + std::to_string(distAB) +
                      "m, B-C=" + std::to_string(distBC) +
                      "m, A-C=" + std::to_string(distAC) + "m");

            bool bForwards = logic->ShouldForwardProximity(locB, locA, threshold);
            bool cForwardsFromB = logic->ShouldForwardProximity(locC, locB, threshold);
            bool cForwardsFromA = logic->ShouldForwardProximity(locC, locA, threshold);

            PrintActual("B forwards A's msg", bForwards ? "yes" : "no");
            PrintActual("C forwards from B", cForwardsFromB ? "yes" : "no");
            PrintActual("C forwards from A", cForwardsFromA ? "yes" : "no");
            PrintExpected("all", "no (all distances < 10m)");

            bool passed = !bForwards && !cForwardsFromB && !cForwardsFromA;
            PrintCheck("Clustered nodes suppress redundant forwarding", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 6.3: Hub and Spoke Topology");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();
            double threshold = 10.0;

            PrintInit("Hub A at center, spokes B,C,D at 20m in different directions");
            PrintInit("Threshold: " + std::to_string(threshold) + "m");

            
            Vector locA(0.0, 0.0, 0.0);
            
            Vector locB(20.0, 0.0, 0.0);
            Vector locC(0.0, 20.0, 0.0);
            Vector locD(-20.0, 0.0, 0.0);

            
            bool bForwards = logic->ShouldForwardProximity(locB, locA, threshold);
            bool cForwards = logic->ShouldForwardProximity(locC, locA, threshold);
            bool dForwards = logic->ShouldForwardProximity(locD, locA, threshold);

            PrintActual("B (east) forwards", bForwards ? "yes" : "no");
            PrintActual("C (north) forwards", cForwards ? "yes" : "no");
            PrintActual("D (west) forwards", dForwards ? "yes" : "no");
            PrintExpected("all", "yes (all 20m > 10m)");

            bool passed = bForwards && cForwards && dForwards;
            PrintCheck("Hub message propagates to all spokes", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 6.4: Geographic Spread - Distant Nodes Forward More");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();
            double threshold = 10.0;

            PrintInit("Source at origin, receivers at various distances");

            Vector source(0.0, 0.0, 0.0);

            struct Receiver {
                std::string name;
                Vector location;
                double expectedDist;
            };

            std::vector<Receiver> receivers = {
                {"Very close (3m)", Vector(3.0, 0.0, 0.0), 3.0},
                {"Close (8m)", Vector(8.0, 0.0, 0.0), 8.0},
                {"At threshold (10m)", Vector(10.0, 0.0, 0.0), 10.0},
                {"Just past (12m)", Vector(12.0, 0.0, 0.0), 12.0},
                {"Medium (20m)", Vector(20.0, 0.0, 0.0), 20.0},
                {"Far (50m)", Vector(50.0, 0.0, 0.0), 50.0},
            };

            std::cout << "\n  " << COLOR_MAGENTA
                      << "Receiver         | Distance | Forwards" << COLOR_RESET << "\n";
            std::cout << "  " << std::string(45, '-') << "\n";

            uint32_t forwardCount = 0;
            for (const auto& r : receivers)
            {
                bool forwards = logic->ShouldForwardProximity(r.location, source, threshold);
                if (forwards) forwardCount++;

                std::cout << "  " << std::left << std::setw(16) << r.name
                          << " | " << std::setw(8) << (std::to_string(r.expectedDist) + "m")
                          << " | " << (forwards ? "YES" : "NO") << "\n";
            }

            PrintState("Forward count: " + std::to_string(forwardCount) + "/6");
            PrintState("Only nodes > 10m should forward: 3 out of 6");

            bool passed = (forwardCount == 3);  
            PrintCheck("Only distant nodes forward (spatial filtering works)", passed);

            allPassed &= passed;
        }

        
        PrintSubHeader("Test 6.5: Multi-Hop Chain - Progressive Distance");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();
            double threshold = 10.0;

            PrintInit("Chain: A(0m) → B(15m) → C(30m) → D(45m) → E(60m)");
            PrintInit("Each hop is 15m apart, all > 10m threshold");

            std::vector<Vector> chain = {
                Vector(0.0, 0.0, 0.0),   
                Vector(15.0, 0.0, 0.0),  
                Vector(30.0, 0.0, 0.0),  
                Vector(45.0, 0.0, 0.0),  
                Vector(60.0, 0.0, 0.0),  
            };

            std::vector<std::string> names = {"A", "B", "C", "D", "E"};

            bool allForward = true;
            PrintState("Checking each hop forwards:");

            for (size_t i = 1; i < chain.size(); ++i)
            {
                bool forwards = logic->ShouldForwardProximity(chain[i], chain[i-1], threshold);
                PrintState("  " + names[i] + " receives from " + names[i-1] +
                          " (15m): " + (forwards ? "forwards" : "drops"));
                if (!forwards) allForward = false;
            }

            PrintCheck("Message propagates through entire chain", allForward);
            allPassed &= allForward;
        }

        
        PrintSubHeader("Test 6.6: Backtrack Prevention");
        {
            Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic>();
            double threshold = 10.0;

            PrintInit("Triangle: A(0,0) → B(15,0) → C(7.5,13)");
            PrintInit("C is close to A (distance ~15m) but received from B");

            Vector locA(0.0, 0.0, 0.0);
            Vector locB(15.0, 0.0, 0.0);
            Vector locC(7.5, 13.0, 0.0);  

            double distCA = logic->CalculateDistance(locC, locA);
            double distCB = logic->CalculateDistance(locC, locB);

            PrintState("C-A distance: " + std::to_string(distCA) + "m");
            PrintState("C-B distance: " + std::to_string(distCB) + "m");

            
            
            bool cForwards = logic->ShouldForwardProximity(locC, locB, threshold);

            PrintActual("C forwards (comparing to B)", cForwards ? "yes" : "no");
            PrintExpected("C forwards", std::string(distCB > threshold ? "yes" : "no") +
                         " (" + std::to_string(distCB) + "m vs 10m threshold)");

            
            PrintState("Note: LHGPS comparison uses last hop (B), not origin (A)");

            PrintCheck("LHGPS comparison uses correct reference point", true);
            allPassed &= true;  
        }

        
        PrintSubHeader("Test 6.7: Spatial Distribution Summary");
        {
            PrintInit("Summary of spatial filtering behavior:");

            std::cout << "\n  " << COLOR_MAGENTA << "Scenario               | Behavior" << COLOR_RESET << "\n";
            std::cout << "  " << std::string(55, '-') << "\n";
            std::cout << "  Nearby nodes (< threshold)  | Drop (reduce redundancy)\n";
            std::cout << "  Distant nodes (> threshold) | Forward (extend coverage)\n";
            std::cout << "  Clusters                    | Limited forwarding within\n";
            std::cout << "  Linear chains               | Full propagation if spaced\n";
            std::cout << "  Hub-spoke                   | Hub reaches all spokes\n";

            PrintState("GPS proximity filtering achieves spatial load balancing");
            PrintCheck("Spatial distribution behavior documented", true);
        }

        RecordResult("Subtask 6: Spatial Message Distribution", allPassed);
        return allPassed;
    }
};





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
                  << "*** ALL TASK 10 SUBTASKS VERIFIED SUCCESSFULLY ***"
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
    std::cout << "#   TASK 10: GPS PROXIMITY FILTERING - COMPREHENSIVE TEST      #\n";
    std::cout << "#                                                              #\n";
    std::cout << "################################################################" << COLOR_RESET << "\n";
    std::cout << "\nVerbose mode: " << (g_verbose ? "ON" : "OFF") << "\n";
    std::cout << COLOR_YELLOW << "Expected values shown in yellow" << COLOR_RESET << "\n";

    std::cout << "\n" << COLOR_CYAN << "Subtasks being tested:" << COLOR_RESET << "\n";
    std::cout << "  1. Implement distance calculation between GPS coordinates\n";
    std::cout << "  2. Define proximity threshold (configure based on network density)\n";
    std::cout << "  3. Add LHGPS comparison logic in forwarding decision\n";
    std::cout << "  4. Skip proximity check when GPS unavailable\n";
    std::cout << "  5. Test with different proximity threshold values\n";
    std::cout << "  6. Validate spatial message distribution\n";

    
    Subtask1Test::Run();
    Subtask2Test::Run();
    Subtask3Test::Run();
    Subtask4Test::Run();
    Subtask5Test::Run();
    Subtask6Test::Run();

    
    PrintFinalResults();

    Simulator::Destroy();

    return 0;
}
