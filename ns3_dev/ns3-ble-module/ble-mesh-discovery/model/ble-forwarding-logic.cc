#include "ble-forwarding-logic.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include <algorithm>
#include <limits>

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
    .AddAttribute ("DefaultDirectNeighbors",
                   "Neighbor count used when legacy overloads are called without an explicit value.",
                   UintegerValue (20),
                   MakeUintegerAccessor (&BleForwardingLogic::m_defaultNeighbors),
                   MakeUintegerChecker<uint32_t> (1))
  ;
  return tid;
}

BleForwardingLogic::BleForwardingLogic ()
  : m_proximityThreshold (10.0)
{
  NS_LOG_FUNCTION (this);
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

  
  double crowding = ble_forwarding_calculate_crowding_factor (rssiSamples.data (),
                                                                rssiSamples.size ());

  NS_LOG_DEBUG ("Calculated crowding factor: " << crowding
                << " from " << rssiSamples.size () << " RSSI samples");

  return crowding;
}

bool
BleForwardingLogic::ShouldForwardCrowding (double crowdingFactor, uint32_t directNeighbors)
{
  NS_LOG_FUNCTION (this << crowdingFactor << directNeighbors);

  if (m_randomStream)
    {
      double clamped = std::max (0.0, std::min (crowdingFactor, 1.0));
      uint32_t neighborCount = (directNeighbors == 0) ? 1 : directNeighbors;
      double baseProbability = 2.0 / static_cast<double> (neighborCount);
      if (baseProbability > 1.0)
        {
          baseProbability = 1.0;
        }

      const double crowdingLow = 0.1;
      const double crowdingHigh = 0.9;
      double forwardProbability;
      if (clamped <= crowdingLow)
        {
          forwardProbability = 1.0;
        }
      else if (clamped >= crowdingHigh)
        {
          forwardProbability = baseProbability;
        }
      else
        {
          double t = (clamped - crowdingLow) / (crowdingHigh - crowdingLow);
          forwardProbability = 1.0 + t * (baseProbability - 1.0);
        }

      double randomValue = m_randomStream->GetValue ();
      bool shouldForward = randomValue < forwardProbability;

      NS_LOG_DEBUG ("Crowding check (ns-3 RNG): crowding=" << clamped
                    << ", neighbors=" << neighborCount
                    << ", probability=" << forwardProbability
                    << ", rand=" << randomValue
                    << " -> " << (shouldForward ? "FORWARD" : "DROP"));
      return shouldForward;
    }

  bool shouldForward = ble_forwarding_should_forward_crowding (crowdingFactor, directNeighbors);

  NS_LOG_DEBUG ("Crowding check: factor=" << crowdingFactor
                << ", neighbors=" << directNeighbors
                << " -> " << (shouldForward ? "FORWARD" : "DROP"));

  return shouldForward;
}

bool
BleForwardingLogic::ShouldForwardCrowding (double crowdingFactor)
{
  return ShouldForwardCrowding (crowdingFactor, m_defaultNeighbors);
}

double
BleForwardingLogic::CalculateDistance (Vector loc1, Vector loc2) const
{
  NS_LOG_FUNCTION (this << loc1 << loc2);

  
  ble_gps_location_t c_loc1 = {loc1.x, loc1.y, loc1.z};
  ble_gps_location_t c_loc2 = {loc2.x, loc2.y, loc2.z};

  
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

  
  ble_gps_location_t c_current = {currentLocation.x, currentLocation.y, currentLocation.z};
  ble_gps_location_t c_lasthop = {lastHopLocation.x, lastHopLocation.y, lastHopLocation.z};

  
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
                                    double proximityThreshold,
                                    uint32_t directNeighbors)
{
  NS_LOG_FUNCTION (this << crowdingFactor << proximityThreshold << directNeighbors);

  
  const ble_discovery_packet_t& c_packet = header.GetCPacket ();

  
  ble_gps_location_t c_current = {currentLocation.x, currentLocation.y, currentLocation.z};

  
  bool shouldForward = ble_forwarding_should_forward (&c_packet,
                                                       &c_current,
                                                       crowdingFactor,
                                                       proximityThreshold,
                                                       directNeighbors);

  NS_LOG_DEBUG ("Forwarding decision for sender=" << c_packet.sender_id
                << ", TTL=" << static_cast<uint32_t> (c_packet.ttl)
                << ", crowding=" << crowdingFactor
                << ", neighbors=" << directNeighbors
                << " -> " << (shouldForward ? "FORWARD" : "DROP"));

  return shouldForward;
}

bool
BleForwardingLogic::ShouldForward (const BleDiscoveryHeaderWrapper& header,
                                    Vector currentLocation,
                                    double crowdingFactor,
                                    double proximityThreshold)
{
  return ShouldForward (header,
                        currentLocation,
                        crowdingFactor,
                        proximityThreshold,
                        m_defaultNeighbors);
}

uint8_t
BleForwardingLogic::CalculatePriority (const BleDiscoveryHeaderWrapper& header) const
{
  NS_LOG_FUNCTION (this);

  uint8_t ttl = header.GetTtl ();

  
  uint8_t priority = ble_forwarding_calculate_priority (ttl);

  return priority;
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

void
BleForwardingLogic::SeedRandom (uint32_t seed)
{
  ble_forwarding_set_random_seed (seed);
}

void
BleForwardingLogic::SetRandomStream (Ptr<RandomVariableStream> stream)
{
  m_randomStream = stream;

  if (m_randomStream)
    {
      
      uint32_t seed = m_randomStream->GetInteger ();
      if (seed == 0)
        {
          seed = 1;
        }
      ble_forwarding_set_random_seed (seed);
    }
}

} 
