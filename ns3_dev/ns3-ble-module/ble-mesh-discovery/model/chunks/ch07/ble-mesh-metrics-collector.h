/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef BLE_MESH_METRICS_COLLECTOR_H
#define BLE_MESH_METRICS_COLLECTOR_H

#include "ns3/object.h"
#include <string>

namespace ns3 {

class BleMeshMetricsCollector : public Object
{
public:
  static TypeId GetTypeId (void);
  BleMeshMetricsCollector ();
  ~BleMeshMetricsCollector () override;

  void SetOutputPrefix (const std::string &prefix);
  std::string GetOutputPrefix () const;

private:
  std::string m_outputPrefix;
};

} // namespace ns3

#endif /* BLE_MESH_METRICS_COLLECTOR_H */
