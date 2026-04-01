/**
 * @file ble-broadcast-timing-test.cc
 * @brief NS-3 integration tests for BLE broadcast timing (Tasks 12 & 14)
 */

#include "ns3/test.h"
#include "ns3/log.h"
#include "ns3/ble-broadcast-timing.h"
#include "ns3/simulator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BleBroadcastTimingTest");

/**
 * @brief Test basic initialization and operations
 */
class BleBroadcastTimingBasicTestCase : public TestCase
{
public:
    BleBroadcastTimingBasicTestCase()
        : TestCase("BLE Broadcast Timing basic operations")
    {
    }

    virtual ~BleBroadcastTimingBasicTestCase()
    {
    }

private:
    virtual void DoRun(void)
    {
        Ptr<BleBroadcastTiming> timing = CreateObject<BleBroadcastTiming>();

        /* Test initialization */
        timing->Initialize(BLE_BROADCAST_SCHEDULE_NOISY, 10, MilliSeconds(100), 0.8);

        NS_TEST_ASSERT_MSG_EQ(timing->GetNumSlots(), 10, "Num slots should be 10");
        NS_TEST_ASSERT_MSG_EQ(timing->GetSlotDuration(), MilliSeconds(100), "Slot duration should be 100ms");

        /* Test slot advancement */
        timing->AdvanceSlot();
        NS_TEST_ASSERT_MSG_LT(timing->GetCurrentSlot(), 10, "Current slot should be < 10");

        /* Test success/failure recording */
        timing->RecordSuccess();
        NS_TEST_ASSERT_MSG_EQ_TOL(timing->GetSuccessRate(), 1.0, 0.001, "Success rate should be 1.0");

        timing->RecordFailure();
        double rate = timing->GetSuccessRate();
        NS_TEST_ASSERT_MSG_EQ_TOL(rate, 0.5, 0.001, "Success rate should be 0.5");
    }
};

/**
 * @brief Test noisy broadcast schedule (Task 12)
 */
class BleBroadcastTimingNoisyTestCase : public TestCase
{
public:
    BleBroadcastTimingNoisyTestCase()
        : TestCase("BLE Noisy Broadcast Schedule (Task 12)")
    {
    }

    virtual ~BleBroadcastTimingNoisyTestCase()
    {
    }

private:
    virtual void DoRun(void)
    {
        Ptr<BleBroadcastTiming> timing = CreateObject<BleBroadcastTiming>();
        timing->Initialize(BLE_BROADCAST_SCHEDULE_NOISY, 10, MilliSeconds(50), 0.8);
        timing->SetSeed(12345);

        /* Advance through many slots */
        int listenCount = 0;
        int broadcastCount = 0;
        int trials = 200;

        for (int i = 0; i < trials; i++)
        {
            bool isBroadcast = timing->AdvanceSlot();
            if (isBroadcast)
            {
                broadcastCount++;
            }
            else
            {
                listenCount++;
            }
        }

        /* Check listen ratio is approximately 0.8 */
        double actualRatio = (double)listenCount / (double)trials;
        NS_TEST_ASSERT_MSG_GT(actualRatio, 0.7, "Listen ratio should be > 0.7");
        NS_TEST_ASSERT_MSG_LT(actualRatio, 0.9, "Listen ratio should be < 0.9");

        /* Verify stochastic randomization */
        NS_TEST_ASSERT_MSG_GT(broadcastCount, 0, "Should have some broadcast slots");
        NS_TEST_ASSERT_MSG_GT(listenCount, 0, "Should have some listen slots");
    }
};

/**
 * @brief Test stochastic broadcast timing (Task 14)
 */
class BleBroadcastTimingStochasticTestCase : public TestCase
{
public:
    BleBroadcastTimingStochasticTestCase()
        : TestCase("BLE Stochastic Broadcast Timing (Task 14)")
    {
    }

    virtual ~BleBroadcastTimingStochasticTestCase()
    {
    }

private:
    virtual void DoRun(void)
    {
        Ptr<BleBroadcastTiming> timing = CreateObject<BleBroadcastTiming>();
        timing->Initialize(BLE_BROADCAST_SCHEDULE_STOCHASTIC, 10, MilliSeconds(100), 0.75);
        timing->SetSeed(999);

        /* Test minority broadcasting (25% broadcast, 75% listen) */
        int trials = 300;
        int broadcastSlots = 0;

        for (int i = 0; i < trials; i++)
        {
            bool isBroadcast = timing->AdvanceSlot();
            if (isBroadcast)
            {
                broadcastSlots++;
            }
        }

        double broadcastRatio = (double)broadcastSlots / (double)trials;

        /* Should be around 25% (minority) */
        NS_TEST_ASSERT_MSG_GT(broadcastRatio, 0.15, "Broadcast ratio should be > 0.15");
        NS_TEST_ASSERT_MSG_LT(broadcastRatio, 0.35, "Broadcast ratio should be < 0.35");

        /* Verify actual listen ratio */
        double listenRatio = timing->GetActualListenRatio();
        NS_TEST_ASSERT_MSG_GT(listenRatio, 0.65, "Listen ratio should be > 0.65 (majority)");
        NS_TEST_ASSERT_MSG_LT(listenRatio, 0.85, "Listen ratio should be < 0.85");
    }
};

/**
 * @brief Test collision avoidance through randomization
 */
class BleBroadcastTimingCollisionAvoidanceTestCase : public TestCase
{
public:
    BleBroadcastTimingCollisionAvoidanceTestCase()
        : TestCase("BLE Collision Avoidance (Task 14)")
    {
    }

    virtual ~BleBroadcastTimingCollisionAvoidanceTestCase()
    {
    }

private:
    virtual void DoRun(void)
    {
        /* Create two nodes with different seeds */
        Ptr<BleBroadcastTiming> node1 = CreateObject<BleBroadcastTiming>();
        Ptr<BleBroadcastTiming> node2 = CreateObject<BleBroadcastTiming>();

        node1->Initialize(BLE_BROADCAST_SCHEDULE_STOCHASTIC, 10, MilliSeconds(100), 0.8);
        node2->Initialize(BLE_BROADCAST_SCHEDULE_STOCHASTIC, 10, MilliSeconds(100), 0.8);

        node1->SetSeed(111);
        node2->SetSeed(222);

        /* Count simultaneous broadcasts (collisions) */
        int collisions = 0;
        int trials = 150;

        for (int i = 0; i < trials; i++)
        {
            bool n1Broadcast = node1->AdvanceSlot();
            bool n2Broadcast = node2->AdvanceSlot();

            if (n1Broadcast && n2Broadcast)
            {
                collisions++;
            }
        }

        /* Collision rate should be low due to randomization */
        double collisionRate = (double)collisions / (double)trials;
        NS_TEST_ASSERT_MSG_LT(collisionRate, 0.1, "Collision rate should be < 10%");

        NS_LOG_INFO("Collision rate: " << collisionRate * 100 << "%");
    }
};

/**
 * @brief Test retry logic
 */
class BleBroadcastTimingRetryTestCase : public TestCase
{
public:
    BleBroadcastTimingRetryTestCase()
        : TestCase("BLE Broadcast Retry Logic (Task 14)")
    {
    }

    virtual ~BleBroadcastTimingRetryTestCase()
    {
    }

private:
    virtual void DoRun(void)
    {
        Ptr<BleBroadcastTiming> timing = CreateObject<BleBroadcastTiming>();
        timing->Initialize(BLE_BROADCAST_SCHEDULE_STOCHASTIC, 10, MilliSeconds(100), 0.5);

        /* First failure - should retry */
        bool shouldRetry = timing->RecordFailure();
        NS_TEST_ASSERT_MSG_EQ(shouldRetry, true, "Should retry after first failure");

        /* Second failure - should retry */
        shouldRetry = timing->RecordFailure();
        NS_TEST_ASSERT_MSG_EQ(shouldRetry, true, "Should retry after second failure");

        /* Third failure - max retries reached */
        shouldRetry = timing->RecordFailure();
        NS_TEST_ASSERT_MSG_EQ(shouldRetry, false, "Should not retry after max retries");

        /* Success resets retry counter */
        timing->RecordSuccess();
        shouldRetry = timing->RecordFailure();
        NS_TEST_ASSERT_MSG_EQ(shouldRetry, true, "Retry counter should reset after success");
    }
};

/**
 * @brief Test success rate calculation
 */
class BleBroadcastTimingSuccessRateTestCase : public TestCase
{
public:
    BleBroadcastTimingSuccessRateTestCase()
        : TestCase("BLE Broadcast Success Rate")
    {
    }

    virtual ~BleBroadcastTimingSuccessRateTestCase()
    {
    }

private:
    virtual void DoRun(void)
    {
        Ptr<BleBroadcastTiming> timing = CreateObject<BleBroadcastTiming>();
        timing->Initialize(BLE_BROADCAST_SCHEDULE_NOISY, 10, MilliSeconds(100), 0.5);

        /* 8 successes, 2 failures = 80% success rate */
        for (int i = 0; i < 8; i++)
        {
            timing->RecordSuccess();
        }
        for (int i = 0; i < 2; i++)
        {
            timing->RecordFailure();
        }

        double successRate = timing->GetSuccessRate();
        NS_TEST_ASSERT_MSG_EQ_TOL(successRate, 0.8, 0.001, "Success rate should be 0.8");
    }
};

/**
 * @brief Test suite for BLE broadcast timing
 */
class BleBroadcastTimingTestSuite : public TestSuite
{
public:
    BleBroadcastTimingTestSuite()
        : TestSuite("ble-broadcast-timing", UNIT)
    {
        AddTestCase(new BleBroadcastTimingBasicTestCase, TestCase::QUICK);
        AddTestCase(new BleBroadcastTimingNoisyTestCase, TestCase::QUICK);
        AddTestCase(new BleBroadcastTimingStochasticTestCase, TestCase::QUICK);
        AddTestCase(new BleBroadcastTimingCollisionAvoidanceTestCase, TestCase::QUICK);
        AddTestCase(new BleBroadcastTimingRetryTestCase, TestCase::QUICK);
        AddTestCase(new BleBroadcastTimingSuccessRateTestCase, TestCase::QUICK);
    }
};

static BleBroadcastTimingTestSuite sBleBroadcastTimingTestSuite;