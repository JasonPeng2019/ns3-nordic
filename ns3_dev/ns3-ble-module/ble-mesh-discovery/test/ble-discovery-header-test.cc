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
#include "ns3/ble-election-header.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BleDiscoveryHeaderTest");

/**
 * \ingroup ble-mesh-discovery-test
 * \brief BLE Discovery Header Basic Test Case
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
  : TestCase ("BleDiscoveryHeader basic test case")
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
  NS_TEST_ASSERT_MSG_EQ (header.HasClusterheadFlag (), false,
                         "Default header should not be flagged as clusterhead");

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
  NS_TEST_ASSERT_MSG_EQ (header.HasClusterheadFlag (), true,
                         "Election message should raise clusterhead flag");
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
  NS_TEST_ASSERT_MSG_EQ (deserializedHeader.HasClusterheadFlag (), header.HasClusterheadFlag (),
                         "Clusterhead flag should survive serialization");

  std::vector<uint32_t> deserializedPath = deserializedHeader.GetPath ();
  NS_TEST_ASSERT_MSG_EQ (deserializedPath.size (), path.size (),
                         "Deserialized path size should match");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Comprehensive NS-3 Packet Integration Test
 */
class BleDiscoveryPacketIntegrationTestCase : public TestCase
{
public:
  BleDiscoveryPacketIntegrationTestCase ();
  virtual ~BleDiscoveryPacketIntegrationTestCase ();

private:
  virtual void DoRun (void);
};

BleDiscoveryPacketIntegrationTestCase::BleDiscoveryPacketIntegrationTestCase ()
  : TestCase ("BleDiscoveryHeader NS-3 packet integration")
{
}

BleDiscoveryPacketIntegrationTestCase::~BleDiscoveryPacketIntegrationTestCase ()
{
}

void
BleDiscoveryPacketIntegrationTestCase::DoRun (void)
{
  // Test: Multiple serialize/deserialize cycles
  BleDiscoveryHeaderWrapper original;
  original.SetSenderId (999);
  original.SetTtl (15);
  original.AddToPath (10);
  original.AddToPath (20);
  original.AddToPath (30);
  original.SetGpsLocation (Vector (1.0, 2.0, 3.0));

  for (int cycle = 0; cycle < 3; cycle++)
    {
      Ptr<Packet> pkt = Create<Packet> (100); // Add payload
      pkt->AddHeader (original);

      NS_TEST_ASSERT_MSG_EQ (pkt->GetSize (), original.GetSerializedSize () + 100,
                             "Packet size should include header and payload");

      BleDiscoveryHeaderWrapper copy;
      pkt->RemoveHeader (copy);

      NS_TEST_ASSERT_MSG_EQ (copy.GetSenderId (), original.GetSenderId (),
                             "Sender ID should survive multiple cycles");
      NS_TEST_ASSERT_MSG_EQ (copy.GetPath ().size (), original.GetPath ().size (),
                             "Path should survive multiple cycles");

      original = copy; // Use for next cycle
    }

  // Test: Packet copy constructor
  Ptr<Packet> pkt1 = Create<Packet> ();
  BleDiscoveryHeaderWrapper hdr1;
  hdr1.SetSenderId (777);
  hdr1.SetTtl (5);
  pkt1->AddHeader (hdr1);

  Ptr<Packet> pkt2 = pkt1->Copy ();
  BleDiscoveryHeaderWrapper hdr2;
  pkt2->RemoveHeader (hdr2);

  NS_TEST_ASSERT_MSG_EQ (hdr2.GetSenderId (), 777, "Packet copy should preserve header");
  NS_TEST_ASSERT_MSG_EQ (hdr2.GetTtl (), 5, "Packet copy should preserve TTL");

  // Test: Packet fragmentation doesn't corrupt header
  Ptr<Packet> largePkt = Create<Packet> (2000);
  BleDiscoveryHeaderWrapper largeHdr;
  largeHdr.SetSenderId (12345);
  for (uint32_t i = 0; i < 10; i++)
    {
      largeHdr.AddToPath (i * 100);
    }
  largePkt->AddHeader (largeHdr);

  BleDiscoveryHeaderWrapper retrievedHdr;
  Ptr<Packet> fragment = largePkt->Copy ();
  fragment->RemoveHeader (retrievedHdr);

  NS_TEST_ASSERT_MSG_EQ (retrievedHdr.GetSenderId (), 12345,
                         "Large packet should preserve sender ID");
  NS_TEST_ASSERT_MSG_EQ (retrievedHdr.GetPath ().size (), 10,
                         "Large packet should preserve full path");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Election Message Comprehensive Test
 */
class BleDiscoveryElectionTestCase : public TestCase
{
public:
  BleDiscoveryElectionTestCase ();
  virtual ~BleDiscoveryElectionTestCase ();

private:
  virtual void DoRun (void);
};

BleDiscoveryElectionTestCase::BleDiscoveryElectionTestCase ()
  : TestCase ("BleDiscoveryHeader election message test")
{
}

BleDiscoveryElectionTestCase::~BleDiscoveryElectionTestCase ()
{
}

void
BleDiscoveryElectionTestCase::DoRun (void)
{
  // Test: Convert discovery to election
  BleDiscoveryHeaderWrapper msg;
  msg.SetSenderId (100);
  msg.SetTtl (8);
  msg.AddToPath (1);
  msg.AddToPath (2);
  msg.SetGpsLocation (Vector (10.0, 20.0, 30.0));

  NS_TEST_ASSERT_MSG_EQ (msg.IsElectionMessage (), false,
                         "Should start as discovery message");

  msg.SetAsElectionMessage ();

  NS_TEST_ASSERT_MSG_EQ (msg.IsElectionMessage (), true,
                         "Should be election message after conversion");
  NS_TEST_ASSERT_MSG_EQ (msg.HasClusterheadFlag (), true,
                         "Clusterhead flag should be set for election");
  NS_TEST_ASSERT_MSG_EQ (msg.GetSenderId (), 100,
                         "Base fields should be preserved during conversion");
  NS_TEST_ASSERT_MSG_EQ (msg.GetPath ().size (), 2,
                         "Path should be preserved during conversion");

  // Test: Election-specific fields
  msg.SetClassId (7);
  msg.SetPdsf (200);
  msg.SetScore (0.95);
  msg.SetHash (0xABCDEF);

  NS_TEST_ASSERT_MSG_EQ (msg.GetClassId (), 7, "Class ID should be set");
  NS_TEST_ASSERT_MSG_EQ (msg.GetPdsf (), 200, "PDSF should be set");
  NS_TEST_ASSERT_MSG_EQ (msg.GetScore (), 0.95, "Score should be set");
  NS_TEST_ASSERT_MSG_EQ (msg.GetHash (), (uint32_t)0xABCDEF, "Hash should be set");

  msg.ResetPdsfHistory ();
  uint32_t pdsf = msg.UpdatePdsf (12, 0);
  NS_TEST_ASSERT_MSG_EQ (pdsf, 12, "First hop PDSF should equal direct connections");
  pdsf = msg.UpdatePdsf (9, 4); // contributes 5 new neighbors
  NS_TEST_ASSERT_MSG_EQ (pdsf, 72, "PDSF should accumulate ΣΠ contributions");

  std::vector<uint32_t> hopHistory = msg.GetPdsfHopHistory ();
  NS_TEST_ASSERT_MSG_EQ (hopHistory.size (), 2, "Should track two hop entries");
  NS_TEST_ASSERT_MSG_EQ (hopHistory[0], 12, "First hop history incorrect");
  NS_TEST_ASSERT_MSG_EQ (hopHistory[1], 5, "Second hop should record deduplicated count");

  // Test: Election serialization size
  uint32_t electionSize = msg.GetSerializedSize ();
  NS_TEST_ASSERT_MSG_GT (electionSize, 50, "Election packet should be substantial size");

  // Test: Election round-trip serialization
  Ptr<Packet> pkt = Create<Packet> ();
  pkt->AddHeader (msg);

  BleDiscoveryHeaderWrapper received;
  pkt->RemoveHeader (received);

  NS_TEST_ASSERT_MSG_EQ (received.IsElectionMessage (), true,
                         "Deserialized message should be election type");
  NS_TEST_ASSERT_MSG_EQ (received.HasClusterheadFlag (), true,
                         "Clusterhead flag should be preserved");
  NS_TEST_ASSERT_MSG_EQ (received.GetClassId (), 7,
                         "Deserialized class ID should match");
  NS_TEST_ASSERT_MSG_EQ (received.GetPdsf (), 72,
                         "Deserialized PDSF should reflect hop history updates");
  NS_TEST_ASSERT_MSG_EQ (received.GetScore (), 0.95,
                         "Deserialized score should match");
  NS_TEST_ASSERT_MSG_EQ (received.GetHash (), (uint32_t)0xABCDEF,
                         "Deserialized hash should match");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief GPS Unavailable Handling Test
 */
class BleDiscoveryGpsTestCase : public TestCase
{
public:
  BleDiscoveryGpsTestCase ();
  virtual ~BleDiscoveryGpsTestCase ();

private:
  virtual void DoRun (void);
};

BleDiscoveryGpsTestCase::BleDiscoveryGpsTestCase ()
  : TestCase ("BleDiscoveryHeader GPS unavailable test")
{
}

BleDiscoveryGpsTestCase::~BleDiscoveryGpsTestCase ()
{
}

void
BleDiscoveryGpsTestCase::DoRun (void)
{
  // Test: Message without GPS should have smaller size
  BleDiscoveryHeaderWrapper noGps;
  noGps.SetSenderId (1);
  noGps.SetTtl (10);
  noGps.AddToPath (1);

  uint32_t sizeNoGps = noGps.GetSerializedSize ();

  BleDiscoveryHeaderWrapper withGps;
  withGps.SetSenderId (1);
  withGps.SetTtl (10);
  withGps.AddToPath (1);
  withGps.SetGpsLocation (Vector (1.0, 2.0, 3.0));

  uint32_t sizeWithGps = withGps.GetSerializedSize ();

  NS_TEST_ASSERT_MSG_EQ (sizeWithGps, sizeNoGps + 24,
                         "GPS adds 24 bytes (3 doubles)");

  // Test: Serialization without GPS
  Ptr<Packet> pkt = Create<Packet> ();
  pkt->AddHeader (noGps);

  BleDiscoveryHeaderWrapper received;
  pkt->RemoveHeader (received);

  NS_TEST_ASSERT_MSG_EQ (received.IsGpsAvailable (), false,
                         "GPS should not be available");

  // Test: Can set GPS availability independently
  BleDiscoveryHeaderWrapper manual;
  manual.SetGpsLocation (Vector (5.0, 6.0, 7.0));
  NS_TEST_ASSERT_MSG_EQ (manual.IsGpsAvailable (), true,
                         "SetGpsLocation should enable GPS");

  manual.SetGpsAvailable (false);
  NS_TEST_ASSERT_MSG_EQ (manual.IsGpsAvailable (), false,
                         "Should be able to disable GPS");

  // GPS coordinates should still be retrievable even if marked unavailable
  Vector loc = manual.GetGpsLocation ();
  NS_TEST_ASSERT_MSG_EQ (loc.x, 5.0, "GPS coordinates preserved");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief TypeId and Print Test
 */
class BleDiscoveryTypeIdTestCase : public TestCase
{
public:
  BleDiscoveryTypeIdTestCase ();
  virtual ~BleDiscoveryTypeIdTestCase ();

private:
  virtual void DoRun (void);
};

BleDiscoveryTypeIdTestCase::BleDiscoveryTypeIdTestCase ()
  : TestCase ("BleDiscoveryHeader TypeId and Print test")
{
}

BleDiscoveryTypeIdTestCase::~BleDiscoveryTypeIdTestCase ()
{
}

void
BleDiscoveryTypeIdTestCase::DoRun (void)
{
  // Test: TypeId
  BleDiscoveryHeaderWrapper hdr;
  TypeId tid = hdr.GetInstanceTypeId ();
  NS_TEST_ASSERT_MSG_EQ (tid.GetName (), "ns3::BleDiscoveryHeaderWrapper",
                         "TypeId name should match");

  // Test: Print output (basic check it doesn't crash)
  std::ostringstream oss;
  hdr.SetSenderId (42);
  hdr.SetTtl (10);
  hdr.AddToPath (1);
  hdr.AddToPath (2);
  hdr.Print (oss);

  std::string output = oss.str ();
  NS_TEST_ASSERT_MSG_NE (output.size (), 0, "Print should produce output");
  NS_TEST_ASSERT_MSG_NE (output.find ("42"), std::string::npos,
                         "Print should include sender ID");

  // Test: Print election message
  BleDiscoveryHeaderWrapper election;
  election.SetAsElectionMessage ();
  election.SetClassId (5);
  election.SetPdsf (100);

  std::ostringstream oss2;
  election.Print (oss2);
  std::string output2 = oss2.str ();

  NS_TEST_ASSERT_MSG_NE (output2.find ("ELECTION"), std::string::npos,
                         "Print should indicate election message");
NS_TEST_ASSERT_MSG_NE (output2.find ("ClassID"), std::string::npos,
                         "Print should include election fields");
}

/**
 * \ingroup ble-mesh-discovery-test
 * \brief Dedicated BleElectionHeader class test
 */
class BleElectionHeaderWrapperTestCase : public TestCase
{
public:
  BleElectionHeaderWrapperTestCase ();
  virtual ~BleElectionHeaderWrapperTestCase ();

private:
  virtual void DoRun (void);
};

BleElectionHeaderWrapperTestCase::BleElectionHeaderWrapperTestCase ()
  : TestCase ("BleElectionHeader specialized class test")
{
}

BleElectionHeaderWrapperTestCase::~BleElectionHeaderWrapperTestCase () = default;

void
BleElectionHeaderWrapperTestCase::DoRun (void)
{
  BleElectionHeader header;
  NS_TEST_ASSERT_MSG_EQ (header.IsElectionMessage (), true,
                         "BleElectionHeader should always represent election packets");
  NS_TEST_ASSERT_MSG_EQ (header.HasClusterheadFlag (), true,
                         "Clusterhead flag should be set by default");

  header.SetSenderId (555);
  header.SetClassId (9);
  header.SetPdsf (75);
  header.SetScore (0.77);
  header.SetHash (0x12345678);
  header.AddToPath (42);
  header.AddToPath (77);

  Ptr<Packet> pkt = Create<Packet> ();
  pkt->AddHeader (header);

  BleElectionHeader decoded;
  pkt->RemoveHeader (decoded);

  NS_TEST_ASSERT_MSG_EQ (decoded.IsElectionMessage (), true,
                         "Decoded BleElectionHeader should remain election");
  NS_TEST_ASSERT_MSG_EQ (decoded.GetClassId (), 9,
                         "Class ID should survive serialization");
  NS_TEST_ASSERT_MSG_EQ (decoded.HasClusterheadFlag (), true,
                         "Clusterhead flag must remain set");
  NS_TEST_ASSERT_MSG_EQ (decoded.GetPath ().size (), 2,
                         "Path should be preserved");
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
  AddTestCase (new BleDiscoveryPacketIntegrationTestCase, TestCase::QUICK);
  AddTestCase (new BleDiscoveryElectionTestCase, TestCase::QUICK);
  AddTestCase (new BleDiscoveryGpsTestCase, TestCase::QUICK);
  AddTestCase (new BleDiscoveryTypeIdTestCase, TestCase::QUICK);
  AddTestCase (new BleElectionHeaderWrapperTestCase, TestCase::QUICK);
}

static BleDiscoveryHeaderTestSuite bleMeshDiscoveryHeaderTestSuite;
