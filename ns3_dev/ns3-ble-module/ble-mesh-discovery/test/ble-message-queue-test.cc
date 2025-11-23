#include "ns3/test.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/ble-message-queue.h"
#include "ns3/ble-discovery-header-wrapper.h"
#include "ns3/vector.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BleMessageQueueTest");

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test basic queue initialization and operations
 */
class BleMessageQueueBasicTestCase : public TestCase
{
public:
  BleMessageQueueBasicTestCase ();
  virtual ~BleMessageQueueBasicTestCase ();

private:
  virtual void DoRun (void);
};

BleMessageQueueBasicTestCase::BleMessageQueueBasicTestCase ()
  : TestCase ("BLE Message Queue basic operations test")
{
}

BleMessageQueueBasicTestCase::~BleMessageQueueBasicTestCase ()
{
}

void
BleMessageQueueBasicTestCase::DoRun (void)
{
  Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue> ();

  
  NS_TEST_ASSERT_MSG_EQ (queue->IsEmpty (), true, "Queue should be empty initially");
  NS_TEST_ASSERT_MSG_EQ (queue->GetSize (), 0, "Queue size should be 0 initially");

  
  BleDiscoveryHeaderWrapper header;
  header.SetSenderId (100);
  header.SetTtl (10);
  header.AddToPath (50); 

  Ptr<Packet> packet = Create<Packet> ();

  
  bool result = queue->Enqueue (packet, header, 1); 
  NS_TEST_ASSERT_MSG_EQ (result, true, "Enqueue should succeed");
  NS_TEST_ASSERT_MSG_EQ (queue->IsEmpty (), false, "Queue should not be empty after enqueue");
  NS_TEST_ASSERT_MSG_EQ (queue->GetSize (), 1, "Queue size should be 1");

  
  BleDiscoveryHeaderWrapper peekedHeader;
  bool peekResult = queue->Peek (peekedHeader);
  NS_TEST_ASSERT_MSG_EQ (peekResult, true, "Peek should succeed");
  NS_TEST_ASSERT_MSG_EQ (peekedHeader.GetSenderId (), 100, "Peeked sender ID should match");
  NS_TEST_ASSERT_MSG_EQ (queue->GetSize (), 1, "Peek should not change queue size");

  
  BleDiscoveryHeaderWrapper dequeuedHeader;
  Ptr<Packet> dequeuedPacket = queue->Dequeue (dequeuedHeader);
  bool packetValid = (dequeuedPacket != nullptr);
  NS_TEST_ASSERT_MSG_EQ (packetValid, true, "Dequeue should return packet");
  NS_TEST_ASSERT_MSG_EQ (dequeuedHeader.GetSenderId (), 100, "Dequeued sender ID should match");
  NS_TEST_ASSERT_MSG_EQ (queue->IsEmpty (), true, "Queue should be empty after dequeue");
  NS_TEST_ASSERT_MSG_EQ (queue->GetSize (), 0, "Queue size should be 0 after dequeue");

  
  Ptr<Packet> emptyDequeue = queue->Dequeue (dequeuedHeader);
  bool emptyDequeueNull = (emptyDequeue == nullptr);
  NS_TEST_ASSERT_MSG_EQ (emptyDequeueNull, true, "Dequeue from empty queue should return nullptr");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test message deduplication
 */
class BleMessageQueueDeduplicationTestCase : public TestCase
{
public:
  BleMessageQueueDeduplicationTestCase ();
  virtual ~BleMessageQueueDeduplicationTestCase ();

private:
  virtual void DoRun (void);
};

BleMessageQueueDeduplicationTestCase::BleMessageQueueDeduplicationTestCase ()
  : TestCase ("BLE Message Queue deduplication test")
{
}

BleMessageQueueDeduplicationTestCase::~BleMessageQueueDeduplicationTestCase ()
{
}

void
BleMessageQueueDeduplicationTestCase::DoRun (void)
{
  Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue> ();

  
  BleDiscoveryHeaderWrapper header;
  header.SetSenderId (200);
  header.SetTtl (5);
  header.AddToPath (200); 

  Ptr<Packet> packet = Create<Packet> ();

  
  bool result1 = queue->Enqueue (packet, header, 1);
  NS_TEST_ASSERT_MSG_EQ (result1, true, "First enqueue should succeed");
  NS_TEST_ASSERT_MSG_EQ (queue->GetSize (), 1, "Queue size should be 1");

  
  bool result2 = queue->Enqueue (packet, header, 1);
  NS_TEST_ASSERT_MSG_EQ (result2, false, "Duplicate message should be rejected");
  NS_TEST_ASSERT_MSG_EQ (queue->GetSize (), 1, "Queue size should still be 1");

  
  BleDiscoveryHeaderWrapper header2;
  header2.SetSenderId (200);
  header2.SetTtl (4); 
  header2.AddToPath (200);
  header2.AddToPath (300); 

  bool result3 = queue->Enqueue (packet, header2, 1);
  NS_TEST_ASSERT_MSG_EQ (result3, true, "Different message from same sender should succeed");
  NS_TEST_ASSERT_MSG_EQ (queue->GetSize (), 2, "Queue size should be 2");

  
  uint32_t enqueued, dequeued, duplicates, loops, overflows;
  queue->GetStatistics (enqueued, dequeued, duplicates, loops, overflows);
  NS_TEST_ASSERT_MSG_EQ (enqueued, 2, "Should have 2 enqueued messages");
  NS_TEST_ASSERT_MSG_EQ (duplicates, 1, "Should have 1 duplicate rejected");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test PSF loop detection
 */
class BleMessageQueueLoopDetectionTestCase : public TestCase
{
public:
  BleMessageQueueLoopDetectionTestCase ();
  virtual ~BleMessageQueueLoopDetectionTestCase ();

private:
  virtual void DoRun (void);
};

BleMessageQueueLoopDetectionTestCase::BleMessageQueueLoopDetectionTestCase ()
  : TestCase ("BLE Message Queue PSF loop detection test")
{
}

BleMessageQueueLoopDetectionTestCase::~BleMessageQueueLoopDetectionTestCase ()
{
}

void
BleMessageQueueLoopDetectionTestCase::DoRun (void)
{
  Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue> ();

  
  BleDiscoveryHeaderWrapper headerWithLoop;
  headerWithLoop.SetSenderId (100);
  headerWithLoop.SetTtl (10);
  headerWithLoop.AddToPath (100); 
  headerWithLoop.AddToPath (2);
  headerWithLoop.AddToPath (5);   
  headerWithLoop.AddToPath (3);

  Ptr<Packet> packet = Create<Packet> ();

  
  bool result1 = queue->Enqueue (packet, headerWithLoop, 5);
  NS_TEST_ASSERT_MSG_EQ (result1, false, "Message with receiver in path should be rejected (loop)");
  NS_TEST_ASSERT_MSG_EQ (queue->GetSize (), 0, "Queue should be empty");

  
  BleDiscoveryHeaderWrapper headerNoLoop;
  headerNoLoop.SetSenderId (100);
  headerNoLoop.SetTtl (10);
  headerNoLoop.AddToPath (100);
  headerNoLoop.AddToPath (2);
  headerNoLoop.AddToPath (3);

  
  bool result2 = queue->Enqueue (packet, headerNoLoop, 5);
  NS_TEST_ASSERT_MSG_EQ (result2, true, "Message without receiver in path should succeed");
  NS_TEST_ASSERT_MSG_EQ (queue->GetSize (), 1, "Queue size should be 1");

  
  uint32_t enqueued, dequeued, duplicates, loops, overflows;
  queue->GetStatistics (enqueued, dequeued, duplicates, loops, overflows);
  NS_TEST_ASSERT_MSG_EQ (loops, 1, "Should have 1 loop detected");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test queue size limits and overflow handling
 */
class BleMessageQueueOverflowTestCase : public TestCase
{
public:
  BleMessageQueueOverflowTestCase ();
  virtual ~BleMessageQueueOverflowTestCase ();

private:
  virtual void DoRun (void);
};

BleMessageQueueOverflowTestCase::BleMessageQueueOverflowTestCase ()
  : TestCase ("BLE Message Queue overflow handling test")
{
}

BleMessageQueueOverflowTestCase::~BleMessageQueueOverflowTestCase ()
{
}

void
BleMessageQueueOverflowTestCase::DoRun (void)
{
  Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue> ();
  Ptr<Packet> packet = Create<Packet> ();

  
  uint32_t successCount = 0;
  for (uint32_t i = 0; i < 150; ++i)
    {
      BleDiscoveryHeaderWrapper header;
      header.SetSenderId (1000 + i); 
      header.SetTtl (10);
      header.AddToPath (1000 + i);

      bool result = queue->Enqueue (packet, header, 1);
      if (result)
        {
          successCount++;
        }
    }

  
  NS_TEST_ASSERT_MSG_EQ (queue->GetSize (), 100, "Queue should be at max capacity");
  NS_TEST_ASSERT_MSG_EQ (successCount, 100, "Should have accepted exactly 100 messages");

  
  uint32_t enqueued, dequeued, duplicates, loops, overflows;
  queue->GetStatistics (enqueued, dequeued, duplicates, loops, overflows);
  NS_TEST_ASSERT_MSG_EQ (overflows, 50, "Should have 50 overflows");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test TTL-based priority ordering
 */
class BleMessageQueuePriorityTestCase : public TestCase
{
public:
  BleMessageQueuePriorityTestCase ();
  virtual ~BleMessageQueuePriorityTestCase ();

private:
  virtual void DoRun (void);
};

BleMessageQueuePriorityTestCase::BleMessageQueuePriorityTestCase ()
  : TestCase ("BLE Message Queue TTL-based priority test")
{
}

BleMessageQueuePriorityTestCase::~BleMessageQueuePriorityTestCase ()
{
}

void
BleMessageQueuePriorityTestCase::DoRun (void)
{
  Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue> ();
  Ptr<Packet> packet = Create<Packet> ();

  
  BleDiscoveryHeaderWrapper headerLowTtl;
  headerLowTtl.SetSenderId (1);
  headerLowTtl.SetTtl (2); 
  headerLowTtl.AddToPath (1);

  BleDiscoveryHeaderWrapper headerMedTtl;
  headerMedTtl.SetSenderId (2);
  headerMedTtl.SetTtl (5); 
  headerMedTtl.AddToPath (2);

  BleDiscoveryHeaderWrapper headerHighTtl;
  headerHighTtl.SetSenderId (3);
  headerHighTtl.SetTtl (10); 
  headerHighTtl.AddToPath (3);

  
  queue->Enqueue (packet, headerLowTtl, 100);
  queue->Enqueue (packet, headerHighTtl, 100);
  queue->Enqueue (packet, headerMedTtl, 100);

  NS_TEST_ASSERT_MSG_EQ (queue->GetSize (), 3, "Should have 3 messages");

  
  BleDiscoveryHeaderWrapper dequeued1;
  queue->Dequeue (dequeued1);
  NS_TEST_ASSERT_MSG_EQ (dequeued1.GetTtl (), 10, "First dequeue should have highest TTL (10)");

  BleDiscoveryHeaderWrapper dequeued2;
  queue->Dequeue (dequeued2);
  NS_TEST_ASSERT_MSG_EQ (dequeued2.GetTtl (), 5, "Second dequeue should have medium TTL (5)");

  BleDiscoveryHeaderWrapper dequeued3;
  queue->Dequeue (dequeued3);
  NS_TEST_ASSERT_MSG_EQ (dequeued3.GetTtl (), 2, "Third dequeue should have lowest TTL (2)");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test queue clear operation
 */
class BleMessageQueueClearTestCase : public TestCase
{
public:
  BleMessageQueueClearTestCase ();
  virtual ~BleMessageQueueClearTestCase ();

private:
  virtual void DoRun (void);
};

BleMessageQueueClearTestCase::BleMessageQueueClearTestCase ()
  : TestCase ("BLE Message Queue clear operation test")
{
}

BleMessageQueueClearTestCase::~BleMessageQueueClearTestCase ()
{
}

void
BleMessageQueueClearTestCase::DoRun (void)
{
  Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue> ();
  Ptr<Packet> packet = Create<Packet> ();

  
  for (uint32_t i = 0; i < 10; ++i)
    {
      BleDiscoveryHeaderWrapper header;
      header.SetSenderId (i);
      header.SetTtl (5);
      header.AddToPath (i);
      queue->Enqueue (packet, header, 100);
    }

  NS_TEST_ASSERT_MSG_EQ (queue->GetSize (), 10, "Should have 10 messages");
  NS_TEST_ASSERT_MSG_EQ (queue->IsEmpty (), false, "Queue should not be empty");

  
  queue->Clear ();

  NS_TEST_ASSERT_MSG_EQ (queue->GetSize (), 0, "Queue size should be 0 after clear");
  NS_TEST_ASSERT_MSG_EQ (queue->IsEmpty (), true, "Queue should be empty after clear");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test queue behavior under high message load
 */
class BleMessageQueueHighLoadTestCase : public TestCase
{
public:
  BleMessageQueueHighLoadTestCase ();
  virtual ~BleMessageQueueHighLoadTestCase ();

private:
  virtual void DoRun (void);
};

BleMessageQueueHighLoadTestCase::BleMessageQueueHighLoadTestCase ()
  : TestCase ("BLE Message Queue high load behavior test")
{
}

BleMessageQueueHighLoadTestCase::~BleMessageQueueHighLoadTestCase ()
{
}

void
BleMessageQueueHighLoadTestCase::DoRun (void)
{
  Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue> ();
  Ptr<Packet> packet = Create<Packet> ();

  
  uint32_t totalEnqueued = 0;
  uint32_t totalDequeued = 0;

  for (uint32_t round = 0; round < 10; ++round)
    {
      
      for (uint32_t i = 0; i < 20; ++i)
        {
          BleDiscoveryHeaderWrapper header;
          header.SetSenderId (round * 100 + i);
          header.SetTtl (10 - (i % 10)); 
          header.AddToPath (round * 100 + i);

          if (queue->Enqueue (packet, header, 999))
            {
              totalEnqueued++;
            }
        }

      
      for (uint32_t i = 0; i < 15; ++i)
        {
          BleDiscoveryHeaderWrapper header;
          if (queue->Dequeue (header) != nullptr)
            {
              totalDequeued++;
            }
        }
    }

  
  NS_TEST_ASSERT_MSG_GT (totalEnqueued, 0, "Should have enqueued some messages");
  NS_TEST_ASSERT_MSG_GT (totalDequeued, 0, "Should have dequeued some messages");

  
  NS_TEST_ASSERT_MSG_LT_OR_EQ (queue->GetSize (), 100, "Queue size should not exceed max");

  uint32_t statEnqueued, statDequeued, duplicates, loops, overflows;
  queue->GetStatistics (statEnqueued, statDequeued, duplicates, loops, overflows);
  NS_TEST_ASSERT_MSG_EQ (statEnqueued, totalEnqueued, "Statistics enqueue count should match");
  NS_TEST_ASSERT_MSG_EQ (statDequeued, totalDequeued, "Statistics dequeue count should match");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Test GPS location preservation through queue
 */
class BleMessageQueueGpsTestCase : public TestCase
{
public:
  BleMessageQueueGpsTestCase ();
  virtual ~BleMessageQueueGpsTestCase ();

private:
  virtual void DoRun (void);
};

BleMessageQueueGpsTestCase::BleMessageQueueGpsTestCase ()
  : TestCase ("BLE Message Queue GPS preservation test")
{
}

BleMessageQueueGpsTestCase::~BleMessageQueueGpsTestCase ()
{
}

void
BleMessageQueueGpsTestCase::DoRun (void)
{
  Ptr<BleMessageQueue> queue = CreateObject<BleMessageQueue> ();
  Ptr<Packet> packet = Create<Packet> ();

  
  BleDiscoveryHeaderWrapper header;
  header.SetSenderId (42);
  header.SetTtl (8);
  header.AddToPath (42);
  header.SetGpsLocation (Vector (10.5, 20.5, 30.5));

  NS_TEST_ASSERT_MSG_EQ (header.IsGpsAvailable (), true, "GPS should be available");

  
  queue->Enqueue (packet, header, 1);

  BleDiscoveryHeaderWrapper dequeuedHeader;
  queue->Dequeue (dequeuedHeader);

  
  NS_TEST_ASSERT_MSG_EQ (dequeuedHeader.IsGpsAvailable (), true, "GPS should still be available");
  Vector gps = dequeuedHeader.GetGpsLocation ();
  NS_TEST_ASSERT_MSG_EQ (gps.x, 10.5, "GPS X should be preserved");
  NS_TEST_ASSERT_MSG_EQ (gps.y, 20.5, "GPS Y should be preserved");
  NS_TEST_ASSERT_MSG_EQ (gps.z, 30.5, "GPS Z should be preserved");

  Simulator::Destroy ();
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief BLE Message Queue Test Suite
 */
class BleMessageQueueTestSuite : public TestSuite
{
public:
  BleMessageQueueTestSuite ();
};

BleMessageQueueTestSuite::BleMessageQueueTestSuite ()
  : TestSuite ("ble-message-queue", UNIT)
{
  AddTestCase (new BleMessageQueueBasicTestCase, TestCase::QUICK);
  AddTestCase (new BleMessageQueueDeduplicationTestCase, TestCase::QUICK);
  AddTestCase (new BleMessageQueueLoopDetectionTestCase, TestCase::QUICK);
  AddTestCase (new BleMessageQueueOverflowTestCase, TestCase::QUICK);
  AddTestCase (new BleMessageQueuePriorityTestCase, TestCase::QUICK);
  AddTestCase (new BleMessageQueueClearTestCase, TestCase::QUICK);
  AddTestCase (new BleMessageQueueHighLoadTestCase, TestCase::QUICK);
  AddTestCase (new BleMessageQueueGpsTestCase, TestCase::QUICK);
}

static BleMessageQueueTestSuite g_bleMessageQueueTestSuite;
