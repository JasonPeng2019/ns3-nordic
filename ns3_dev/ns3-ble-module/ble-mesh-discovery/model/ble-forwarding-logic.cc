/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025
 *
 * Author: Benjamin Huh <buh07@github>
 *
 * C++ Wrapper Implementation - Thin layer over C protocol core
 */

#include "ble-forwarding-logic.h"
#include "ns3/log.h"
#include "ns3/double.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BleForwardingLogic");

NS_OBJECT_ENSURE_REGISTERED (BleForwardingLogic);

TypeId
BleForwardingLogic::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BleForwardingLogic")
    .SetParent<Object> ()
    .SetGroupName ("BleMeshDiscovery")
    .AddConstructor<BleForwardingLogic> ()
    .AddAttribute ("ProximityThreshold",
                   "Minimum GPS distance for message forwarding (meters)",
                   DoubleValue (10.0),
                   MakeDoubleAccessor (&BleForwardingLogic::m_proximityThreshold),
                   MakeDoubleChecker<double> (0.0))
  ;
  return tid;
}

BleForwardingLogic::BleForwardingLogic ()
  : m_proximityThreshold (10.0)
{
  NS_LOG_FUNCTION (this);
  m_random = CreateObject<UniformRandomVariable> ();
  m_random->SetAttribute ("Min", DoubleValue (0.0));
  m_random->SetAttribute ("Max", DoubleValue (1.0));
}

BleForwardingLogic::~BleForwardingLogic ()
{
  NS_LOG_FUNCTION (this);
}

double
BleForwardingLogic::CalculateCrowdingFactor (const std::vector<int8_t>& rssiSamples) const
{
  NS_LOG_FUNCTION (this << rssiSamples.size ());

  if (rssiSamples.empty ())
    {
      return 0.0;
    }

  // Call C core function
  double crowding = ble_forwarding_calculate_crowding_factor (rssiSamples.data (),
                                                                rssiSamples.size ());

  NS_LOG_DEBUG ("Calculated crowding factor: " << crowding
                << " from " << rssiSamples.size () << " RSSI samples");

  return crowding;
}

bool
BleForwardingLogic::ShouldForwardCrowding (double crowdingFactor)
{
  NS_LOG_FUNCTION (this << crowdingFactor);

  // Generate random value
  double randomValue = m_random->GetValue ();

  // Call C core function
  bool shouldForward = ble_forwarding_should_forward_crowding (crowdingFactor, randomValue);

  NS_LOG_DEBUG ("Crowding check: factor=" << crowdingFactor
                << ", random=" << randomValue
                << ", forward=" << (shouldForward ? "YES" : "NO"));

  return shouldForward;
}

double
BleForwardingLogic::CalculateDistance (Vector loc1, Vector loc2) const
{
  NS_LOG_FUNCTION (this << loc1 << loc2);

  // Convert NS-3 Vector to C GPS locations
  ble_gps_location_t c_loc1 = {loc1.x, loc1.y, loc1.z};
  ble_gps_location_t c_loc2 = {loc2.x, loc2.y, loc2.z};

  // Call C core function
  double distance = ble_forwarding_calculate_distance (&c_loc1, &c_loc2);

  NS_LOG_DEBUG ("Distance between locations: " << distance << " meters");

  return distance;
}

bool
BleForwardingLogic::ShouldForwardProximity (Vector currentLocation,
                                             Vector lastHopLocation,
                                             double proximityThreshold) const
{
  NS_LOG_FUNCTION (this << currentLocation << lastHopLocation << proximityThreshold);

  // Convert NS-3 Vectors to C GPS locations
  ble_gps_location_t c_current = {currentLocation.x, currentLocation.y, currentLocation.z};
  ble_gps_location_t c_lasthop = {lastHopLocation.x, lastHopLocation.y, lastHopLocation.z};

  // Call C core function
  bool shouldForward = ble_forwarding_should_forward_proximity (&c_current,
                                                                 &c_lasthop,
                                                                 proximityThreshold);

  double distance = ble_forwarding_calculate_distance (&c_current, &c_lasthop);

  NS_LOG_DEBUG ("Proximity check: distance=" << distance
                << ", threshold=" << proximityThreshold
                << ", forward=" << (shouldForward ? "YES" : "NO"));

  return shouldForward;
}

bool
BleForwardingLogic::ShouldForward (const BleDiscoveryHeaderWrapper& header,
                                    Vector currentLocation,
                                    double crowdingFactor,
                                    double proximityThreshold)
{
  NS_LOG_FUNCTION (this << crowdingFactor << proximityThreshold);

  // Get C packet from wrapper
  const ble_discovery_packet_t& c_packet = header.GetCPacket ();

  // Convert current location to C GPS
  ble_gps_location_t c_current = {currentLocation.x, currentLocation.y, currentLocation.z};

  // Generate random value
  double randomValue = m_random->GetValue ();

  // Call C core function (performs all 3 checks)
  bool shouldForward = ble_forwarding_should_forward (&c_packet,
                                                       &c_current,
                                                       crowdingFactor,
                                                       proximityThreshold,
                                                       randomValue);

  NS_LOG_DEBUG ("Forwarding decision for sender=" << c_packet.sender_id
                << ", TTL=" << static_cast<uint32_t> (c_packet.ttl)
                << ", crowding=" << crowdingFactor
                << ", random=" << randomValue
                << " -> " << (shouldForward ? "FORWARD" : "DROP"));

  return shouldForward;
}

uint8_t
BleForwardingLogic::CalculatePriority (const BleDiscoveryHeaderWrapper& header) const
{
  NS_LOG_FUNCTION (this);

  uint8_t ttl = header.GetTtl ();

  // Call C core function
  uint8_t priority = ble_forwarding_calculate_priority (ttl);

  return priority;
}

void
BleForwardingLogic::SetRandomStream (Ptr<UniformRandomVariable> stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_random = stream;
}

void
BleForwardingLogic::SetProximityThreshold (double threshold)
{
  NS_LOG_FUNCTION (this << threshold);
  m_proximityThreshold = threshold;
}

double
BleForwardingLogic::GetProximityThreshold () const
{
  return m_proximityThreshold;
}

} // namespace ns3
