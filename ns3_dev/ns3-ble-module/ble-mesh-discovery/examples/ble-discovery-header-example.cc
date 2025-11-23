#include "ns3/core-module.h"
#include "ns3/ble-discovery-header-wrapper.h"
#include "ns3/packet.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BleDiscoveryHeaderExample");

int
main (int argc, char *argv[])
{
  
  LogComponentEnable ("BleDiscoveryHeaderExample", LOG_LEVEL_INFO);

  NS_LOG_INFO ("BLE Discovery Header Example");
  NS_LOG_INFO ("===========================");

  
  NS_LOG_INFO ("\n--- Example 1: Basic Discovery Message ---");
  BleDiscoveryHeaderWrapper discoveryMsg;
  
  discoveryMsg.SetSenderId (101);
  discoveryMsg.SetTtl (10);

  
  discoveryMsg.AddToPath (101);
  discoveryMsg.AddToPath (102);
  discoveryMsg.AddToPath (103);

  
  Vector gpsLoc (37.7749, -122.4194, 50.0); 
  discoveryMsg.SetGpsLocation (gpsLoc);
  discoveryMsg.SetGpsAvailable (true);

  std::cout << "Discovery Message Created:" << std::endl;
  std::cout << "  Sender ID: " << discoveryMsg.GetSenderId () << std::endl;
  std::cout << "  TTL: " << (int)discoveryMsg.GetTtl () << std::endl;
  std::cout << "  Path: ";
  std::vector<uint32_t> path = discoveryMsg.GetPath ();
  for (size_t i = 0; i < path.size (); ++i)
    {
      std::cout << path[i];
      if (i < path.size () - 1)
        std::cout << " -> ";
    }
  std::cout << std::endl;
  std::cout << "  GPS: (" << discoveryMsg.GetGpsLocation ().x << ", "
            << discoveryMsg.GetGpsLocation ().y << ", "
            << discoveryMsg.GetGpsLocation ().z << ")" << std::endl;

  
  NS_LOG_INFO ("\n--- Example 2: Serialization/Deserialization ---");
  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (discoveryMsg);

  uint32_t packetSize = packet->GetSize ();
  std::cout << "Serialized packet size: " << packetSize << " bytes" << std::endl;

  BleDiscoveryHeaderWrapper receivedMsg;
  packet->RemoveHeader (receivedMsg);

  std::cout << "Received message:" << std::endl;
  std::cout << "  Sender ID: " << receivedMsg.GetSenderId () << std::endl;
  std::cout << "  TTL: " << (int)receivedMsg.GetTtl () << std::endl;

  
  NS_LOG_INFO ("\n--- Example 3: Election Announcement ---");
  BleDiscoveryHeaderWrapper electionMsg;
  electionMsg.SetAsElectionMessage ();
  electionMsg.SetSenderId (201);
  electionMsg.SetTtl (8);

  
  electionMsg.SetClassId (1);
  electionMsg.SetPdsf (120); 
  electionMsg.SetScore (0.87); 
  electionMsg.SetHash (987654321);

  
  electionMsg.AddToPath (201);

  
  Vector clusterheadGps (37.7750, -122.4195, 55.0);
  electionMsg.SetGpsLocation (clusterheadGps);
  electionMsg.SetGpsAvailable (true);

  std::cout << "Election Announcement Created:" << std::endl;
  std::cout << "  Sender ID: " << electionMsg.GetSenderId () << std::endl;
  std::cout << "  Class ID: " << electionMsg.GetClassId () << std::endl;
  std::cout << "  PDSF: " << electionMsg.GetPdsf () << " devices" << std::endl;
  std::cout << "  Score: " << electionMsg.GetScore () << std::endl;
  std::cout << "  Hash: " << electionMsg.GetHash () << std::endl;

  Ptr<Packet> electionPacket = Create<Packet> ();
  electionPacket->AddHeader (electionMsg);
  std::cout << "Serialized election packet size: " << electionPacket->GetSize ()
            << " bytes" << std::endl;

  
  NS_LOG_INFO ("\n--- Example 4: TTL Operations ---");
  BleDiscoveryHeaderWrapper ttlMsg;
  ttlMsg.SetTtl (3);

  std::cout << "Initial TTL: " << (int)ttlMsg.GetTtl () << std::endl;
  for (int i = 1; i <= 4; ++i)
    {
      bool result = ttlMsg.DecrementTtl ();
      std::cout << "Hop " << i << ": TTL = " << (int)ttlMsg.GetTtl ()
                << ", Continue? " << (result ? "Yes" : "No") << std::endl;
    }

  
  NS_LOG_INFO ("\n--- Example 5: Loop Detection ---");
  BleDiscoveryHeaderWrapper loopMsg;
  loopMsg.AddToPath (1);
  loopMsg.AddToPath (2);
  loopMsg.AddToPath (3);
  loopMsg.AddToPath (4);

  std::cout << "Path: 1 -> 2 -> 3 -> 4" << std::endl;
  std::cout << "Is node 3 in path? " << (loopMsg.IsInPath (3) ? "Yes" : "No") << std::endl;
  std::cout << "Is node 5 in path? " << (loopMsg.IsInPath (5) ? "Yes" : "No") << std::endl;

  NS_LOG_INFO ("\n=== Example Complete ===");

  return 0;
}
