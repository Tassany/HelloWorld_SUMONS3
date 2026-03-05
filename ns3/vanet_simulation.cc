#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ns2-mobility-helper.h"

#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <iomanip>
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

struct FlowSpec {
    uint32_t src;        // ID do nó fonte
    uint32_t dst;        // ID do nó destino
    double   period_s;   // período (s)
    double   payload_MB; // payload por período (MB)
};

static const std::vector<FlowSpec> TABLE_II = {
    { 0,  1, 0.1, 0.5},
    { 0,  9, 1.0, 1.0},
    { 1,  2, 0.2, 0.6},
    { 2,  3, 0.2, 0.8},
    { 3,  4, 0.2, 0.6},
    { 5, 10, 0.5, 1.2},
    { 6, 12, 0.3, 1.0},
    { 7, 13, 0.2, 1.0},
    { 8, 14, 0.5, 1.5},
    { 9, 15, 0.1, 0.5},
    {11, 19, 1.0, 1.5},
    {15, 18, 0.3, 0.8},
    {20, 25, 0.2, 0.4},
    {22, 29, 0.3, 0.6},
};

void SetupWave(NodeContainer& nodes,
                NetDeviceContainer& devices,
                double commRange_m){
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss(
        "ns3::RangePropagationLossModel",
        "MaxRange", DoubleValue(commRange_m)
    );

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);
    // 6 Mbps OFDM com largura de banda 10 MHz (padrão 802.11p)
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",    StringValue("OfdmRate6MbpsBW10MHz"),
                                 "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    devices = wifi.Install(phy, mac, nodes);
}
// TODO: Verify if this code is correct
void SaveThroughput(FlowMonitorHelper& fmHelper,
                           Ptr<FlowMonitor> monitor,
                           Ipv4InterfaceContainer& interfaces,
                           uint32_t numVehicles,
                           double simTime,
                           const std::string& outputFile)
{
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(fmHelper.GetClassifier());
    // NS3 3.41: FlowStatsMap → FlowStatsContainer
    const FlowMonitor::FlowStatsContainer& stats = monitor->GetFlowStats();

    // Acumular bytes recebidos por nó destino
    std::map<uint32_t, uint64_t> rxBytesPerNode;
    for (uint32_t i = 0; i < numVehicles; i++)
        rxBytesPerNode[i] = 0;

    for (auto& kv : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(kv.first);
        // Identificar o nó destino pelo endereço IP
        for (uint32_t i = 0; i < numVehicles; i++)
        {
            if (interfaces.GetAddress(i) == t.destinationAddress)
            {
                rxBytesPerNode[i] += kv.second.rxBytes;
                break;
            }
        }
    }

    // Escrever CSV
    std::ofstream csv(outputFile);
    csv << "node_id,throughput_MBps\n";
    double totalThroughput = 0.0;
    for (uint32_t i = 0; i < numVehicles; i++)
    {
        // throughput médio ao longo de toda a simulação (descontando 1s inicial)
        double eff_time = simTime - 1.0;
        double tp_MBps  = (eff_time > 0)
                          ? (rxBytesPerNode[i] / 1e6) / eff_time
                          : 0.0;
        csv << i << "," << std::fixed << std::setprecision(4) << tp_MBps << "\n";
        totalThroughput += tp_MBps;
    }
    csv.close();

    std::cout << "\n══════════════════════════════════════════" << std::endl;
    std::cout << "  Resultados salvos em: " << outputFile << std::endl;
    std::cout << "  Throughput total: "
              << std::fixed << std::setprecision(3)
              << totalThroughput << " MB/s" << std::endl;
    std::cout << "  Throughput médio/veículo: "
              << totalThroughput / numVehicles << " MB/s" << std::endl;
    std::cout << "══════════════════════════════════════════\n" << std::endl;

    // Log detalhado no terminal
    std::cout << "node_id | throughput (MB/s)" << std::endl;
    std::cout << "--------+------------------" << std::endl;
    for (uint32_t i = 0; i < numVehicles; i++)
    {
        double eff_time = simTime - 1.0;
        double tp = (eff_time > 0)
                    ? (rxBytesPerNode[i] / 1e6) / eff_time
                    : 0.0;
        std::cout << "  " << std::setw(4) << i
                  << "  | " << std::fixed << std::setprecision(4) << tp
                  << std::endl;
    }
}

// TODO: Verify if this code is correct 
static void InstallTraffic(NodeContainer& nodes,
                           Ipv4InterfaceContainer& interfaces,
                           uint32_t numVehicles,
                           double simTime)
{
    uint16_t basePort = 9000;
    uint16_t port     = basePort;

    for (const auto& flow : TABLE_II)
    {
        if (flow.src >= numVehicles || flow.dst >= numVehicles)
            continue; // nó não existe neste cenário

        // Taxa de dados: payload / período → Bps → bps
        double dataRate_bps = (flow.payload_MB * 1e6 * 8.0) / flow.period_s;
        std::string dataRateStr =
            std::to_string(static_cast<uint64_t>(dataRate_bps)) + "bps";

        Ipv4Address dstAddr = interfaces.GetAddress(flow.dst);

        // Servidor UDP no destino
        UdpServerHelper server(port);
        ApplicationContainer srvApp = server.Install(nodes.Get(flow.dst));
        srvApp.Start(Seconds(0.0));
        srvApp.Stop(Seconds(simTime));

        // Cliente UDP OnOff na fonte
        OnOffHelper client("ns3::UdpSocketFactory",
                           InetSocketAddress(dstAddr, port));
        client.SetConstantRate(DataRate(dataRateStr));
        client.SetAttribute("PacketSize",  UintegerValue(1000)); // bytes
        client.SetAttribute("OnTime",
            StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime",
            StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer cliApp = client.Install(nodes.Get(flow.src));
        cliApp.Start(Seconds(1.0)); // aguarda roteamento convergir
        cliApp.Stop(Seconds(simTime));

        NS_LOG_INFO("Fluxo " << flow.src << "->" << flow.dst
                    << " | " << dataRateStr
                    << " | port=" << port);
        port++;
    }
}


int main(int argc, char* argv[]){
   
    uint32_t    numVehicles  = 30;
    std::string mobilityFile = "../sumo/mobility_30v.tcl";
    double      simTime      = 100.0;   // s
    double      commRange    = 500.0;   // meters. Communication range for 802.11p is typically around 300-500 meters in open environments, but can be less in urban areas due to obstacles.
    std::string outputFile   = "../results/throughput.csv";
    bool        verbose      = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("vehicles",     "Número de veículos (10/20/30)",  numVehicles);
    cmd.AddValue("mobilityFile", "Trace de mobilidade NS2 do SUMO",mobilityFile);
    cmd.AddValue("simTime",      "Duração da simulação (s)",       simTime);
    cmd.AddValue("commRange",    "Alcance de comunicação (m)",     commRange);
    cmd.AddValue("outputFile",   "Arquivo CSV de saída",           outputFile);
    cmd.AddValue("verbose",      "Log detalhado",                  verbose);
    cmd.Parse(argc, argv);
    Time::SetResolution(Time::NS);

    if (verbose)
    {
        LogComponentEnable("VanetSimulation", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(numVehicles);

    Ns2MobilityHelper ns2mob(mobilityFile);
    ns2mob.Install();

    NetDeviceContainer devices;
    SetupWave(nodes, devices, commRange);

    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    InstallTraffic(nodes, interfaces, numVehicles, simTime);

    FlowMonitorHelper fmHelper;
    Ptr<FlowMonitor> monitor = fmHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
        SaveThroughput(fmHelper, monitor, interfaces,
                   numVehicles, simTime, outputFile);

    Simulator::Destroy();

    return 0;
}