#include "ble-election.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BleElection");
NS_OBJECT_ENSURE_REGISTERED (BleElection);

TypeId
BleElection::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BleElection")
    .SetParent<Object> ()
    .SetGroupName ("BleMeshDiscovery")
    .AddConstructor<BleElection> ()
    .AddAttribute ("MinNeighborsForCandidacy",
                   "Minimum direct neighbors to become candidate",
                   UintegerValue (10),
                   MakeUintegerAccessor (&BleElection::m_state.min_neighbors_for_candidacy),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MinConnectionNoiseRatio",
                   "Minimum connection:noise ratio for candidacy",
                   DoubleValue (5.0),
                   MakeDoubleAccessor (&BleElection::m_state.min_connection_noise_ratio),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MinGeographicDistribution",
                   "Minimum geographic distribution score for candidacy",
                   DoubleValue (0.3),
                   MakeDoubleAccessor (&BleElection::m_state.min_geographic_distribution),
                   MakeDoubleChecker<double> (0.0, 1.0))
  ;
  return tid;
}

BleElection::BleElection ()
{
  NS_LOG_FUNCTION (this);
  ble_election_init (&m_state);
}

BleElection::~BleElection ()
{
  NS_LOG_FUNCTION (this);
}

void
BleElection::UpdateNeighbor (uint32_t nodeId, Vector location, int8_t rssi)
{
  NS_LOG_FUNCTION (this << nodeId << location << static_cast<int32_t> (rssi));

  
  ble_gps_location_t c_location = {location.x, location.y, location.z};

  
  uint32_t current_time_ms = static_cast<uint32_t> (Simulator::Now ().GetMilliSeconds ());

  
  ble_election_update_neighbor (&m_state, nodeId, &c_location, rssi, current_time_ms);

  NS_LOG_DEBUG ("Updated neighbor " << nodeId << " (RSSI=" << static_cast<int32_t> (rssi)
                << " dBm, total neighbors=" << m_state.neighbor_count << ")");
}

void
BleElection::AddRssiSample (int8_t rssi)
{
  NS_LOG_FUNCTION (this << static_cast<int32_t> (rssi));
  ble_election_add_rssi_sample (&m_state, rssi);
}

double
BleElection::CalculateCrowding () const
{
  double crowding = ble_election_calculate_crowding (&m_state);
  NS_LOG_DEBUG ("Crowding factor: " << crowding);
  return crowding;
}

uint32_t
BleElection::CountDirectConnections () const
{
  uint32_t count = ble_election_count_direct_connections (&m_state);
  NS_LOG_DEBUG ("Direct connections: " << count);
  return count;
}

double
BleElection::CalculateGeographicDistribution () const
{
  double distribution = ble_election_calculate_geographic_distribution (&m_state);
  NS_LOG_DEBUG ("Geographic distribution: " << distribution);
  return distribution;
}

void
BleElection::UpdateMetrics ()
{
  NS_LOG_FUNCTION (this);
  ble_election_update_metrics (&m_state);

  NS_LOG_DEBUG ("Metrics updated: direct=" << m_state.metrics.direct_connections
                << ", total=" << m_state.metrics.total_neighbors
                << ", crowding=" << m_state.metrics.crowding_factor
                << ", CN ratio=" << m_state.metrics.connection_noise_ratio
                << ", geo dist=" << m_state.metrics.geographic_distribution);
}

double
BleElection::CalculateCandidacyScore () const
{
  double score = ble_election_calculate_candidacy_score (&m_state);
  NS_LOG_DEBUG ("Candidacy score: " << score);
  return score;
}

bool
BleElection::ShouldBecomeCandidate ()
{
  NS_LOG_FUNCTION (this);

  bool should = ble_election_should_become_candidate (&m_state);

  if (should)
    {
      NS_LOG_INFO ("Node qualifies as clusterhead candidate "
                   << "(score=" << m_state.candidacy_score
                   << ", direct=" << m_state.metrics.direct_connections
                   << ", CN ratio=" << m_state.metrics.connection_noise_ratio << ")");
    }
  else
    {
      NS_LOG_DEBUG ("Node does not qualify as candidate "
                    << "(direct=" << m_state.metrics.direct_connections
                    << "/" << m_state.min_neighbors_for_candidacy
                    << ", CN ratio=" << m_state.metrics.connection_noise_ratio
                    << "/" << m_state.min_connection_noise_ratio << ")");
    }

  return should;
}

ConnectivityMetrics
BleElection::GetMetrics () const
{
  ConnectivityMetrics metrics;
  metrics.directConnections = m_state.metrics.direct_connections;
  metrics.totalNeighbors = m_state.metrics.total_neighbors;
  metrics.crowdingFactor = m_state.metrics.crowding_factor;
  metrics.connectionNoiseRatio = m_state.metrics.connection_noise_ratio;
  metrics.geographicDistribution = m_state.metrics.geographic_distribution;
  metrics.messagesForwarded = m_state.metrics.messages_forwarded;
  metrics.messagesReceived = m_state.metrics.messages_received;
  metrics.forwardingSuccessRate = m_state.metrics.forwarding_success_rate;

  return metrics;
}

std::vector<NeighborInfo>
BleElection::GetNeighbors () const
{
  std::vector<NeighborInfo> neighbors;

  for (uint32_t i = 0; i < m_state.neighbor_count; i++)
    {
      const ble_neighbor_info_t& c_neighbor = m_state.neighbors[i];

      NeighborInfo info;
      info.nodeId = c_neighbor.node_id;
      info.location = Vector (c_neighbor.location.x,
                               c_neighbor.location.y,
                               c_neighbor.location.z);
      info.rssi = c_neighbor.rssi;
      info.messageCount = c_neighbor.message_count;
      info.lastSeen = MilliSeconds (c_neighbor.last_seen_time_ms);
      info.isDirect = c_neighbor.is_direct;

      neighbors.push_back (info);
    }

  return neighbors;
}

bool
BleElection::GetNeighbor (uint32_t nodeId, NeighborInfo& info) const
{
  const ble_neighbor_info_t* c_neighbor = ble_election_get_neighbor (&m_state, nodeId);

  if (!c_neighbor)
    {
      return false;
    }

  info.nodeId = c_neighbor->node_id;
  info.location = Vector (c_neighbor->location.x,
                          c_neighbor->location.y,
                          c_neighbor->location.z);
  info.rssi = c_neighbor->rssi;
  info.messageCount = c_neighbor->message_count;
  info.lastSeen = MilliSeconds (c_neighbor->last_seen_time_ms);
  info.isDirect = c_neighbor->is_direct;

  return true;
}

uint32_t
BleElection::CleanOldNeighbors (Time timeout)
{
  NS_LOG_FUNCTION (this << timeout);

  uint32_t current_time_ms = static_cast<uint32_t> (Simulator::Now ().GetMilliSeconds ());
  uint32_t timeout_ms = static_cast<uint32_t> (timeout.GetMilliSeconds ());

  uint32_t removed = ble_election_clean_old_neighbors (&m_state, current_time_ms, timeout_ms);

  if (removed > 0)
    {
      NS_LOG_DEBUG ("Cleaned " << removed << " old neighbors");
    }

  return removed;
}

void
BleElection::SetThresholds (uint32_t minNeighbors, double minCnRatio, double minGeoDist)
{
  NS_LOG_FUNCTION (this << minNeighbors << minCnRatio << minGeoDist);
  ble_election_set_thresholds (&m_state, minNeighbors, minCnRatio, minGeoDist);
}

void
BleElection::SetDirectRssiThreshold (int8_t threshold)
{
  NS_LOG_FUNCTION (this << static_cast<int32_t> (threshold));
  m_state.direct_connection_rssi_threshold = threshold;
}

void
BleElection::SetScoreWeights (double directWeight,
                              double ratioWeight,
                              double geoWeight,
                              double forwardingWeight)
{
  NS_LOG_FUNCTION (this << directWeight << ratioWeight << geoWeight << forwardingWeight);
  ble_score_weights_t weights;
  weights.direct_weight = directWeight;
  weights.connection_noise_weight = ratioWeight;
  weights.geographic_weight = geoWeight;
  weights.forwarding_weight = forwardingWeight;
  ble_election_set_score_weights (&m_state, &weights);
}

void
BleElection::RecordMessageForwarded ()
{
  m_state.metrics.messages_forwarded++;
}

void
BleElection::RecordMessageReceived ()
{
  m_state.metrics.messages_received++;
}

bool
BleElection::IsCandidate () const
{
  return m_state.is_candidate;
}

double
BleElection::GetCandidacyScore () const
{
  return m_state.candidacy_score;
}

} 
