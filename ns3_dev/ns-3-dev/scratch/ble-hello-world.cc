/* ble-hello-world.cc
 *
 * A standalone, minimal BLE broadcast simulation using the Stijn BLE module.
 *
 * Topology:
 *   5 nodes placed in a line (0m, 5m, 10m, 15m, 20m).
 *   Every node broadcasts a small packet periodically.
 *   Every other node that receives it prints a human-readable log line.
 *
 * Run:
 *   python3 waf --run ble-hello-world
 *
 * To see verbose NS-3 logs add: --verbose
 */

#include <ns3/core-module.h>
#include <ns3/ble-module.h>
#include <ns3/propagation-loss-model.h>
#include <ns3/propagation-delay-model.h>
#include <ns3/simulator.h>
#include <ns3/single-model-spectrum-channel.h>
#include <ns3/packet.h>
#include <ns3/rng-seed-manager.h>
#include <ns3/spectrum-module.h>
#include <ns3/mobility-module.h>
#include <ns3/spectrum-value.h>
#include <ns3/isotropic-antenna-model.h>
#include <ns3/trace-helper.h>
#include <ns3/drop-tail-queue.h>
#include "ns3/network-module.h"
#include <iostream>
#include <iomanip>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BleHelloWorld");

// ---------------------------------------------------------
// Per-node counters
// ---------------------------------------------------------
struct NodeStats {
    std::string  addr;
    uint32_t     transmitted     = 0;
    uint32_t     broadcastRx     = 0;
    uint32_t     rxError         = 0;
    uint32_t     txWindowSkipped = 0;
};

std::map<uint32_t, NodeStats> g_stats;  // addr index (1-based) -> stats

// ---------------------------------------------------------
// Helpers
// ---------------------------------------------------------
static uint32_t srcAddrIndex(const Ptr<const Packet>& pkt)
{
    Ptr<Packet> copy = pkt->Copy();
    BleMacHeader hdr;
    copy->RemoveHeader(hdr);
    uint8_t buf[2];
    hdr.GetSrcAddr().CopyTo(buf);
    return buf[1];
}

static uint32_t dstAddrIndex(const Ptr<const Packet>& pkt)
{
    Ptr<Packet> copy = pkt->Copy();
    BleMacHeader hdr;
    copy->RemoveHeader(hdr);
    uint8_t buf[2];
    hdr.GetDestAddr().CopyTo(buf);
    return buf[1];
}

// ---------------------------------------------------------
// Trace callbacks
// ---------------------------------------------------------
void OnTx(const Ptr<const Packet> pkt)
{
    uint32_t idx = srcAddrIndex(pkt);
    g_stats[idx].transmitted++;
    std::cout << std::fixed << std::setprecision(3)
              << "[" << Simulator::Now().GetSeconds() << "s] "
              << "Node " << idx << " SENT  (total sent="
              << g_stats[idx].transmitted << ")\n";
}

void OnBroadcastRx(const Ptr<const Packet> pkt,
                   const Ptr<const BleNetDevice> receiver)
{
    Ptr<Packet> copy = pkt->Copy();
    BleMacHeader hdr;
    copy->RemoveHeader(hdr);

    uint8_t buf[2];
    receiver->GetAddress16().CopyTo(buf);
    uint32_t rxIdx = buf[1];

    buf[0] = 0; buf[1] = 0;
    hdr.GetSrcAddr().CopyTo(buf);
    uint32_t txIdx = buf[1];

    g_stats[rxIdx].broadcastRx++;
    std::cout << std::fixed << std::setprecision(3)
              << "[" << Simulator::Now().GetSeconds() << "s] "
              << "Node " << rxIdx << " RECV from Node " << txIdx
              << "  (total rx=" << g_stats[rxIdx].broadcastRx << ")\n";
}

void OnRxError(const Ptr<const Packet> pkt)
{
    uint32_t idx = dstAddrIndex(pkt);
    g_stats[idx].rxError++;
}

void OnTxWindowSkipped(const Ptr<const BleNetDevice> nd)
{
    uint8_t buf[2];
    nd->GetAddress16().CopyTo(buf);
    uint32_t idx = buf[1];
    g_stats[idx].txWindowSkipped++;
}

// ---------------------------------------------------------
// main
// ---------------------------------------------------------
int main(int argc, char** argv)
{
    bool verbose = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Enable NS-3 BLE log components", verbose);
    cmd.Parse(argc, argv);

    const uint32_t nNodes         = 5;
    const double   simDuration    = 60.0;
    const double   txDuration     = 50.0;
    const int      pktSize        = 20;
    const int      txIntervalSec  = 5;
    const uint32_t nbConnInterval = 3200;
    const double   roomSideMeters = 20.0;

    std::cout << "\n=== BLE Hello World Simulation ===\n";
    std::cout << "  Nodes            : " << nNodes << "\n";
    std::cout << "  Room side        : " << roomSideMeters << " m\n";
    std::cout << "  Packet size      : " << pktSize << " bytes\n";
    std::cout << "  Tx interval/node : " << txIntervalSec << " s\n";
    std::cout << "  Sim duration     : " << simDuration << " s\n\n";

    BleHelper bleHelper;
    if (verbose)
        bleHelper.EnableLogComponents();

    Packet::EnablePrinting();
    Packet::EnableChecking();

    // ---- Nodes ----
    NodeContainer nodes;
    nodes.Create(nNodes);

    // ---- Mobility: straight line ----
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    double spacing = roomSideMeters / (nNodes - 1);
    for (uint32_t i = 0; i < nNodes; i++)
        posAlloc->Add(Vector(i * spacing, 0.0, 1.0));

    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    std::cout << "Node positions:\n";
    for (uint32_t i = 0; i < nNodes; i++) {
        Vector pos = nodes.Get(i)->GetObject<MobilityModel>()->GetPosition();
        std::cout << "  Node " << (i+1) << " -> x=" << pos.x << " m\n";
    }
    std::cout << "\n";

    // ---- Install BLE ----
    NetDeviceContainer devices = bleHelper.Install(nodes);

    // ---- Addresses ----
    for (uint32_t i = 0; i < nNodes; i++) {
        std::stringstream ss;
        ss << std::hex << std::setw(4) << std::setfill('0') << (i + 1);
        std::string s = ss.str();
        s.insert(2, 1, ':');
        DynamicCast<BleNetDevice>(devices.Get(i))
            ->SetAddress(Mac16Address(s.c_str()));

        NodeStats st;
        st.addr = s;
        g_stats[i + 1] = st;
    }

    // ---- Links & traffic ----
    bleHelper.CreateBroadcastLink(devices, true, nbConnInterval, true);

    Ptr<UniformRandomVariable> randVar = CreateObject<UniformRandomVariable>();
    bleHelper.GenerateBroadcastTraffic(randVar, nodes, pktSize,
                                       0.0, txDuration, txIntervalSec);

    // ---- Trace callbacks ----
    for (uint32_t i = 0; i < devices.GetN(); i++) {
        Ptr<BleNetDevice> nd = DynamicCast<BleNetDevice>(devices.Get(i));
        nd->TraceConnectWithoutContext("MacTx",           MakeCallback(&OnTx));
        nd->TraceConnectWithoutContext("MacRxBroadcast",  MakeCallback(&OnBroadcastRx));
        nd->TraceConnectWithoutContext("MacRxError",      MakeCallback(&OnRxError));
        nd->TraceConnectWithoutContext("TXWindowSkipped", MakeCallback(&OnTxWindowSkipped));
    }

    // ---- Run ----
    std::cout << "--- Simulation events ---\n";
    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    // ---- Summary ----
    std::cout << "\n=== Final Statistics ===\n";
    std::cout << std::left
              << std::setw(8)  << "Node"
              << std::setw(8)  << "Addr"
              << std::setw(12) << "Sent"
              << std::setw(15) << "BroadcastRx"
              << std::setw(12) << "RxErrors"
              << std::setw(15) << "WinSkipped"
              << "\n";
    std::cout << std::string(70, '-') << "\n";

    uint32_t totalSent = 0, totalRx = 0;
    for (uint32_t i = 1; i <= nNodes; i++) {
        const NodeStats& s = g_stats[i];
        std::cout << std::left
                  << std::setw(8)  << i
                  << std::setw(8)  << ("00:0" + std::to_string(i))
                  << std::setw(12) << s.transmitted
                  << std::setw(15) << s.broadcastRx
                  << std::setw(12) << s.rxError
                  << std::setw(15) << s.txWindowSkipped
                  << "\n";
        totalSent += s.transmitted;
        totalRx   += s.broadcastRx;
    }
    std::cout << std::string(70, '-') << "\n";
    std::cout << "Total packets sent     : " << totalSent << "\n";
    std::cout << "Total broadcast recv'd : " << totalRx   << "\n";

    if (totalSent > 0) {
        double deliveryRatio = (double)totalRx / ((double)totalSent * (nNodes-1)) * 100.0;
        std::cout << std::fixed << std::setprecision(1)
                  << "Broadcast delivery     : " << deliveryRatio << "%\n";
    }
    std::cout << "\nDone.\n";
    return 0;
}
