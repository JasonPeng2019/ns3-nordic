/* ble-hello-world-2.cc
 *
 * Same 5-node line topology as ble-hello-world.cc, but instead of empty
 * nodes firing packets on a timer, each node runs the real BLE mesh
 * discovery protocol (BleDiscoveryEngineWrapper) over the actual BLE
 * radio stack (BlePhy + SpectrumChannel).
 *
 * This is the key difference from the existing phase2/phase3 simulations,
 * which use a SimpleVirtualChannel that bypasses the radio entirely.
 * Here, packets travel through the real BLE PHY — propagation loss,
 * collision modelling, and connection-interval timing all apply.
 *
 * Run:
 *   python3 waf --run ble-hello-world-2
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
#include "ns3/ble-discovery-engine-wrapper.h"
#include "ns3/ble-discovery-header-wrapper.h"
#include <iostream>
#include <iomanip>
#include <map>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BleHelloWorld2");

// ---------------------------------------------------------
// Global maps used by callbacks
// ---------------------------------------------------------
std::map<uint32_t, Ptr<BleDiscoveryEngineWrapper>> g_engines; // NS-3 node id -> engine
std::map<uint16_t, Ptr<BleNetDevice>> g_devicesByAddr;         // MAC16 -> device

static uint16_t
MacToUint16 (const Mac16Address& mac)
{
    uint8_t buf[2] = {0, 0};
    mac.CopyTo(buf);
    return static_cast<uint16_t>(buf[0] << 8) | static_cast<uint16_t>(buf[1]);
}

static int8_t
EstimateRssiDbm (const BleMacHeader& macHdr, const Ptr<const BleNetDevice>& receiver)
{
    auto it = g_devicesByAddr.find(MacToUint16(macHdr.GetSrcAddr()));
    if (it == g_devicesByAddr.end())
    {
        return -127;
    }

    Ptr<BleNetDevice> sender = it->second;
    Ptr<MobilityModel> txMob = sender->GetNode()->GetObject<MobilityModel>();
    Ptr<MobilityModel> rxMob = receiver->GetNode()->GetObject<MobilityModel>();
    if (txMob == nullptr || rxMob == nullptr)
    {
        return -127;
    }

    Ptr<SpectrumChannel> ch = receiver->GetPhy()->GetChannel();
    if (ch == nullptr)
    {
        return -127;
    }

    Ptr<PropagationLossModel> loss = ch->GetPropagationLossModel();
    if (loss == nullptr)
    {
        return -127;
    }

    // BlePhy currently defaults to 10 mW transmit power (10 dBm).
    constexpr double kDefaultBleTxPowerDbm = 10.0;
    const double rxPowerDbm = loss->CalcRxPower(kDefaultBleTxPowerDbm, txMob, rxMob);
    return static_cast<int8_t>(std::lround(rxPowerDbm));
}

static void
SyncEngineGpsFromMobility (Ptr<BleDiscoveryEngineWrapper> engine,
                           Ptr<Node> node,
                           Time period)
{
    Ptr<MobilityModel> mm = node->GetObject<MobilityModel>();
    if (mm != nullptr)
    {
        engine->SetGpsLocation(mm->GetPosition(), true);
    }
    else
    {
        engine->SetGpsLocation(Vector(), false);
    }

    Simulator::Schedule(period, &SyncEngineGpsFromMobility, engine, node, period);
}

// ---------------------------------------------------------
// TX: engine wants to send — hand packet to BleNetDevice
// ---------------------------------------------------------
void SendViaDevice(Ptr<BleNetDevice> device, Ptr<Packet> pkt)
{
    device->Send(pkt, Mac16Address("FF:FF"), 0);
}

// ---------------------------------------------------------
// RX: broadcast packet arrived — hand it to the engine
// Hooked via MacRxBroadcast trace source, which fires for all
// broadcast receptions (SetReceiveCallback only fires for unicast).
// ---------------------------------------------------------
void OnBroadcastReceive(Ptr<BleDiscoveryEngineWrapper> engine,
                        const Ptr<const Packet> pkt,
                        const Ptr<const BleNetDevice> receiver)
{
    Ptr<Packet> copy = pkt->Copy();
    // Strip the BleMacHeader that BleNetDevice leaves on the packet
    BleMacHeader macHdr;
    copy->RemoveHeader(macHdr);
    const int8_t rssiDbm = EstimateRssiDbm(macHdr, receiver);

    BleDiscoveryHeaderWrapper hdr;
    if (copy->RemoveHeader(hdr) == 0)
        return;

    engine->Receive(hdr, rssiDbm);
}

// ---------------------------------------------------------
// Trace callbacks — same as ble-hello-world for observability
// ---------------------------------------------------------
void OnTx(const Ptr<const Packet> pkt)
{
    Ptr<Packet> copy = pkt->Copy();
    BleMacHeader hdr;
    copy->RemoveHeader(hdr);
    uint8_t buf[2];
    hdr.GetSrcAddr().CopyTo(buf);
    uint32_t idx = buf[1];
    std::cout << std::fixed << std::setprecision(3)
              << "[" << Simulator::Now().GetSeconds() << "s] "
              << "Node " << idx << " SENT a discovery packet\n";
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
    std::cout << std::fixed << std::setprecision(3)
              << "[" << Simulator::Now().GetSeconds() << "s] "
              << "Node " << rxIdx << " RECV discovery from Node " << txIdx << "\n";
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
    const double   simDuration    = 30.0;   // enough for multiple discovery cycles
    // nbConnInterval in units of 1.25 ms.  3200 = 4 s is realistic BLE but far too
    // slow: the engine's NEIGHBOR window is only 200 ms, so there is only a 5%
    // chance of a radio-level packet landing during that window.  Use 8 (= 10 ms)
    // so the radio keeps pace with the protocol engine.
    const uint32_t nbConnInterval = 8;
    const double   roomSideMeters = 20.0;

    // Discovery engine parameters
    const uint32_t slotDurationMs     = 50;
    const uint32_t noiseSlotCount     = 1;
    const uint32_t neighborSlotCount  = 4;
    const uint8_t  initialTtl         = 3;
    const double   proximityThreshold = 10.0;
    const uint32_t forwardingSeed     = 424242;

    // Randomize initial phase so nodes are asynchronous without enforcing a
    // fixed deterministic staggering pattern.
    const uint32_t discoverySlots     = 4; // BLE_DISCOVERY_NUM_SLOTS
    const uint32_t fullCycleMs        = (noiseSlotCount + neighborSlotCount + discoverySlots)
                                        * slotDurationMs;

    std::cout << "\n=== BLE Hello World 2 — Discovery Protocol Simulation ===\n";
    std::cout << "  Nodes            : " << nNodes << "\n";
    std::cout << "  Room side        : " << roomSideMeters << " m\n";
    std::cout << "  Sim duration     : " << simDuration << " s\n";
    std::cout << "  Slot duration    : " << slotDurationMs << " ms\n";
    std::cout << "  Noise slots      : " << noiseSlotCount << "\n";
    std::cout << "  Neighbor slots   : " << neighborSlotCount << "\n";
    std::cout << "  Initial TTL      : " << (int)initialTtl << "\n";

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

    // ---- Install BLE radio stack ----
    NetDeviceContainer devices = bleHelper.Install(nodes);

    // ---- Assign BLE addresses ----
    for (uint32_t i = 0; i < nNodes; i++) {
        std::stringstream ss;
        ss << std::hex << std::setw(4) << std::setfill('0') << (i + 1);
        std::string s = ss.str();
        s.insert(2, 1, ':');
        Ptr<BleNetDevice> nd = DynamicCast<BleNetDevice>(devices.Get(i));
        Mac16Address addr(s.c_str());
        nd->SetAddress(addr);
        g_devicesByAddr[MacToUint16(addr)] = nd;
    }

    // ---- Broadcast link — disable built-in collision avoidance so medium access
    // behavior comes from radio/channel effects instead of helper-level staggering.
    bleHelper.CreateBroadcastLink(devices, true, nbConnInterval, false);

    // ---- Install discovery engine on each node ----
    for (uint32_t i = 0; i < nNodes; i++) {
        Ptr<BleNetDevice> nd = DynamicCast<BleNetDevice>(devices.Get(i));
        uint32_t nsNodeId = nodes.Get(i)->GetId();   // NS-3 node id (0-based)
        uint32_t engineId = i + 1;                   // protocol node id (1-based)

        Ptr<BleDiscoveryEngineWrapper> engine = CreateObject<BleDiscoveryEngineWrapper>();
        engine->SetAttribute("NodeId",            UintegerValue(engineId));
        engine->SetAttribute("SlotDuration",      TimeValue(MilliSeconds(slotDurationMs)));
        engine->SetAttribute("InitialTtl",        UintegerValue(initialTtl));
        engine->SetAttribute("ProximityThreshold",DoubleValue(proximityThreshold));
        engine->SetAttribute("NoiseSlotCount",          UintegerValue(noiseSlotCount));
        engine->SetAttribute("NoiseSlotDuration",       TimeValue(MilliSeconds(slotDurationMs)));
        engine->SetAttribute("NeighborSlotCount",       UintegerValue(neighborSlotCount));
        engine->SetAttribute("NeighborSlotDuration",    TimeValue(MilliSeconds(slotDurationMs)));
        // Keep neighbors for 20 cycles (~9 s) so they survive across multiple
        // discovery rounds even if the radio misses a few NEIGHBOR windows.
        engine->SetAttribute("NeighborTimeoutCycles",   UintegerValue(20));

        // TX: engine produces a Packet → send via BleNetDevice
        engine->SetSendCallback(MakeBoundCallback(&SendViaDevice, nd));

        // RX: hook MacRxBroadcast trace source → hand packet to engine
        nd->TraceConnectWithoutContext("MacRxBroadcast",
            MakeBoundCallback(&OnBroadcastReceive, engine));

        // The current protocol core uses a global forwarding RNG; seed once for
        // deterministic runs (per-node independent streams are not available here).
        if (i == 0) {
            engine->SeedRandom(forwardingSeed);
        }

        NS_ABORT_MSG_IF(!engine->Initialize(),
            "Failed to initialize engine for node " << engineId);

        // Feed physical position into protocol GPS state and keep it synced.
        SyncEngineGpsFromMobility(engine, nodes.Get(i), MilliSeconds(100));

        g_engines[nsNodeId] = engine;
    }

    // ---- Trace callbacks for TX visibility ----
    for (uint32_t i = 0; i < devices.GetN(); i++) {
        Ptr<BleNetDevice> nd = DynamicCast<BleNetDevice>(devices.Get(i));
        nd->TraceConnectWithoutContext("MacTx",          MakeCallback(&OnTx));
        nd->TraceConnectWithoutContext("MacRxBroadcast", MakeCallback(&OnBroadcastRx));
    }

    // ---- Start engines with randomized offsets in one full cycle ----
    Ptr<UniformRandomVariable> phaseOffsetRv = CreateObject<UniformRandomVariable>();
    phaseOffsetRv->SetAttribute("Min", DoubleValue(0.0));
    phaseOffsetRv->SetAttribute("Max", DoubleValue(static_cast<double>(fullCycleMs)));
    std::cout << "  Full cycle       : " << fullCycleMs << " ms\n";
    std::cout << "  Start phase      : random in [0, " << fullCycleMs << "] ms\n\n";
    std::cout << "--- Simulation events ---\n";
    for (uint32_t i = 0; i < nNodes; i++) {
        Ptr<BleDiscoveryEngineWrapper> eng = g_engines[nodes.Get(i)->GetId()];
        double delayMs = phaseOffsetRv->GetValue();
        Simulator::Schedule(MilliSeconds(delayMs),
                            &BleDiscoveryEngineWrapper::Start, eng);
    }

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    // ---- Discovery results ----
    std::cout << "\n=== Discovery Results ===\n";
    std::cout << std::left
              << std::setw(8)  << "Node"
              << std::setw(10) << "Neighbors"
              << std::setw(10) << "Sent"
              << std::setw(10) << "Received"
              << std::setw(10) << "Forwarded"
              << std::setw(10) << "Dropped"
              << "\n";
    std::cout << std::string(58, '-') << "\n";

    for (uint32_t i = 0; i < nNodes; i++) {
        uint32_t nsNodeId = nodes.Get(i)->GetId();
        Ptr<BleDiscoveryEngineWrapper> engine = g_engines[nsNodeId];
        const ble_mesh_node_t* state = engine->GetNode();

        std::cout << std::left
                  << std::setw(8)  << (i + 1)
                  << std::setw(10) << state->neighbors.count
                  << std::setw(10) << state->stats.messages_sent
                  << std::setw(10) << state->stats.messages_received
                  << std::setw(10) << state->stats.messages_forwarded
                  << std::setw(10) << state->stats.messages_dropped
                  << "\n";
    }

    std::cout << "\nDone.\n";
    return 0;
}
