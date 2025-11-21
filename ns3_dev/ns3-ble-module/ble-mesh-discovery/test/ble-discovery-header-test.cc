/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ns3/test.h"
#include "ns3/ble-discovery-header-wrapper.h"
#include "ns3/packet.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BleDiscoveryHeaderTest");

/**
 * \ingroup ble-mesh-discovery-test
 * \brief BLE Discovery Header Test Case
 */
class BleDiscoveryHeaderTestCase : public TestCase
{
public:
  BleDiscoveryHeaderTestCase ();
  virtual ~BleDiscoveryHeaderTestCase ();

private:
  virtual void DoRun (void);
};

BleDiscoveryHeaderTestCase::BleDiscoveryHeaderTestCase ()
  : TestCase ("BleDiscoveryHeader test case")
{
}

BleDiscoveryHeaderTestCase::~BleDiscoveryHeaderTestCase ()
{
}

void
BleDiscoveryHeaderTestCase::DoRun (void)
{
  // Test 1: Basic header creation and getters/setters
  BleDiscoveryHeaderWrapper header;
  header.SetSenderId (42);
  header.SetTtl (10);

  NS_TEST_ASSERT_MSG_EQ (header.GetSenderId (), 42, "Sender ID should be 42");
  NS_TEST_ASSERT_MSG_EQ (header.GetTtl (), 10, "TTL should be 10");

  // Test 2: TTL decrement
  bool result = header.DecrementTtl ();
  NS_TEST_ASSERT_MSG_EQ (result, true, "DecrementTtl should return true when TTL > 0");
  NS_TEST_ASSERT_MSG_EQ (header.GetTtl (), 9, "TTL should be 9 after decrement");

  // Test 3: Path So Far operations
  header.AddToPath (1);
  header.AddToPath (2);
  header.AddToPath (3);

  NS_TEST_ASSERT_MSG_EQ (header.IsInPath (2), true, "Node 2 should be in path");
  NS_TEST_ASSERT_MSG_EQ (header.IsInPath (5), false, "Node 5 should not be in path");

  std::vector<uint32_t> path = header.GetPath ();
  NS_TEST_ASSERT_MSG_EQ (path.size (), 3, "Path should have 3 nodes");
  NS_TEST_ASSERT_MSG_EQ (path[0], 1, "First node should be 1");
  NS_TEST_ASSERT_MSG_EQ (path[2], 3, "Third node should be 3");

  // Test 4: GPS location
  Vector gpsLoc (10.5, 20.5, 30.5);
  header.SetGpsLocation (gpsLoc);
  header.SetGpsAvailable (true);

  NS_TEST_ASSERT_MSG_EQ (header.IsGpsAvailable (), true, "GPS should be available");
  Vector retrieved = header.GetGpsLocation ();
  NS_TEST_ASSERT_MSG_EQ (retrieved.x, 10.5, "GPS X coordinate should match");
  NS_TEST_ASSERT_MSG_EQ (retrieved.y, 20.5, "GPS Y coordinate should match");
  NS_TEST_ASSERT_MSG_EQ (retrieved.z, 30.5, "GPS Z coordinate should match");

  // Test 5: Election announcement fields
  header.SetAsElectionMessage ();
  header.SetClassId (100);
  header.SetPdsf (150);
  header.SetScore (0.85);
  header.SetHash (12345);

  NS_TEST_ASSERT_MSG_EQ (header.IsElectionMessage (), true,
                         "Message type should be ELECTION_ANNOUNCEMENT");
  NS_TEST_ASSERT_MSG_EQ (header.GetClassId (), 100, "Class ID should be 100");
  NS_TEST_ASSERT_MSG_EQ (header.GetPdsf (), 150, "PDSF should be 150");
  NS_TEST_ASSERT_MSG_EQ (header.GetScore (), 0.85, "Score should be 0.85");
  NS_TEST_ASSERT_MSG_EQ (header.GetHash (), 12345, "Hash should be 12345");

  // Test 6: Serialization and deserialization
  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (header);

  BleDiscoveryHeaderWrapper deserializedHeader;
  packet->RemoveHeader (deserializedHeader);

  NS_TEST_ASSERT_MSG_EQ (deserializedHeader.GetSenderId (), header.GetSenderId (),
                         "Deserialized sender ID should match");
  NS_TEST_ASSERT_MSG_EQ (deserializedHeader.GetTtl (), header.GetTtl (),
                         "Deserialized TTL should match");
  NS_TEST_ASSERT_MSG_EQ (deserializedHeader.GetClassId (), header.GetClassId (),
                         "Deserialized class ID should match");

  std::vector<uint32_t> deserializedPath = deserializedHeader.GetPath ();
  NS_TEST_ASSERT_MSG_EQ (deserializedPath.size (), path.size (),
                         "Deserialized path size should match");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief BLE Discovery Header Test Suite
 */
class BleDiscoveryHeaderTestSuite : public TestSuite
{
public:
  BleDiscoveryHeaderTestSuite ();
};

BleDiscoveryHeaderTestSuite::BleDiscoveryHeaderTestSuite ()
  : TestSuite ("ble-discovery-header", UNIT)
{
  AddTestCase (new BleDiscoveryHeaderTestCase, TestCase::QUICK);
}

static BleDiscoveryHeaderTestSuite bleMeshDiscoveryHeaderTestSuite;
