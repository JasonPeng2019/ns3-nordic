#include "ns3/test.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/ble-discovery-cycle-wrapper.h"
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BleDiscoveryCycleWrapperTest");

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief Test basic initialization and configuration of BleDiscoveryCycleWrapper
 *
 * Tests:
 * - Default slot duration (100ms)
 * - Cycle duration calculation (4 slots)
 * - Setting custom slot duration
 * - Initial state (not running)
 */
class BleDiscoveryCycleWrapperBasicTestCase : public TestCase
{
public:
  BleDiscoveryCycleWrapperBasicTestCase ();
  virtual ~BleDiscoveryCycleWrapperBasicTestCase ();

private:
  virtual void DoRun (void);
};

BleDiscoveryCycleWrapperBasicTestCase::BleDiscoveryCycleWrapperBasicTestCase ()
  : TestCase ("BLE Discovery Cycle basic initialization test")
{
}

BleDiscoveryCycleWrapperBasicTestCase::~BleDiscoveryCycleWrapperBasicTestCase ()
{
}

void
BleDiscoveryCycleWrapperBasicTestCase::DoRun (void)
{
  Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper> ();

  
  NS_TEST_ASSERT_MSG_EQ (cycle->IsRunning (), false, "Cycle should not be running initially");
  NS_TEST_ASSERT_MSG_EQ (cycle->GetCurrentSlot (), 0, "Initial slot should be 0");

  
  Time slotDuration = cycle->GetSlotDuration ();
  NS_TEST_ASSERT_MSG_EQ (slotDuration, MilliSeconds (100), "Default slot duration should be 100ms");

  
  Time cycleDuration = cycle->GetCycleDuration ();
  NS_TEST_ASSERT_MSG_EQ (cycleDuration, MilliSeconds (400), "Cycle duration should be 4x slot duration (400ms)");

  
  cycle->SetSlotDuration (MilliSeconds (50));
  NS_TEST_ASSERT_MSG_EQ (cycle->GetSlotDuration (), MilliSeconds (50), "Slot duration should be 50ms");
  NS_TEST_ASSERT_MSG_EQ (cycle->GetCycleDuration (), MilliSeconds (200), "Cycle duration should be 200ms");

  
  cycle->SetSlotDuration (Seconds (1));
  NS_TEST_ASSERT_MSG_EQ (cycle->GetSlotDuration (), Seconds (1), "Slot duration should be 1s");
  NS_TEST_ASSERT_MSG_EQ (cycle->GetCycleDuration (), Seconds (4), "Cycle duration should be 4s");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief Test the 4-slot timing structure
 *
 * Tests:
 * - Slot 0: Own message transmission slot
 * - Slots 1-3: Forwarding slots
 * - Correct slot progression
 * - Timing accuracy between slots
 */
class BleDiscoveryCycleWrapperTimingStructureTestCase : public TestCase
{
public:
  BleDiscoveryCycleWrapperTimingStructureTestCase ();
  virtual ~BleDiscoveryCycleWrapperTimingStructureTestCase ();

private:
  virtual void DoRun (void);
  void RecordSlot0 ();
  void RecordSlot1 ();
  void RecordSlot2 ();
  void RecordSlot3 ();

  std::vector<std::pair<uint8_t, Time>> m_slotExecutions;
};

BleDiscoveryCycleWrapperTimingStructureTestCase::BleDiscoveryCycleWrapperTimingStructureTestCase ()
  : TestCase ("BLE Discovery Cycle 4-slot timing structure test")
{
}

BleDiscoveryCycleWrapperTimingStructureTestCase::~BleDiscoveryCycleWrapperTimingStructureTestCase ()
{
}

void
BleDiscoveryCycleWrapperTimingStructureTestCase::RecordSlot0 ()
{
  m_slotExecutions.push_back (std::make_pair (0, Simulator::Now ()));
}

void
BleDiscoveryCycleWrapperTimingStructureTestCase::RecordSlot1 ()
{
  m_slotExecutions.push_back (std::make_pair (1, Simulator::Now ()));
}

void
BleDiscoveryCycleWrapperTimingStructureTestCase::RecordSlot2 ()
{
  m_slotExecutions.push_back (std::make_pair (2, Simulator::Now ()));
}

void
BleDiscoveryCycleWrapperTimingStructureTestCase::RecordSlot3 ()
{
  m_slotExecutions.push_back (std::make_pair (3, Simulator::Now ()));
}

void
BleDiscoveryCycleWrapperTimingStructureTestCase::DoRun (void)
{
  m_slotExecutions.clear ();

  Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper> ();
  cycle->SetSlotDuration (MilliSeconds (100));

  
  cycle->SetSlot0Callback (MakeCallback (&BleDiscoveryCycleWrapperTimingStructureTestCase::RecordSlot0, this));
  cycle->SetForwardingSlotCallback (1, MakeCallback (&BleDiscoveryCycleWrapperTimingStructureTestCase::RecordSlot1, this));
  cycle->SetForwardingSlotCallback (2, MakeCallback (&BleDiscoveryCycleWrapperTimingStructureTestCase::RecordSlot2, this));
  cycle->SetForwardingSlotCallback (3, MakeCallback (&BleDiscoveryCycleWrapperTimingStructureTestCase::RecordSlot3, this));

  cycle->Start ();

  
  
  Simulator::Stop (MilliSeconds (350));
  Simulator::Run ();

  cycle->Stop ();
  Simulator::Destroy ();

  
  NS_TEST_ASSERT_MSG_EQ (m_slotExecutions.size (), 4, "Should have executed 4 slots in one cycle");

  
  NS_TEST_ASSERT_MSG_EQ (m_slotExecutions[0].first, 0, "First slot should be 0");
  NS_TEST_ASSERT_MSG_EQ (m_slotExecutions[1].first, 1, "Second slot should be 1");
  NS_TEST_ASSERT_MSG_EQ (m_slotExecutions[2].first, 2, "Third slot should be 2");
  NS_TEST_ASSERT_MSG_EQ (m_slotExecutions[3].first, 3, "Fourth slot should be 3");

  
  NS_TEST_ASSERT_MSG_EQ (m_slotExecutions[0].second, MilliSeconds (0), "Slot 0 should execute at 0ms");
  NS_TEST_ASSERT_MSG_EQ (m_slotExecutions[1].second, MilliSeconds (100), "Slot 1 should execute at 100ms");
  NS_TEST_ASSERT_MSG_EQ (m_slotExecutions[2].second, MilliSeconds (200), "Slot 2 should execute at 200ms");
  NS_TEST_ASSERT_MSG_EQ (m_slotExecutions[3].second, MilliSeconds (300), "Slot 3 should execute at 300ms");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief Test slot allocation (1 for own message, 3 for forwarding)
 *
 * Tests:
 * - Slot 0 is dedicated to own message transmission
 * - Slots 1-3 are dedicated to forwarding
 * - Invalid slot numbers are rejected
 */
class BleDiscoveryCycleWrapperSlotAllocationTestCase : public TestCase
{
public:
  BleDiscoveryCycleWrapperSlotAllocationTestCase ();
  virtual ~BleDiscoveryCycleWrapperSlotAllocationTestCase ();

private:
  virtual void DoRun (void);
  void OnSlot0 ();
  void OnSlot1 ();
  void OnSlot2 ();
  void OnSlot3 ();

  bool m_slot0Called;
  bool m_slot1Called;
  bool m_slot2Called;
  bool m_slot3Called;
};

BleDiscoveryCycleWrapperSlotAllocationTestCase::BleDiscoveryCycleWrapperSlotAllocationTestCase ()
  : TestCase ("BLE Discovery Cycle slot allocation test (1 own + 3 forwarding)"),
    m_slot0Called (false),
    m_slot1Called (false),
    m_slot2Called (false),
    m_slot3Called (false)
{
}

BleDiscoveryCycleWrapperSlotAllocationTestCase::~BleDiscoveryCycleWrapperSlotAllocationTestCase ()
{
}

void
BleDiscoveryCycleWrapperSlotAllocationTestCase::OnSlot0 ()
{
  m_slot0Called = true;
}

void
BleDiscoveryCycleWrapperSlotAllocationTestCase::OnSlot1 ()
{
  m_slot1Called = true;
}

void
BleDiscoveryCycleWrapperSlotAllocationTestCase::OnSlot2 ()
{
  m_slot2Called = true;
}

void
BleDiscoveryCycleWrapperSlotAllocationTestCase::OnSlot3 ()
{
  m_slot3Called = true;
}

void
BleDiscoveryCycleWrapperSlotAllocationTestCase::DoRun (void)
{
  m_slot0Called = false;
  m_slot1Called = false;
  m_slot2Called = false;
  m_slot3Called = false;

  Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper> ();
  cycle->SetSlotDuration (MilliSeconds (10));

  
  cycle->SetSlot0Callback (MakeCallback (&BleDiscoveryCycleWrapperSlotAllocationTestCase::OnSlot0, this));

  
  cycle->SetForwardingSlotCallback (1, MakeCallback (&BleDiscoveryCycleWrapperSlotAllocationTestCase::OnSlot1, this));
  cycle->SetForwardingSlotCallback (2, MakeCallback (&BleDiscoveryCycleWrapperSlotAllocationTestCase::OnSlot2, this));
  cycle->SetForwardingSlotCallback (3, MakeCallback (&BleDiscoveryCycleWrapperSlotAllocationTestCase::OnSlot3, this));

  
  
  

  cycle->Start ();

  
  Simulator::Stop (MilliSeconds (50));
  Simulator::Run ();

  cycle->Stop ();
  Simulator::Destroy ();

  
  NS_TEST_ASSERT_MSG_EQ (m_slot0Called, true, "Slot 0 (own message) should be called");

  
  NS_TEST_ASSERT_MSG_EQ (m_slot1Called, true, "Forwarding slot 1 should be called");
  NS_TEST_ASSERT_MSG_EQ (m_slot2Called, true, "Forwarding slot 2 should be called");
  NS_TEST_ASSERT_MSG_EQ (m_slot3Called, true, "Forwarding slot 3 should be called");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief Test discovery cycle scheduler - multiple cycles
 *
 * Tests:
 * - Cycle repeats correctly
 * - Cycle complete callback fires
 * - Multiple cycles maintain timing consistency
 */
class BleDiscoveryCycleWrapperSchedulerTestCase : public TestCase
{
public:
  BleDiscoveryCycleWrapperSchedulerTestCase ();
  virtual ~BleDiscoveryCycleWrapperSchedulerTestCase ();

private:
  virtual void DoRun (void);
  void OnSlotExecuted ();
  void OnCycleComplete ();

  uint32_t m_slotCount;
  uint32_t m_cycleCount;
  std::vector<Time> m_cycleCompleteTimes;
};

BleDiscoveryCycleWrapperSchedulerTestCase::BleDiscoveryCycleWrapperSchedulerTestCase ()
  : TestCase ("BLE Discovery Cycle scheduler test - multiple cycles"),
    m_slotCount (0),
    m_cycleCount (0)
{
}

BleDiscoveryCycleWrapperSchedulerTestCase::~BleDiscoveryCycleWrapperSchedulerTestCase ()
{
}

void
BleDiscoveryCycleWrapperSchedulerTestCase::OnSlotExecuted ()
{
  m_slotCount++;
}

void
BleDiscoveryCycleWrapperSchedulerTestCase::OnCycleComplete ()
{
  m_cycleCount++;
  m_cycleCompleteTimes.push_back (Simulator::Now ());
}

void
BleDiscoveryCycleWrapperSchedulerTestCase::DoRun (void)
{
  m_slotCount = 0;
  m_cycleCount = 0;
  m_cycleCompleteTimes.clear ();

  Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper> ();
  cycle->SetSlotDuration (MilliSeconds (50));

  
  cycle->SetSlot0Callback (MakeCallback (&BleDiscoveryCycleWrapperSchedulerTestCase::OnSlotExecuted, this));
  cycle->SetForwardingSlotCallback (1, MakeCallback (&BleDiscoveryCycleWrapperSchedulerTestCase::OnSlotExecuted, this));
  cycle->SetForwardingSlotCallback (2, MakeCallback (&BleDiscoveryCycleWrapperSchedulerTestCase::OnSlotExecuted, this));
  cycle->SetForwardingSlotCallback (3, MakeCallback (&BleDiscoveryCycleWrapperSchedulerTestCase::OnSlotExecuted, this));
  cycle->SetCycleCompleteCallback (MakeCallback (&BleDiscoveryCycleWrapperSchedulerTestCase::OnCycleComplete, this));

  cycle->Start ();

  
  
  
  Simulator::Stop (MilliSeconds (599));
  Simulator::Run ();

  cycle->Stop ();
  Simulator::Destroy ();

  
  NS_TEST_ASSERT_MSG_EQ (m_slotCount, 12, "Should have 12 slot executions (4 slots * 3 cycles)");

  
  NS_TEST_ASSERT_MSG_EQ (m_cycleCount, 2, "Should have 2 cycle completions");

  
  NS_TEST_ASSERT_MSG_EQ (m_cycleCompleteTimes.size (), 2, "Should have 2 cycle completion times");
  NS_TEST_ASSERT_MSG_EQ (m_cycleCompleteTimes[0], MilliSeconds (200), "First cycle should complete at 200ms");
  NS_TEST_ASSERT_MSG_EQ (m_cycleCompleteTimes[1], MilliSeconds (400), "Second cycle should complete at 400ms");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief Test start/stop behavior
 *
 * Tests:
 * - Starting cycle sets running flag
 * - Stopping cycle cancels events
 * - Cannot change slot duration while running
 * - Double start is handled gracefully
 * - Double stop is handled gracefully
 */
class BleDiscoveryCycleWrapperStartStopTestCase : public TestCase
{
public:
  BleDiscoveryCycleWrapperStartStopTestCase ();
  virtual ~BleDiscoveryCycleWrapperStartStopTestCase ();

private:
  virtual void DoRun (void);
  void OnSlotExecuted ();

  uint32_t m_executionCount;
};

BleDiscoveryCycleWrapperStartStopTestCase::BleDiscoveryCycleWrapperStartStopTestCase ()
  : TestCase ("BLE Discovery Cycle start-stop behavior test"),
    m_executionCount (0)
{
}

BleDiscoveryCycleWrapperStartStopTestCase::~BleDiscoveryCycleWrapperStartStopTestCase ()
{
}

void
BleDiscoveryCycleWrapperStartStopTestCase::OnSlotExecuted ()
{
  m_executionCount++;
}

void
BleDiscoveryCycleWrapperStartStopTestCase::DoRun (void)
{
  m_executionCount = 0;

  Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper> ();
  cycle->SetSlotDuration (MilliSeconds (10));
  cycle->SetSlot0Callback (MakeCallback (&BleDiscoveryCycleWrapperStartStopTestCase::OnSlotExecuted, this));

  
  NS_TEST_ASSERT_MSG_EQ (cycle->IsRunning (), false, "Should not be running initially");

  
  cycle->Start ();
  NS_TEST_ASSERT_MSG_EQ (cycle->IsRunning (), true, "Should be running after Start()");

  
  cycle->Start ();
  NS_TEST_ASSERT_MSG_EQ (cycle->IsRunning (), true, "Should still be running after double Start()");

  
  Time originalDuration = cycle->GetSlotDuration ();
  cycle->SetSlotDuration (MilliSeconds (500));
  NS_TEST_ASSERT_MSG_EQ (cycle->GetSlotDuration (), originalDuration,
                         "Slot duration should not change while running");

  
  Simulator::Stop (MilliSeconds (25));
  Simulator::Run ();

  
  cycle->Stop ();
  NS_TEST_ASSERT_MSG_EQ (cycle->IsRunning (), false, "Should not be running after Stop()");

  
  cycle->Stop ();
  NS_TEST_ASSERT_MSG_EQ (cycle->IsRunning (), false, "Should still not be running after double Stop()");

  
  uint32_t countBeforeMoreTime = m_executionCount;

  Simulator::Stop (MilliSeconds (100));
  Simulator::Run ();

  NS_TEST_ASSERT_MSG_EQ (m_executionCount, countBeforeMoreTime,
                         "No more executions should occur after Stop()");

  
  cycle->SetSlotDuration (MilliSeconds (25));
  NS_TEST_ASSERT_MSG_EQ (cycle->GetSlotDuration (), MilliSeconds (25),
                         "Slot duration should change after stopping");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief Test current slot tracking
 *
 * Tests:
 * - GetCurrentSlot() returns correct slot number during execution
 * - Slot number updates at correct times
 */
class BleDiscoveryCycleWrapperCurrentSlotTestCase : public TestCase
{
public:
  BleDiscoveryCycleWrapperCurrentSlotTestCase ();
  virtual ~BleDiscoveryCycleWrapperCurrentSlotTestCase ();

private:
  virtual void DoRun (void);
  void RecordCurrentSlot0 ();
  void RecordCurrentSlot1 ();
  void RecordCurrentSlot2 ();
  void RecordCurrentSlot3 ();

  Ptr<BleDiscoveryCycleWrapper> m_cycle;
  std::vector<std::pair<Time, uint8_t>> m_slotRecordings;
};

BleDiscoveryCycleWrapperCurrentSlotTestCase::BleDiscoveryCycleWrapperCurrentSlotTestCase ()
  : TestCase ("BLE Discovery Cycle current slot tracking test")
{
}

BleDiscoveryCycleWrapperCurrentSlotTestCase::~BleDiscoveryCycleWrapperCurrentSlotTestCase ()
{
}

void
BleDiscoveryCycleWrapperCurrentSlotTestCase::RecordCurrentSlot0 ()
{
  m_slotRecordings.push_back (std::make_pair (Simulator::Now (), m_cycle->GetCurrentSlot ()));
}

void
BleDiscoveryCycleWrapperCurrentSlotTestCase::RecordCurrentSlot1 ()
{
  m_slotRecordings.push_back (std::make_pair (Simulator::Now (), m_cycle->GetCurrentSlot ()));
}

void
BleDiscoveryCycleWrapperCurrentSlotTestCase::RecordCurrentSlot2 ()
{
  m_slotRecordings.push_back (std::make_pair (Simulator::Now (), m_cycle->GetCurrentSlot ()));
}

void
BleDiscoveryCycleWrapperCurrentSlotTestCase::RecordCurrentSlot3 ()
{
  m_slotRecordings.push_back (std::make_pair (Simulator::Now (), m_cycle->GetCurrentSlot ()));
}

void
BleDiscoveryCycleWrapperCurrentSlotTestCase::DoRun (void)
{
  m_slotRecordings.clear ();

  m_cycle = CreateObject<BleDiscoveryCycleWrapper> ();
  m_cycle->SetSlotDuration (MilliSeconds (100));

  
  m_cycle->SetSlot0Callback (MakeCallback (&BleDiscoveryCycleWrapperCurrentSlotTestCase::RecordCurrentSlot0, this));
  m_cycle->SetForwardingSlotCallback (1, MakeCallback (&BleDiscoveryCycleWrapperCurrentSlotTestCase::RecordCurrentSlot1, this));
  m_cycle->SetForwardingSlotCallback (2, MakeCallback (&BleDiscoveryCycleWrapperCurrentSlotTestCase::RecordCurrentSlot2, this));
  m_cycle->SetForwardingSlotCallback (3, MakeCallback (&BleDiscoveryCycleWrapperCurrentSlotTestCase::RecordCurrentSlot3, this));

  m_cycle->Start ();

  
  Simulator::Stop (MilliSeconds (350));
  Simulator::Run ();

  m_cycle->Stop ();
  Simulator::Destroy ();

  
  NS_TEST_ASSERT_MSG_EQ (m_slotRecordings.size (), 4, "Should have 4 recordings");

  NS_TEST_ASSERT_MSG_EQ (m_slotRecordings[0].second, 0, "Current slot should be 0 at slot 0 callback");
  NS_TEST_ASSERT_MSG_EQ (m_slotRecordings[1].second, 1, "Current slot should be 1 at slot 1 callback");
  NS_TEST_ASSERT_MSG_EQ (m_slotRecordings[2].second, 2, "Current slot should be 2 at slot 2 callback");
  NS_TEST_ASSERT_MSG_EQ (m_slotRecordings[3].second, 3, "Current slot should be 3 at slot 3 callback");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief Test timing accuracy and consistency across multiple cycles
 *
 * Tests:
 * - Slot timing remains accurate over many cycles
 * - No timing drift occurs
 * - Consistent intervals between slots
 */
class BleDiscoveryCycleWrapperTimingAccuracyTestCase : public TestCase
{
public:
  BleDiscoveryCycleWrapperTimingAccuracyTestCase ();
  virtual ~BleDiscoveryCycleWrapperTimingAccuracyTestCase ();

private:
  virtual void DoRun (void);
  void RecordSlot0Time ();
  void RecordSlot1Time ();
  void RecordSlot2Time ();
  void RecordSlot3Time ();

  std::vector<std::pair<uint8_t, Time>> m_timings;
};

BleDiscoveryCycleWrapperTimingAccuracyTestCase::BleDiscoveryCycleWrapperTimingAccuracyTestCase ()
  : TestCase ("BLE Discovery Cycle timing accuracy test")
{
}

BleDiscoveryCycleWrapperTimingAccuracyTestCase::~BleDiscoveryCycleWrapperTimingAccuracyTestCase ()
{
}

void
BleDiscoveryCycleWrapperTimingAccuracyTestCase::RecordSlot0Time ()
{
  m_timings.push_back (std::make_pair (0, Simulator::Now ()));
}

void
BleDiscoveryCycleWrapperTimingAccuracyTestCase::RecordSlot1Time ()
{
  m_timings.push_back (std::make_pair (1, Simulator::Now ()));
}

void
BleDiscoveryCycleWrapperTimingAccuracyTestCase::RecordSlot2Time ()
{
  m_timings.push_back (std::make_pair (2, Simulator::Now ()));
}

void
BleDiscoveryCycleWrapperTimingAccuracyTestCase::RecordSlot3Time ()
{
  m_timings.push_back (std::make_pair (3, Simulator::Now ()));
}

void
BleDiscoveryCycleWrapperTimingAccuracyTestCase::DoRun (void)
{
  m_timings.clear ();

  Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper> ();
  Time slotDuration = MilliSeconds (25);
  cycle->SetSlotDuration (slotDuration);

  cycle->SetSlot0Callback (MakeCallback (&BleDiscoveryCycleWrapperTimingAccuracyTestCase::RecordSlot0Time, this));
  cycle->SetForwardingSlotCallback (1, MakeCallback (&BleDiscoveryCycleWrapperTimingAccuracyTestCase::RecordSlot1Time, this));
  cycle->SetForwardingSlotCallback (2, MakeCallback (&BleDiscoveryCycleWrapperTimingAccuracyTestCase::RecordSlot2Time, this));
  cycle->SetForwardingSlotCallback (3, MakeCallback (&BleDiscoveryCycleWrapperTimingAccuracyTestCase::RecordSlot3Time, this));

  cycle->Start ();

  
  
  
  
  Simulator::Stop (MilliSeconds (499));
  Simulator::Run ();

  cycle->Stop ();
  Simulator::Destroy ();

  
  NS_TEST_ASSERT_MSG_EQ (m_timings.size (), 20, "Should have 20 slot executions");

  
  Time cycleDuration = slotDuration * 4;

  for (uint32_t cycleNum = 0; cycleNum < 5; cycleNum++)
    {
      uint32_t baseIndex = cycleNum * 4;
      Time cycleStart = cycleDuration * cycleNum;

      
      Time expectedSlot0 = cycleStart;
      Time expectedSlot1 = cycleStart + slotDuration;
      Time expectedSlot2 = cycleStart + slotDuration * 2;
      Time expectedSlot3 = cycleStart + slotDuration * 3;

      std::stringstream ss;

      ss.str ("");
      ss << "Cycle " << cycleNum << " Slot 0 timing";
      NS_TEST_ASSERT_MSG_EQ (m_timings[baseIndex].second, expectedSlot0, ss.str ());
      NS_TEST_ASSERT_MSG_EQ (m_timings[baseIndex].first, 0, "Slot 0 ID");

      ss.str ("");
      ss << "Cycle " << cycleNum << " Slot 1 timing";
      NS_TEST_ASSERT_MSG_EQ (m_timings[baseIndex + 1].second, expectedSlot1, ss.str ());
      NS_TEST_ASSERT_MSG_EQ (m_timings[baseIndex + 1].first, 1, "Slot 1 ID");

      ss.str ("");
      ss << "Cycle " << cycleNum << " Slot 2 timing";
      NS_TEST_ASSERT_MSG_EQ (m_timings[baseIndex + 2].second, expectedSlot2, ss.str ());
      NS_TEST_ASSERT_MSG_EQ (m_timings[baseIndex + 2].first, 2, "Slot 2 ID");

      ss.str ("");
      ss << "Cycle " << cycleNum << " Slot 3 timing";
      NS_TEST_ASSERT_MSG_EQ (m_timings[baseIndex + 3].second, expectedSlot3, ss.str ());
      NS_TEST_ASSERT_MSG_EQ (m_timings[baseIndex + 3].first, 3, "Slot 3 ID");
    }
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief Test network-wide synchronization (multiple nodes with same cycle parameters)
 *
 * Tests:
 * - Multiple BleDiscoveryCycleWrapper instances can run simultaneously
 * - Each maintains independent state
 * - Synchronization can be achieved by starting at same time
 */
class BleDiscoveryCycleWrapperSynchronizationTestCase : public TestCase
{
public:
  BleDiscoveryCycleWrapperSynchronizationTestCase ();
  virtual ~BleDiscoveryCycleWrapperSynchronizationTestCase ();

private:
  virtual void DoRun (void);
  void RecordNode1Slot0 ();
  void RecordNode2Slot0 ();
  void RecordNode3Slot0 ();

  std::vector<Time> m_node1Slot0Times;
  std::vector<Time> m_node2Slot0Times;
  std::vector<Time> m_node3Slot0Times;
};

BleDiscoveryCycleWrapperSynchronizationTestCase::BleDiscoveryCycleWrapperSynchronizationTestCase ()
  : TestCase ("BLE Discovery Cycle network synchronization test")
{
}

BleDiscoveryCycleWrapperSynchronizationTestCase::~BleDiscoveryCycleWrapperSynchronizationTestCase ()
{
}

void
BleDiscoveryCycleWrapperSynchronizationTestCase::RecordNode1Slot0 ()
{
  m_node1Slot0Times.push_back (Simulator::Now ());
}

void
BleDiscoveryCycleWrapperSynchronizationTestCase::RecordNode2Slot0 ()
{
  m_node2Slot0Times.push_back (Simulator::Now ());
}

void
BleDiscoveryCycleWrapperSynchronizationTestCase::RecordNode3Slot0 ()
{
  m_node3Slot0Times.push_back (Simulator::Now ());
}

void
BleDiscoveryCycleWrapperSynchronizationTestCase::DoRun (void)
{
  m_node1Slot0Times.clear ();
  m_node2Slot0Times.clear ();
  m_node3Slot0Times.clear ();

  
  Ptr<BleDiscoveryCycleWrapper> node1Cycle = CreateObject<BleDiscoveryCycleWrapper> ();
  Ptr<BleDiscoveryCycleWrapper> node2Cycle = CreateObject<BleDiscoveryCycleWrapper> ();
  Ptr<BleDiscoveryCycleWrapper> node3Cycle = CreateObject<BleDiscoveryCycleWrapper> ();

  Time slotDuration = MilliSeconds (50);
  node1Cycle->SetSlotDuration (slotDuration);
  node2Cycle->SetSlotDuration (slotDuration);
  node3Cycle->SetSlotDuration (slotDuration);

  node1Cycle->SetSlot0Callback (MakeCallback (&BleDiscoveryCycleWrapperSynchronizationTestCase::RecordNode1Slot0, this));
  node2Cycle->SetSlot0Callback (MakeCallback (&BleDiscoveryCycleWrapperSynchronizationTestCase::RecordNode2Slot0, this));
  node3Cycle->SetSlot0Callback (MakeCallback (&BleDiscoveryCycleWrapperSynchronizationTestCase::RecordNode3Slot0, this));

  
  node1Cycle->Start ();
  node2Cycle->Start ();
  node3Cycle->Start ();

  
  
  Simulator::Stop (MilliSeconds (599));
  Simulator::Run ();

  node1Cycle->Stop ();
  node2Cycle->Stop ();
  node3Cycle->Stop ();
  Simulator::Destroy ();

  
  NS_TEST_ASSERT_MSG_EQ (m_node1Slot0Times.size (), 3, "Node 1 should have 3 slot 0 executions");
  NS_TEST_ASSERT_MSG_EQ (m_node2Slot0Times.size (), 3, "Node 2 should have 3 slot 0 executions");
  NS_TEST_ASSERT_MSG_EQ (m_node3Slot0Times.size (), 3, "Node 3 should have 3 slot 0 executions");

  
  for (size_t i = 0; i < 3; i++)
    {
      std::stringstream ss;
      ss << "Synchronized slot 0 execution " << i;
      NS_TEST_ASSERT_MSG_EQ (m_node1Slot0Times[i], m_node2Slot0Times[i], ss.str ());
      NS_TEST_ASSERT_MSG_EQ (m_node2Slot0Times[i], m_node3Slot0Times[i], ss.str ());
    }

  
  Time cycleDuration = slotDuration * 4;
  NS_TEST_ASSERT_MSG_EQ (m_node1Slot0Times[0], MilliSeconds (0), "First slot 0 at 0ms");
  NS_TEST_ASSERT_MSG_EQ (m_node1Slot0Times[1], cycleDuration, "Second slot 0 at cycle duration");
  NS_TEST_ASSERT_MSG_EQ (m_node1Slot0Times[2], cycleDuration * 2, "Third slot 0 at 2x cycle duration");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief Test TypeId and NS-3 Object system integration
 *
 * Tests:
 * - TypeId is correctly registered
 * - Object can be created through CreateObject
 */
class BleDiscoveryCycleWrapperTypeIdTestCase : public TestCase
{
public:
  BleDiscoveryCycleWrapperTypeIdTestCase ();
  virtual ~BleDiscoveryCycleWrapperTypeIdTestCase ();

private:
  virtual void DoRun (void);
};

BleDiscoveryCycleWrapperTypeIdTestCase::BleDiscoveryCycleWrapperTypeIdTestCase ()
  : TestCase ("BLE Discovery Cycle TypeId test")
{
}

BleDiscoveryCycleWrapperTypeIdTestCase::~BleDiscoveryCycleWrapperTypeIdTestCase ()
{
}

void
BleDiscoveryCycleWrapperTypeIdTestCase::DoRun (void)
{
  
  TypeId tid = BleDiscoveryCycleWrapper::GetTypeId ();
  NS_TEST_ASSERT_MSG_EQ (tid.GetName (), "ns3::BleDiscoveryCycleWrapper", "TypeId name should match");

  
  Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper> ();
  NS_TEST_ASSERT_MSG_NE (cycle, 0, "Should be able to create BleDiscoveryCycleWrapper object");

  
  NS_TEST_ASSERT_MSG_EQ (cycle->GetSlotDuration (), MilliSeconds (100),
                         "Default slot duration should be 100ms");

  
  cycle->SetSlotDuration (MilliSeconds (75));
  NS_TEST_ASSERT_MSG_EQ (cycle->GetSlotDuration (), MilliSeconds (75),
                         "Slot duration should be settable");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief Test callbacks with no callbacks set (null callback handling)
 *
 * Tests:
 * - Cycle runs without crashing when no callbacks are set
 * - Partial callbacks (some set, some not) work correctly
 */
class BleDiscoveryCycleWrapperNullCallbackTestCase : public TestCase
{
public:
  BleDiscoveryCycleWrapperNullCallbackTestCase ();
  virtual ~BleDiscoveryCycleWrapperNullCallbackTestCase ();

private:
  virtual void DoRun (void);
  void OnSlot1 ();

  bool m_slot1Called;
};

BleDiscoveryCycleWrapperNullCallbackTestCase::BleDiscoveryCycleWrapperNullCallbackTestCase ()
  : TestCase ("BLE Discovery Cycle null callback handling test"),
    m_slot1Called (false)
{
}

BleDiscoveryCycleWrapperNullCallbackTestCase::~BleDiscoveryCycleWrapperNullCallbackTestCase ()
{
}

void
BleDiscoveryCycleWrapperNullCallbackTestCase::OnSlot1 ()
{
  m_slot1Called = true;
}

void
BleDiscoveryCycleWrapperNullCallbackTestCase::DoRun (void)
{
  m_slot1Called = false;

  Ptr<BleDiscoveryCycleWrapper> cycle = CreateObject<BleDiscoveryCycleWrapper> ();
  cycle->SetSlotDuration (MilliSeconds (10));

  
  cycle->SetForwardingSlotCallback (1, MakeCallback (&BleDiscoveryCycleWrapperNullCallbackTestCase::OnSlot1, this));

  
  cycle->Start ();

  Simulator::Stop (MilliSeconds (50));
  Simulator::Run ();

  cycle->Stop ();
  Simulator::Destroy ();

  
  NS_TEST_ASSERT_MSG_EQ (m_slot1Called, true, "Slot 1 callback should be called even with other null callbacks");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief Test cycle count tracking
 *
 * Tests:
 * - GetCycleCount() returns correct count
 * - Count increments after each cycle completion
 */
class BleDiscoveryCycleWrapperCycleCountTestCase : public TestCase
{
public:
  BleDiscoveryCycleWrapperCycleCountTestCase ();
  virtual ~BleDiscoveryCycleWrapperCycleCountTestCase ();

private:
  virtual void DoRun (void);
  void OnCycleComplete ();

  Ptr<BleDiscoveryCycleWrapper> m_cycle;
  std::vector<uint32_t> m_cycleCountsAtCompletion;
};

BleDiscoveryCycleWrapperCycleCountTestCase::BleDiscoveryCycleWrapperCycleCountTestCase ()
  : TestCase ("BLE Discovery Cycle count tracking test")
{
}

BleDiscoveryCycleWrapperCycleCountTestCase::~BleDiscoveryCycleWrapperCycleCountTestCase ()
{
}

void
BleDiscoveryCycleWrapperCycleCountTestCase::OnCycleComplete ()
{
  m_cycleCountsAtCompletion.push_back (m_cycle->GetCycleCount ());
}

void
BleDiscoveryCycleWrapperCycleCountTestCase::DoRun (void)
{
  m_cycleCountsAtCompletion.clear ();

  m_cycle = CreateObject<BleDiscoveryCycleWrapper> ();
  m_cycle->SetSlotDuration (MilliSeconds (25));
  m_cycle->SetCycleCompleteCallback (MakeCallback (&BleDiscoveryCycleWrapperCycleCountTestCase::OnCycleComplete, this));

  
  NS_TEST_ASSERT_MSG_EQ (m_cycle->GetCycleCount (), 0, "Initial cycle count should be 0");

  m_cycle->Start ();

  
  Simulator::Stop (MilliSeconds (450));
  Simulator::Run ();

  m_cycle->Stop ();
  Simulator::Destroy ();

  
  NS_TEST_ASSERT_MSG_EQ (m_cycleCountsAtCompletion.size (), 4, "Should have 4 cycle completions");

  
  NS_TEST_ASSERT_MSG_EQ (m_cycleCountsAtCompletion[0], 1, "First cycle count should be 1");
  NS_TEST_ASSERT_MSG_EQ (m_cycleCountsAtCompletion[1], 2, "Second cycle count should be 2");
  NS_TEST_ASSERT_MSG_EQ (m_cycleCountsAtCompletion[2], 3, "Third cycle count should be 3");
  NS_TEST_ASSERT_MSG_EQ (m_cycleCountsAtCompletion[3], 4, "Fourth cycle count should be 4");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief BLE Discovery Cycle Test Suite
 */
class BleDiscoveryCycleWrapperTestSuite : public TestSuite
{
public:
  BleDiscoveryCycleWrapperTestSuite ();
};

BleDiscoveryCycleWrapperTestSuite::BleDiscoveryCycleWrapperTestSuite ()
  : TestSuite ("ble-discovery-cycle", UNIT)
{
  
  AddTestCase (new BleDiscoveryCycleWrapperBasicTestCase, TestCase::QUICK);
  AddTestCase (new BleDiscoveryCycleWrapperTypeIdTestCase, TestCase::QUICK);

  
  AddTestCase (new BleDiscoveryCycleWrapperTimingStructureTestCase, TestCase::QUICK);
  AddTestCase (new BleDiscoveryCycleWrapperSlotAllocationTestCase, TestCase::QUICK);
  AddTestCase (new BleDiscoveryCycleWrapperCurrentSlotTestCase, TestCase::QUICK);

  
  AddTestCase (new BleDiscoveryCycleWrapperSchedulerTestCase, TestCase::QUICK);
  AddTestCase (new BleDiscoveryCycleWrapperStartStopTestCase, TestCase::QUICK);

  
  AddTestCase (new BleDiscoveryCycleWrapperTimingAccuracyTestCase, TestCase::QUICK);

  
  AddTestCase (new BleDiscoveryCycleWrapperSynchronizationTestCase, TestCase::QUICK);

  
  AddTestCase (new BleDiscoveryCycleWrapperNullCallbackTestCase, TestCase::QUICK);

  
  AddTestCase (new BleDiscoveryCycleWrapperCycleCountTestCase, TestCase::QUICK);
}

static BleDiscoveryCycleWrapperTestSuite g_bleDiscoveryCycleTestSuite;
