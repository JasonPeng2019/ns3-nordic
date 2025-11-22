/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025
 *
 * Author: Benjamin Huh <buh07@github>
 *
 * NS-3 Integration Tests for BLE Mesh Node Wrapper
 */

#include "ns3/test.h"
#include "ns3/log.h"
#include "ns3/ble-mesh-node-wrapper.h"
#include "ns3/mobility-model.h"
#include "ns3/constant-position-mobility-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BleMeshNodeTest");

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief BLE Mesh Node Basic Functionality Test
 */
class BleMeshNodeBasicTestCase : public TestCase
{
public:
  BleMeshNodeBasicTestCase ();
  virtual ~BleMeshNodeBasicTestCase ();

private:
  virtual void DoRun (void);
};

BleMeshNodeBasicTestCase::BleMeshNodeBasicTestCase ()
  : TestCase ("BLE Mesh Node basic functionality test")
{
}

BleMeshNodeBasicTestCase::~BleMeshNodeBasicTestCase ()
{
}

void
BleMeshNodeBasicTestCase::DoRun (void)
{
  Ptr<BleMeshNodeWrapper> node = CreateObject<BleMeshNodeWrapper> ();
  node->Initialize (42);

  // Test initialization
  NS_TEST_ASSERT_MSG_EQ (node->GetNodeId (), 42, "Node ID should be 42");
  NS_TEST_ASSERT_MSG_EQ (node->GetState (), BLE_NODE_STATE_INIT, "Initial state should be INIT");
  NS_TEST_ASSERT_MSG_EQ (node->GetNeighborCount (), 0, "Initial neighbor count should be 0");
  NS_TEST_ASSERT_MSG_EQ (node->IsGpsAvailable (), false, "GPS should be unavailable initially");

  // Test GPS management
  Vector gps (10.0, 20.0, 5.0);
  node->SetGpsLocation (gps);
  NS_TEST_ASSERT_MSG_EQ (node->IsGpsAvailable (), true, "GPS should be available after setting");

  Vector retrieved = node->GetGpsLocation ();
  NS_TEST_ASSERT_MSG_EQ (retrieved.x, 10.0, "GPS X coordinate");
  NS_TEST_ASSERT_MSG_EQ (retrieved.y, 20.0, "GPS Y coordinate");
  NS_TEST_ASSERT_MSG_EQ (retrieved.z, 5.0, "GPS Z coordinate");

  node->ClearGps ();
  NS_TEST_ASSERT_MSG_EQ (node->IsGpsAvailable (), false, "GPS should be cleared");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief BLE Mesh Node State Machine Test
 */
class BleMeshNodeStateMachineTestCase : public TestCase
{
public:
  BleMeshNodeStateMachineTestCase ();
  virtual ~BleMeshNodeStateMachineTestCase ();

private:
  virtual void DoRun (void);
};

BleMeshNodeStateMachineTestCase::BleMeshNodeStateMachineTestCase ()
  : TestCase ("BLE Mesh Node state machine test")
{
}

BleMeshNodeStateMachineTestCase::~BleMeshNodeStateMachineTestCase ()
{
}

void
BleMeshNodeStateMachineTestCase::DoRun (void)
{
  Ptr<BleMeshNodeWrapper> node = CreateObject<BleMeshNodeWrapper> ();
  node->Initialize (100);

  // Test valid transitions
  bool result = node->SetState (BLE_NODE_STATE_DISCOVERY);
  NS_TEST_ASSERT_MSG_EQ (result, true, "INIT -> DISCOVERY should succeed");
  NS_TEST_ASSERT_MSG_EQ (node->GetState (), BLE_NODE_STATE_DISCOVERY, "State should be DISCOVERY");
  NS_TEST_ASSERT_MSG_EQ (node->GetPreviousState (), BLE_NODE_STATE_INIT, "Previous state should be INIT");

  result = node->SetState (BLE_NODE_STATE_EDGE);
  NS_TEST_ASSERT_MSG_EQ (result, true, "DISCOVERY -> EDGE should succeed");
  NS_TEST_ASSERT_MSG_EQ (node->GetState (), BLE_NODE_STATE_EDGE, "State should be EDGE");

  result = node->SetState (BLE_NODE_STATE_CLUSTERHEAD_CANDIDATE);
  NS_TEST_ASSERT_MSG_EQ (result, true, "EDGE -> CANDIDATE should succeed");

  result = node->SetState (BLE_NODE_STATE_CLUSTERHEAD);
  NS_TEST_ASSERT_MSG_EQ (result, true, "CANDIDATE -> CLUSTERHEAD should succeed");

  // Test state names
  std::string stateName = node->GetCurrentStateName ();
  NS_TEST_ASSERT_MSG_EQ (stateName, "CLUSTERHEAD", "State name should be CLUSTERHEAD");

  std::string edgeName = BleMeshNodeWrapper::GetStateName (BLE_NODE_STATE_EDGE);
  NS_TEST_ASSERT_MSG_EQ (edgeName, "EDGE", "Edge state name");

  // Test invalid transitions
  Ptr<BleMeshNodeWrapper> node2 = CreateObject<BleMeshNodeWrapper> ();
  node2->Initialize (101);

  result = node2->SetState (BLE_NODE_STATE_CLUSTERHEAD);
  NS_TEST_ASSERT_MSG_EQ (result, false, "INIT -> CLUSTERHEAD should fail");
  NS_TEST_ASSERT_MSG_EQ (node2->GetState (), BLE_NODE_STATE_INIT, "State should remain INIT");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief BLE Mesh Node Neighbor Management Test
 */
class BleMeshNodeNeighborTestCase : public TestCase
{
public:
  BleMeshNodeNeighborTestCase ();
  virtual ~BleMeshNodeNeighborTestCase ();

private:
  virtual void DoRun (void);
};

BleMeshNodeNeighborTestCase::BleMeshNodeNeighborTestCase ()
  : TestCase ("BLE Mesh Node neighbor management test")
{
}

BleMeshNodeNeighborTestCase::~BleMeshNodeNeighborTestCase ()
{
}

void
BleMeshNodeNeighborTestCase::DoRun (void)
{
  Ptr<BleMeshNodeWrapper> node = CreateObject<BleMeshNodeWrapper> ();
  node->Initialize (200);

  // Add neighbors
  bool result = node->AddNeighbor (100, -50, 1);
  NS_TEST_ASSERT_MSG_EQ (result, true, "Adding first neighbor should succeed");
  NS_TEST_ASSERT_MSG_EQ (node->GetNeighborCount (), 1, "Neighbor count should be 1");

  result = node->AddNeighbor (101, -55, 1);
  NS_TEST_ASSERT_MSG_EQ (result, true, "Adding second neighbor should succeed");
  NS_TEST_ASSERT_MSG_EQ (node->GetNeighborCount (), 2, "Neighbor count should be 2");

  result = node->AddNeighbor (200, -70, 2);
  NS_TEST_ASSERT_MSG_EQ (result, true, "Adding third neighbor should succeed");
  NS_TEST_ASSERT_MSG_EQ (node->GetNeighborCount (), 3, "Neighbor count should be 3");

  // Test direct neighbor count
  uint16_t directCount = node->GetDirectNeighborCount ();
  NS_TEST_ASSERT_MSG_EQ (directCount, 2, "Direct neighbor count should be 2");

  // Test average RSSI
  // Average of -50, -55, -70 = -58.33... = -58 (integer division)
  int8_t avgRssi = node->GetAverageRssi ();
  NS_TEST_ASSERT_MSG_EQ (avgRssi, -58, "Average RSSI should be -58");

  // Test neighbor GPS update
  Vector gps (15.0, 25.0, 3.0);
  result = node->UpdateNeighborGps (100, gps);
  NS_TEST_ASSERT_MSG_EQ (result, true, "Updating neighbor GPS should succeed");

  // Test updating non-existent neighbor
  result = node->UpdateNeighborGps (999, gps);
  NS_TEST_ASSERT_MSG_EQ (result, false, "Updating non-existent neighbor should fail");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief BLE Mesh Node Election Logic Test
 */
class BleMeshNodeElectionTestCase : public TestCase
{
public:
  BleMeshNodeElectionTestCase ();
  virtual ~BleMeshNodeElectionTestCase ();

private:
  virtual void DoRun (void);
};

BleMeshNodeElectionTestCase::BleMeshNodeElectionTestCase ()
  : TestCase ("BLE Mesh Node election logic test")
{
}

BleMeshNodeElectionTestCase::~BleMeshNodeElectionTestCase ()
{
}

void
BleMeshNodeElectionTestCase::DoRun (void)
{
  // Test edge node decision (few neighbors)
  Ptr<BleMeshNodeWrapper> edgeNode = CreateObject<BleMeshNodeWrapper> ();
  edgeNode->Initialize (300);
  edgeNode->AddNeighbor (100, -50, 1);
  edgeNode->AddNeighbor (101, -55, 1);

  bool shouldBeEdge = edgeNode->ShouldBecomeEdge ();
  NS_TEST_ASSERT_MSG_EQ (shouldBeEdge, true, "Node with 2 neighbors should become edge");

  bool shouldBeCandidate = edgeNode->ShouldBecomeCandidate ();
  NS_TEST_ASSERT_MSG_EQ (shouldBeCandidate, false, "Node with 2 neighbors should not be candidate");

  // Test candidate node decision (many neighbors)
  Ptr<BleMeshNodeWrapper> candidateNode = CreateObject<BleMeshNodeWrapper> ();
  candidateNode->Initialize (301);
  for (uint32_t i = 0; i < 10; i++)
    {
      candidateNode->AddNeighbor (1000 + i, -50, 1);
    }

  shouldBeEdge = candidateNode->ShouldBecomeEdge ();
  NS_TEST_ASSERT_MSG_EQ (shouldBeEdge, false, "Node with 10 neighbors should not become edge");

  shouldBeCandidate = candidateNode->ShouldBecomeCandidate ();
  NS_TEST_ASSERT_MSG_EQ (shouldBeCandidate, true, "Node with 10 neighbors should be candidate");

  // Test candidacy score
  double score = candidateNode->CalculateCandidacyScore (0.5);
  NS_TEST_ASSERT_MSG_GT (score, 0.0, "Candidacy score should be positive");

  candidateNode->SetCandidacyScore (42.5);
  NS_TEST_ASSERT_MSG_EQ (candidateNode->GetCandidacyScore (), 42.5, "Candidacy score should be set");

  // Test PDSF
  candidateNode->SetPdsf (100);
  NS_TEST_ASSERT_MSG_EQ (candidateNode->GetPdsf (), 100, "PDSF should be set");

  // Test election hash
  uint32_t hash = candidateNode->GetElectionHash ();
  NS_TEST_ASSERT_MSG_NE (hash, 0, "Election hash should be generated");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief BLE Mesh Node Statistics Test
 */
class BleMeshNodeStatisticsTestCase : public TestCase
{
public:
  BleMeshNodeStatisticsTestCase ();
  virtual ~BleMeshNodeStatisticsTestCase ();

private:
  virtual void DoRun (void);
};

BleMeshNodeStatisticsTestCase::BleMeshNodeStatisticsTestCase ()
  : TestCase ("BLE Mesh Node statistics test")
{
}

BleMeshNodeStatisticsTestCase::~BleMeshNodeStatisticsTestCase ()
{
}

void
BleMeshNodeStatisticsTestCase::DoRun (void)
{
  Ptr<BleMeshNodeWrapper> node = CreateObject<BleMeshNodeWrapper> ();
  node->Initialize (400);

  // Test initial statistics
  NS_TEST_ASSERT_MSG_EQ (node->GetMessagesSent (), 0, "Initial messages sent should be 0");
  NS_TEST_ASSERT_MSG_EQ (node->GetMessagesReceived (), 0, "Initial messages received should be 0");
  NS_TEST_ASSERT_MSG_EQ (node->GetMessagesForwarded (), 0, "Initial messages forwarded should be 0");
  NS_TEST_ASSERT_MSG_EQ (node->GetMessagesDropped (), 0, "Initial messages dropped should be 0");
  NS_TEST_ASSERT_MSG_EQ (node->GetDiscoveryCycles (), 0, "Initial discovery cycles should be 0");

  // Test message counters
  node->IncrementSent ();
  node->IncrementSent ();
  NS_TEST_ASSERT_MSG_EQ (node->GetMessagesSent (), 2, "Messages sent should be 2");

  node->IncrementReceived ();
  NS_TEST_ASSERT_MSG_EQ (node->GetMessagesReceived (), 1, "Messages received should be 1");

  node->IncrementForwarded ();
  node->IncrementForwarded ();
  node->IncrementForwarded ();
  NS_TEST_ASSERT_MSG_EQ (node->GetMessagesForwarded (), 3, "Messages forwarded should be 3");

  node->IncrementDropped ();
  NS_TEST_ASSERT_MSG_EQ (node->GetMessagesDropped (), 1, "Messages dropped should be 1");

  // Test cycle advancement
  node->AdvanceCycle ();
  NS_TEST_ASSERT_MSG_EQ (node->GetCurrentCycle (), 1, "Current cycle should be 1");
  NS_TEST_ASSERT_MSG_EQ (node->GetDiscoveryCycles (), 1, "Discovery cycles should be 1");

  node->AdvanceCycle ();
  NS_TEST_ASSERT_MSG_EQ (node->GetCurrentCycle (), 2, "Current cycle should be 2");

  // Test statistics update
  node->AddNeighbor (100, -50, 1);
  node->AddNeighbor (101, -60, 1);
  node->UpdateStatistics ();
  // Statistics are updated internally, this should not crash
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief BLE Mesh Node Clustering Test
 */
class BleMeshNodeClusteringTestCase : public TestCase
{
public:
  BleMeshNodeClusteringTestCase ();
  virtual ~BleMeshNodeClusteringTestCase ();

private:
  virtual void DoRun (void);
};

BleMeshNodeClusteringTestCase::BleMeshNodeClusteringTestCase ()
  : TestCase ("BLE Mesh Node clustering test")
{
}

BleMeshNodeClusteringTestCase::~BleMeshNodeClusteringTestCase ()
{
}

void
BleMeshNodeClusteringTestCase::DoRun (void)
{
  Ptr<BleMeshNodeWrapper> node = CreateObject<BleMeshNodeWrapper> ();
  node->Initialize (500);

  // Test clusterhead ID
  NS_TEST_ASSERT_MSG_EQ (node->GetClusterheadId (), BLE_MESH_INVALID_NODE_ID,
                         "Initial clusterhead ID should be invalid");

  node->SetClusterheadId (999);
  NS_TEST_ASSERT_MSG_EQ (node->GetClusterheadId (), 999, "Clusterhead ID should be set");

  // Test cluster class
  NS_TEST_ASSERT_MSG_EQ (node->GetClusterClass (), 0, "Initial cluster class should be 0");

  node->SetClusterClass (5);
  NS_TEST_ASSERT_MSG_EQ (node->GetClusterClass (), 5, "Cluster class should be set");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief BLE Mesh Node Neighbor Pruning Test
 */
class BleMeshNodePruningTestCase : public TestCase
{
public:
  BleMeshNodePruningTestCase ();
  virtual ~BleMeshNodePruningTestCase ();

private:
  virtual void DoRun (void);
};

BleMeshNodePruningTestCase::BleMeshNodePruningTestCase ()
  : TestCase ("BLE Mesh Node neighbor pruning test")
{
}

BleMeshNodePruningTestCase::~BleMeshNodePruningTestCase ()
{
}

void
BleMeshNodePruningTestCase::DoRun (void)
{
  Ptr<BleMeshNodeWrapper> node = CreateObject<BleMeshNodeWrapper> ();
  node->Initialize (600);

  // Add neighbors at cycle 0
  node->AddNeighbor (100, -50, 1);
  node->AddNeighbor (101, -55, 1);
  node->AddNeighbor (102, -60, 1);

  // Advance to cycle 10
  for (int i = 0; i < 10; i++)
    {
      node->AdvanceCycle ();
    }

  // Update one neighbor at cycle 10
  node->AddNeighbor (100, -50, 1);

  // Advance to cycle 20
  for (int i = 0; i < 10; i++)
    {
      node->AdvanceCycle ();
    }

  // Prune neighbors older than 15 cycles
  // Neighbor 100: age = 20 - 10 = 10 (keep)
  // Neighbor 101: age = 20 - 0 = 20 (remove)
  // Neighbor 102: age = 20 - 0 = 20 (remove)
  uint16_t removed = node->PruneStaleNeighbors (15);
  NS_TEST_ASSERT_MSG_EQ (removed, 2, "Should remove 2 stale neighbors");
  NS_TEST_ASSERT_MSG_EQ (node->GetNeighborCount (), 1, "Should have 1 neighbor remaining");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \ingroup tests
 *
 * \brief BLE Mesh Node Test Suite
 */
class BleMeshNodeTestSuite : public TestSuite
{
public:
  BleMeshNodeTestSuite ();
};

BleMeshNodeTestSuite::BleMeshNodeTestSuite ()
  : TestSuite ("ble-mesh-node", UNIT)
{
  AddTestCase (new BleMeshNodeBasicTestCase, TestCase::QUICK);
  AddTestCase (new BleMeshNodeStateMachineTestCase, TestCase::QUICK);
  AddTestCase (new BleMeshNodeNeighborTestCase, TestCase::QUICK);
  AddTestCase (new BleMeshNodeElectionTestCase, TestCase::QUICK);
  AddTestCase (new BleMeshNodeStatisticsTestCase, TestCase::QUICK);
  AddTestCase (new BleMeshNodeClusteringTestCase, TestCase::QUICK);
  AddTestCase (new BleMeshNodePruningTestCase, TestCase::QUICK);
}

static BleMeshNodeTestSuite g_bleMeshNodeTestSuite;
