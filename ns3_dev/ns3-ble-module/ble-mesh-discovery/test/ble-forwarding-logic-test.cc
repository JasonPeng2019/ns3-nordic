#include "ns3/test.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/ble-forwarding-logic.h"
#include "ns3/ble-discovery-header-wrapper.h"
#include "ns3/vector.h"
#include "ns3/double.h"
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BleForwardingLogicTest");

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test crowding factor calculation (Task 9)
 */
class BleForwardingCrowdingFactorTestCase : public TestCase
{
public:
  BleForwardingCrowdingFactorTestCase ();
  virtual ~BleForwardingCrowdingFactorTestCase ();

private:
  virtual void DoRun (void);
};

BleForwardingCrowdingFactorTestCase::BleForwardingCrowdingFactorTestCase ()
  : TestCase ("BLE Forwarding crowding factor calculation test")
{
}

BleForwardingCrowdingFactorTestCase::~BleForwardingCrowdingFactorTestCase ()
{
}

void
BleForwardingCrowdingFactorTestCase::DoRun (void)
{
  Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic> ();

  
  std::vector<int8_t> noSamples;
  double crowdingEmpty = logic->CalculateCrowdingFactor (noSamples);
  NS_TEST_ASSERT_MSG_EQ (crowdingEmpty, 0.0, "Empty samples should give crowding factor 0");

  
  std::vector<int8_t> weakRssi = {-90, -85, -88};
  double crowdingWeak = logic->CalculateCrowdingFactor (weakRssi);
  NS_TEST_ASSERT_MSG_LT (crowdingWeak, 0.5, "Weak RSSI should give low crowding factor");

  
  std::vector<int8_t> strongRssi = {-40, -35, -45, -38, -42};
  double crowdingStrong = logic->CalculateCrowdingFactor (strongRssi);
  NS_TEST_ASSERT_MSG_GT (crowdingStrong, 0.5, "Strong RSSI should give high crowding factor");

  
  std::vector<int8_t> mixedRssi = {-60, -70, -55, -65};
  double crowdingMixed = logic->CalculateCrowdingFactor (mixedRssi);
  NS_TEST_ASSERT_MSG_GT (crowdingMixed, 0.0, "Mixed RSSI should give positive crowding");
  NS_TEST_ASSERT_MSG_LT (crowdingMixed, 1.0, "Crowding should be less than 1.0");

  
  NS_TEST_ASSERT_MSG_GT_OR_EQ (crowdingWeak, 0.0, "Crowding should be >= 0");
  NS_TEST_ASSERT_MSG_LT_OR_EQ (crowdingStrong, 1.0, "Crowding should be <= 1");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test picky forwarding probability (Task 9)
 */
class BleForwardingPickyAlgorithmTestCase : public TestCase
{
public:
  BleForwardingPickyAlgorithmTestCase ();
  virtual ~BleForwardingPickyAlgorithmTestCase ();

private:
  virtual void DoRun (void);
};

BleForwardingPickyAlgorithmTestCase::BleForwardingPickyAlgorithmTestCase ()
  : TestCase ("BLE Forwarding picky algorithm test")
{
}

BleForwardingPickyAlgorithmTestCase::~BleForwardingPickyAlgorithmTestCase ()
{
}

void
BleForwardingPickyAlgorithmTestCase::DoRun (void)
{
  Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic> ();

  
  
  logic->SeedRandom (0x1234abcd);
  uint32_t forwardCountZero = 0;
  for (uint32_t i = 0; i < 100; ++i)
    {
      if (logic->ShouldForwardCrowding (0.0, 5))
        {
          forwardCountZero++;
        }
    }
  NS_TEST_ASSERT_MSG_EQ (forwardCountZero, 100, "Zero crowding should always forward");

  
  logic->SeedRandom (0x1234abcd);
  uint32_t forwardCountHigh = 0;
  for (uint32_t i = 0; i < 100; ++i)
    {
      if (logic->ShouldForwardCrowding (0.9, 20))
        {
          forwardCountHigh++;
        }
    }
  
  NS_TEST_ASSERT_MSG_LT (forwardCountHigh, 50, "High crowding should forward rarely");

  
  uint32_t forwardLow = 0;
  uint32_t forwardMed = 0;
  uint32_t forwardHighNew = 0;

  logic->SeedRandom (0x1234abcd);
  for (uint32_t i = 0; i < 1000; ++i)
    {
      if (logic->ShouldForwardCrowding (0.2, 20))
        forwardLow++;
      if (logic->ShouldForwardCrowding (0.5, 20))
        forwardMed++;
      if (logic->ShouldForwardCrowding (0.8, 20))
        forwardHighNew++;
    }

  NS_TEST_ASSERT_MSG_GT (forwardLow, forwardMed, "Low crowding should forward more than medium");
  NS_TEST_ASSERT_MSG_GT (forwardMed, forwardHighNew, "Medium crowding should forward more than high");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test GPS distance calculation (Task 10)
 */
class BleForwardingGpsDistanceTestCase : public TestCase
{
public:
  BleForwardingGpsDistanceTestCase ();
  virtual ~BleForwardingGpsDistanceTestCase ();

private:
  virtual void DoRun (void);
};

BleForwardingGpsDistanceTestCase::BleForwardingGpsDistanceTestCase ()
  : TestCase ("BLE Forwarding GPS distance calculation test")
{
}

BleForwardingGpsDistanceTestCase::~BleForwardingGpsDistanceTestCase ()
{
}

void
BleForwardingGpsDistanceTestCase::DoRun (void)
{
  Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic> ();

  
  Vector loc1 (10.0, 20.0, 0.0);
  double distSame = logic->CalculateDistance (loc1, loc1);
  NS_TEST_ASSERT_MSG_EQ (distSame, 0.0, "Same location should have distance 0");

  
  Vector loc2 (10.0, 20.0, 0.0);
  Vector loc3 (13.0, 24.0, 0.0); 
  double dist2d = logic->CalculateDistance (loc2, loc3);
  NS_TEST_ASSERT_MSG_EQ (dist2d, 5.0, "2D distance should be 5 meters");

  
  Vector loc4 (0.0, 0.0, 0.0);
  Vector loc5 (3.0, 4.0, 0.0);
  double dist3d = logic->CalculateDistance (loc4, loc5);
  NS_TEST_ASSERT_MSG_EQ (dist3d, 5.0, "3D distance should be 5 meters");

  
  Vector loc6 (0.0, 0.0, 0.0);
  Vector loc7 (0.0, 0.0, 10.0);
  double distZ = logic->CalculateDistance (loc6, loc7);
  NS_TEST_ASSERT_MSG_EQ (distZ, 10.0, "Vertical distance should be 10 meters");

  
  Vector loc8 (-5.0, -5.0, 0.0);
  Vector loc9 (5.0, 5.0, 0.0);
  double distNeg = logic->CalculateDistance (loc8, loc9);
  double expected = std::sqrt (200.0); 
  NS_TEST_ASSERT_MSG_EQ_TOL (distNeg, expected, 0.001, "Distance with negative coords should work");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test GPS proximity filtering (Task 10)
 */
class BleForwardingGpsProximityTestCase : public TestCase
{
public:
  BleForwardingGpsProximityTestCase ();
  virtual ~BleForwardingGpsProximityTestCase ();

private:
  virtual void DoRun (void);
};

BleForwardingGpsProximityTestCase::BleForwardingGpsProximityTestCase ()
  : TestCase ("BLE Forwarding GPS proximity filtering test")
{
}

BleForwardingGpsProximityTestCase::~BleForwardingGpsProximityTestCase ()
{
}

void
BleForwardingGpsProximityTestCase::DoRun (void)
{
  Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic> ();

  Vector currentLoc (0.0, 0.0, 0.0);
  Vector nearbyLoc (5.0, 0.0, 0.0);   
  Vector farLoc (20.0, 0.0, 0.0);     

  double threshold10m = 10.0;

  
  bool forwardNearby = logic->ShouldForwardProximity (currentLoc, nearbyLoc, threshold10m);
  NS_TEST_ASSERT_MSG_EQ (forwardNearby, false, "Should not forward from nearby node (< 10m)");

  
  bool forwardFar = logic->ShouldForwardProximity (currentLoc, farLoc, threshold10m);
  NS_TEST_ASSERT_MSG_EQ (forwardFar, true, "Should forward from far node (> 10m)");

  
  Vector thresholdLoc (10.0, 0.0, 0.0); 
  bool forwardThreshold = logic->ShouldForwardProximity (currentLoc, thresholdLoc, threshold10m);
  
  NS_TEST_ASSERT_MSG_EQ (forwardThreshold, false, "Should not forward at exactly threshold");

  
  double threshold5m = 5.0;
  bool forwardNearbySmallThreshold = logic->ShouldForwardProximity (currentLoc, nearbyLoc, threshold5m);
  
  NS_TEST_ASSERT_MSG_EQ (forwardNearbySmallThreshold, false, "5m away with 5m threshold should not forward");

  double threshold3m = 3.0;
  bool forwardNearbyTinyThreshold = logic->ShouldForwardProximity (currentLoc, nearbyLoc, threshold3m);
  NS_TEST_ASSERT_MSG_EQ (forwardNearbyTinyThreshold, true, "5m away with 3m threshold should forward");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test GPS unavailable handling (Task 10)
 */
class BleForwardingGpsUnavailableTestCase : public TestCase
{
public:
  BleForwardingGpsUnavailableTestCase ();
  virtual ~BleForwardingGpsUnavailableTestCase ();

private:
  virtual void DoRun (void);
};

BleForwardingGpsUnavailableTestCase::BleForwardingGpsUnavailableTestCase ()
  : TestCase ("BLE Forwarding GPS unavailable handling test")
{
}

BleForwardingGpsUnavailableTestCase::~BleForwardingGpsUnavailableTestCase ()
{
}

void
BleForwardingGpsUnavailableTestCase::DoRun (void)
{
  Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic> ();

  
  BleDiscoveryHeaderWrapper headerNoGps;
  headerNoGps.SetSenderId (100);
  headerNoGps.SetTtl (10);
  headerNoGps.AddToPath (100);
  

  NS_TEST_ASSERT_MSG_EQ (headerNoGps.IsGpsAvailable (), false, "GPS should not be available");

  
  
  Vector currentLoc (0.0, 0.0, 0.0);

  logic->SeedRandom (0x1);

  bool shouldForward = logic->ShouldForward (headerNoGps, currentLoc, 0.0, 10.0, 5);
  
  NS_TEST_ASSERT_MSG_EQ (shouldForward, true, "Should forward when GPS unavailable");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test TTL-based priority calculation (Task 11)
 */
class BleForwardingTtlPriorityTestCase : public TestCase
{
public:
  BleForwardingTtlPriorityTestCase ();
  virtual ~BleForwardingTtlPriorityTestCase ();

private:
  virtual void DoRun (void);
};

BleForwardingTtlPriorityTestCase::BleForwardingTtlPriorityTestCase ()
  : TestCase ("BLE Forwarding TTL-based priority calculation test")
{
}

BleForwardingTtlPriorityTestCase::~BleForwardingTtlPriorityTestCase ()
{
}

void
BleForwardingTtlPriorityTestCase::DoRun (void)
{
  Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic> ();

  
  BleDiscoveryHeaderWrapper headerHighTtl;
  headerHighTtl.SetTtl (15);

  BleDiscoveryHeaderWrapper headerMedTtl;
  headerMedTtl.SetTtl (8);

  BleDiscoveryHeaderWrapper headerLowTtl;
  headerLowTtl.SetTtl (2);

  uint8_t priorityHigh = logic->CalculatePriority (headerHighTtl);
  uint8_t priorityMed = logic->CalculatePriority (headerMedTtl);
  uint8_t priorityLow = logic->CalculatePriority (headerLowTtl);

  
  NS_TEST_ASSERT_MSG_LT (priorityHigh, priorityMed, "High TTL should have lower priority value");
  NS_TEST_ASSERT_MSG_LT (priorityMed, priorityLow, "Medium TTL should have lower priority value than low");

  
  BleDiscoveryHeaderWrapper headerMaxTtl;
  headerMaxTtl.SetTtl (255);
  uint8_t priorityMax = logic->CalculatePriority (headerMaxTtl);

  BleDiscoveryHeaderWrapper headerMinTtl;
  headerMinTtl.SetTtl (1);
  uint8_t priorityMin = logic->CalculatePriority (headerMinTtl);

  NS_TEST_ASSERT_MSG_LT (priorityMax, priorityMin, "Max TTL should have highest priority");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test TTL expiration handling (Task 11)
 */
class BleForwardingTtlExpirationTestCase : public TestCase
{
public:
  BleForwardingTtlExpirationTestCase ();
  virtual ~BleForwardingTtlExpirationTestCase ();

private:
  virtual void DoRun (void);
};

BleForwardingTtlExpirationTestCase::BleForwardingTtlExpirationTestCase ()
  : TestCase ("BLE Forwarding TTL expiration handling test")
{
}

BleForwardingTtlExpirationTestCase::~BleForwardingTtlExpirationTestCase ()
{
}

void
BleForwardingTtlExpirationTestCase::DoRun (void)
{
  Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic> ();

  logic->SeedRandom (0x1);

  Vector currentLoc (0.0, 0.0, 0.0);

  
  BleDiscoveryHeaderWrapper headerTtlZero;
  headerTtlZero.SetSenderId (100);
  headerTtlZero.SetTtl (0);
  headerTtlZero.AddToPath (100);

  bool forwardTtlZero = logic->ShouldForward (headerTtlZero, currentLoc, 0.0, 0.0, 5);
  NS_TEST_ASSERT_MSG_EQ (forwardTtlZero, false, "TTL=0 message should not be forwarded");

  
  BleDiscoveryHeaderWrapper headerTtlOne;
  headerTtlOne.SetSenderId (101);
  headerTtlOne.SetTtl (1);
  headerTtlOne.AddToPath (101);

  bool forwardTtlOne = logic->ShouldForward (headerTtlOne, currentLoc, 0.0, 0.0, 5);
  NS_TEST_ASSERT_MSG_EQ (forwardTtlOne, true, "TTL=1 message should be forwarded");

  
  BleDiscoveryHeaderWrapper headerTtlHigh;
  headerTtlHigh.SetSenderId (102);
  headerTtlHigh.SetTtl (10);
  headerTtlHigh.AddToPath (102);

  bool forwardTtlHigh = logic->ShouldForward (headerTtlHigh, currentLoc, 0.0, 0.0, 5);
  NS_TEST_ASSERT_MSG_EQ (forwardTtlHigh, true, "High TTL message should be forwarded");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test TTL decrement on forwarding (Task 11)
 */
class BleForwardingTtlDecrementTestCase : public TestCase
{
public:
  BleForwardingTtlDecrementTestCase ();
  virtual ~BleForwardingTtlDecrementTestCase ();

private:
  virtual void DoRun (void);
};

BleForwardingTtlDecrementTestCase::BleForwardingTtlDecrementTestCase ()
  : TestCase ("BLE Forwarding TTL decrement test")
{
}

BleForwardingTtlDecrementTestCase::~BleForwardingTtlDecrementTestCase ()
{
}

void
BleForwardingTtlDecrementTestCase::DoRun (void)
{
  
  BleDiscoveryHeaderWrapper header;
  header.SetTtl (10);

  NS_TEST_ASSERT_MSG_EQ (header.GetTtl (), 10, "Initial TTL should be 10");

  
  bool result1 = header.DecrementTtl ();
  NS_TEST_ASSERT_MSG_EQ (result1, true, "Decrement should succeed with TTL > 0");
  NS_TEST_ASSERT_MSG_EQ (header.GetTtl (), 9, "TTL should be 9 after decrement");

  
  for (int i = 0; i < 9; ++i)
    {
      header.DecrementTtl ();
    }
  NS_TEST_ASSERT_MSG_EQ (header.GetTtl (), 0, "TTL should be 0");

  
  bool resultAtZero = header.DecrementTtl ();
  NS_TEST_ASSERT_MSG_EQ (resultAtZero, false, "Decrement at TTL=0 should fail");
  NS_TEST_ASSERT_MSG_EQ (header.GetTtl (), 0, "TTL should remain 0");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test combined forwarding decision (all three metrics)
 */
class BleForwardingCombinedTestCase : public TestCase
{
public:
  BleForwardingCombinedTestCase ();
  virtual ~BleForwardingCombinedTestCase ();

private:
  virtual void DoRun (void);
};

BleForwardingCombinedTestCase::BleForwardingCombinedTestCase ()
  : TestCase ("BLE Forwarding combined metrics test")
{
}

BleForwardingCombinedTestCase::~BleForwardingCombinedTestCase ()
{
}

void
BleForwardingCombinedTestCase::DoRun (void)
{
  Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic> ();

  logic->SeedRandom (0x1);

  
  BleDiscoveryHeaderWrapper header;
  header.SetSenderId (100);
  header.SetTtl (10);
  header.AddToPath (100);
  header.SetGpsLocation (Vector (50.0, 50.0, 0.0)); 

  Vector currentLoc (0.0, 0.0, 0.0);

  
  
  
  
  bool forward1 = logic->ShouldForward (header, currentLoc, 0.0, 10.0, 5);
  NS_TEST_ASSERT_MSG_EQ (forward1, true, "Should forward when all checks pass");

  
  BleDiscoveryHeaderWrapper headerTtl0;
  headerTtl0.SetSenderId (101);
  headerTtl0.SetTtl (0);
  headerTtl0.AddToPath (101);
  headerTtl0.SetGpsLocation (Vector (50.0, 50.0, 0.0));

  bool forward2 = logic->ShouldForward (headerTtl0, currentLoc, 0.0, 10.0, 5);
  NS_TEST_ASSERT_MSG_EQ (forward2, false, "Should not forward with TTL=0");

  
  BleDiscoveryHeaderWrapper headerClose;
  headerClose.SetSenderId (102);
  headerClose.SetTtl (10);
  headerClose.AddToPath (102);
  headerClose.SetGpsLocation (Vector (5.0, 0.0, 0.0)); 

  bool forward3 = logic->ShouldForward (headerClose, currentLoc, 0.0, 10.0, 5);
  NS_TEST_ASSERT_MSG_EQ (forward3, false, "Should not forward when GPS too close");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test proximity threshold configuration
 */
class BleForwardingThresholdConfigTestCase : public TestCase
{
public:
  BleForwardingThresholdConfigTestCase ();
  virtual ~BleForwardingThresholdConfigTestCase ();

private:
  virtual void DoRun (void);
};

BleForwardingThresholdConfigTestCase::BleForwardingThresholdConfigTestCase ()
  : TestCase ("BLE Forwarding threshold configuration test")
{
}

BleForwardingThresholdConfigTestCase::~BleForwardingThresholdConfigTestCase ()
{
}

void
BleForwardingThresholdConfigTestCase::DoRun (void)
{
  Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic> ();

  
  double defaultThreshold = logic->GetProximityThreshold ();
  NS_TEST_ASSERT_MSG_EQ (defaultThreshold, 10.0, "Default proximity threshold should be 10m");

  
  logic->SetProximityThreshold (25.0);
  NS_TEST_ASSERT_MSG_EQ (logic->GetProximityThreshold (), 25.0, "Threshold should be 25m");

  
  Ptr<BleForwardingLogic> logic2 = CreateObject<BleForwardingLogic> ();
  logic2->SetAttribute ("ProximityThreshold", DoubleValue (15.0));

  DoubleValue thresholdValue;
  logic2->GetAttribute ("ProximityThreshold", thresholdValue);
  NS_TEST_ASSERT_MSG_EQ (thresholdValue.Get (), 15.0, "Attribute should be 15m");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test TypeId registration
 */
class BleForwardingTypeIdTestCase : public TestCase
{
public:
  BleForwardingTypeIdTestCase ();
  virtual ~BleForwardingTypeIdTestCase ();

private:
  virtual void DoRun (void);
};

BleForwardingTypeIdTestCase::BleForwardingTypeIdTestCase ()
  : TestCase ("BLE Forwarding TypeId test")
{
}

BleForwardingTypeIdTestCase::~BleForwardingTypeIdTestCase ()
{
}

void
BleForwardingTypeIdTestCase::DoRun (void)
{
  TypeId tid = BleForwardingLogic::GetTypeId ();
  NS_TEST_ASSERT_MSG_EQ (tid.GetName (), "ns3::BleForwardingLogic", "TypeId name should match");

  Ptr<BleForwardingLogic> logic = CreateObject<BleForwardingLogic> ();
  bool logicCreated = (logic != nullptr);
  NS_TEST_ASSERT_MSG_EQ (logicCreated, true, "Should create object successfully");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief BLE Forwarding Logic Test Suite
 */
class BleForwardingLogicTestSuite : public TestSuite
{
public:
  BleForwardingLogicTestSuite ();
};

BleForwardingLogicTestSuite::BleForwardingLogicTestSuite ()
  : TestSuite ("ble-forwarding-logic", UNIT)
{
  
  AddTestCase (new BleForwardingCrowdingFactorTestCase, TestCase::QUICK);
  AddTestCase (new BleForwardingPickyAlgorithmTestCase, TestCase::QUICK);

  
  AddTestCase (new BleForwardingGpsDistanceTestCase, TestCase::QUICK);
  AddTestCase (new BleForwardingGpsProximityTestCase, TestCase::QUICK);
  AddTestCase (new BleForwardingGpsUnavailableTestCase, TestCase::QUICK);

  
  AddTestCase (new BleForwardingTtlPriorityTestCase, TestCase::QUICK);
  AddTestCase (new BleForwardingTtlExpirationTestCase, TestCase::QUICK);
  AddTestCase (new BleForwardingTtlDecrementTestCase, TestCase::QUICK);

  
  AddTestCase (new BleForwardingCombinedTestCase, TestCase::QUICK);
  AddTestCase (new BleForwardingThresholdConfigTestCase, TestCase::QUICK);
  AddTestCase (new BleForwardingTypeIdTestCase, TestCase::QUICK);
}

static BleForwardingLogicTestSuite g_bleForwardingLogicTestSuite;
