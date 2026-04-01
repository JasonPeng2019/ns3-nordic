/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ble-mesh-metrics-collector.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (BleMeshMetricsCollector);

TypeId
BleMeshMetricsCollector::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BleMeshMetricsCollector")
    .SetParent<Object> ()
    .SetGroupName ("BleMeshDiscovery")
    .AddConstructor<BleMeshMetricsCollector> ();
  return tid;
}

BleMeshMetricsCollector::BleMeshMetricsCollector ()
  : m_outputPrefix ("ble_mesh_metrics")
{
}

BleMeshMetricsCollector::~BleMeshMetricsCollector () = default;

void
BleMeshMetricsCollector::SetOutputPrefix (const std::string &prefix)
{
  m_outputPrefix = prefix;
}

std::string
BleMeshMetricsCollector::GetOutputPrefix () const
{
  return m_outputPrefix;
}

} // namespace ns3
