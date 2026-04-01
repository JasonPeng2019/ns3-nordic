/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ble-election-header.h"
#include "ns3/log.h"
#include "ns3/assert.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BleElectionHeader");
NS_OBJECT_ENSURE_REGISTERED (BleElectionHeader);

BleElectionHeader::BleElectionHeader ()
{
  SetAsElectionMessage ();
}

BleElectionHeader::~BleElectionHeader () = default;

TypeId
BleElectionHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BleElectionHeader")
    .SetParent<BleDiscoveryHeaderWrapper> ()
    .SetGroupName ("BleMeshDiscovery")
    .AddConstructor<BleElectionHeader> ();
  return tid;
}

TypeId
BleElectionHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
BleElectionHeader::GetSerializedSize (void) const
{
  NS_ABORT_MSG_IF (!IsElectionMessage (),
                   "BleElectionHeader can only serialize election packets");
  return BleDiscoveryHeaderWrapper::GetSerializedSize ();
}

void
BleElectionHeader::Serialize (Buffer::Iterator start) const
{
  NS_ABORT_MSG_IF (!IsElectionMessage (),
                   "BleElectionHeader requires election message flag");
  BleDiscoveryHeaderWrapper::Serialize (start);
}

uint32_t
BleElectionHeader::Deserialize (Buffer::Iterator start)
{
  uint32_t bytes = BleDiscoveryHeaderWrapper::Deserialize (start);
  NS_ABORT_MSG_IF (!IsElectionMessage (),
                   "BleElectionHeader expected election announcement");
  return bytes;
}

} // namespace ns3
