// =============================================================================
// Scenario 2: VANET com Drones como nós relay estáticos
//
// Diferenças em relação ao Scenario 1 (vanet_simulation.cc):
//   1. NodeContainer separado para drones (droneNodes)
//   2. Drones recebem posições fixas via ConstantPositionMobilityModel
//   3. Veículos + Drones compartilham o mesmo canal 802.11p (allNodes)
//   4. O tráfego de aplicação continua apenas entre veículos
//   5. Drones participam do roteamento AODV como relay transparente
// =============================================================================

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ns2-mobility-helper.h"

#include <cmath>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <iomanip>
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetScenario2");

struct FlowSpec {
    uint32_t src;
    uint32_t dst;
    double   period_s;
    double   payload_MB;
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

// -----------------------------------------------------------------------------
// Configura canal 802.11p — idêntico ao Scenario 1
// Recebe allNodes (veículos + drones) para instalar o mesmo canal em todos
// -----------------------------------------------------------------------------
void SetupWave(NodeContainer& allNodes,
               NetDeviceContainer& devices,
               double commRange_m)
{
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
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",    StringValue("OfdmRate6MbpsBW10MHz"),
                                 "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    devices = wifi.Install(phy, mac, allNodes);
}

// -----------------------------------------------------------------------------
// Salva throughput apenas dos nós veículo (índices 0..numVehicles-1)
// Os drones (índices numVehicles..numVehicles+numDrones-1) são ignorados aqui
// porque eles não são destino de nenhum fluxo de aplicação
// -----------------------------------------------------------------------------
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
    const FlowMonitor::FlowStatsContainer& stats = monitor->GetFlowStats();

    std::map<uint32_t, uint64_t> rxBytesPerNode;
    for (uint32_t i = 0; i < numVehicles; i++)
        rxBytesPerNode[i] = 0;

    for (auto& kv : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(kv.first);
        for (uint32_t i = 0; i < numVehicles; i++)
        {
            if (interfaces.GetAddress(i) == t.destinationAddress)
            {
                rxBytesPerNode[i] += kv.second.rxBytes;
                break;
            }
        }
    }

    std::ofstream csv(outputFile);
    csv << "node_id,throughput_MBps\n";
    double totalThroughput = 0.0;
    for (uint32_t i = 0; i < numVehicles; i++)
    {
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

static void InstallTraffic(NodeContainer& vehicleNodes,
                           Ipv4InterfaceContainer& interfaces,
                           uint32_t numVehicles,
                           double simTime)
{
    uint16_t port = 9000;

    for (const auto& flow : TABLE_II)
    {
        if (flow.src >= numVehicles || flow.dst >= numVehicles)
            continue;

        double dataRate_bps = (flow.payload_MB * 1e6 * 8.0) / flow.period_s;
        std::string dataRateStr =
            std::to_string(static_cast<uint64_t>(dataRate_bps)) + "bps";

        Ipv4Address dstAddr = interfaces.GetAddress(flow.dst);

        UdpServerHelper server(port);
        ApplicationContainer srvApp = server.Install(vehicleNodes.Get(flow.dst));
        srvApp.Start(Seconds(0.0));
        srvApp.Stop(Seconds(simTime));

        OnOffHelper client("ns3::UdpSocketFactory",
                           InetSocketAddress(dstAddr, port));
        client.SetConstantRate(DataRate(dataRateStr));
        client.SetAttribute("PacketSize",  UintegerValue(1000));
        client.SetAttribute("OnTime",
            StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime",
            StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer cliApp = client.Install(vehicleNodes.Get(flow.src));
        cliApp.Start(Seconds(1.0));
        cliApp.Stop(Seconds(simTime));

        NS_LOG_INFO("Fluxo " << flow.src << "->" << flow.dst
                    << " | " << dataRateStr << " | port=" << port);
        port++;
    }
}


int main(int argc, char* argv[])
{
    uint32_t    numVehicles  = 30;
    uint32_t    numDrones    = 2;          // NOVO: quantidade de drones
    std::string mobilityFile = "../sumo/mobility_30v.tcl";
    double      simTime      = 100.0;
    double      commRange    = 500.0;
    std::string outputFile   = "../results/throughput_s2.csv";
    bool        verbose      = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("vehicles",     "Número de veículos",             numVehicles);
    cmd.AddValue("drones",       "Número de drones relay",         numDrones);  // NOVO
    cmd.AddValue("mobilityFile", "Trace de mobilidade NS2 do SUMO",mobilityFile);
    cmd.AddValue("simTime",      "Duração da simulação (s)",       simTime);
    cmd.AddValue("commRange",    "Alcance de comunicação (m)",     commRange);
    cmd.AddValue("outputFile",   "Arquivo CSV de saída",           outputFile);
    cmd.AddValue("verbose",      "Log detalhado",                  verbose);
    cmd.Parse(argc, argv);
    Time::SetResolution(Time::NS);

    if (verbose)
        LogComponentEnable("VanetScenario2", LOG_LEVEL_INFO);

    // -------------------------------------------------------------------------
    // VEÍCULOS: criação e mobilidade idênticas ao Scenario 1
    // -------------------------------------------------------------------------
    NodeContainer vehicleNodes;
    vehicleNodes.Create(numVehicles);

    Ns2MobilityHelper ns2mob(mobilityFile);
    ns2mob.Install();

    // -------------------------------------------------------------------------
    // DRONES: criação e posicionamento estático
    //
    // Posições escolhidas como centros de célula do grid SUMO:
    //   O grid vai de (1000,1000) a (3000,3000) com células de 200m.
    //   Centros de célula estão em: 1100, 1300, 1500, ..., 2900 em cada eixo.
    //
    //   Drone 0: (1500, 1900) — setor esquerdo, centro vertical
    //   Drone 1: (2500, 1900) — setor direito, centro vertical
    //
    // z=50 representa altitude do drone (não afeta propagação no modelo atual,
    // mas é semanticamente correto para extensões futuras).
    // -------------------------------------------------------------------------
    NodeContainer droneNodes;
    droneNodes.Create(numDrones);

    MobilityHelper droneMobility;
    droneMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    Ptr<ListPositionAllocator> dronePositions = CreateObject<ListPositionAllocator>();

    // Distribui os drones uniformemente em grade sobre o grid SUMO (1000..3000 x 1000..3000)
    {
        const double gridMinX = 1000.0, gridMaxX = 3000.0;
        const double gridMinY = 1000.0, gridMaxY = 3000.0;
        uint32_t cols = std::max(1u, (uint32_t)std::ceil(std::sqrt((double)numDrones)));
        uint32_t rows = (numDrones + cols - 1) / cols;
        uint32_t placed = 0;
        for (uint32_t r = 0; r < rows && placed < numDrones; r++) {
            for (uint32_t c = 0; c < cols && placed < numDrones; c++) {
                double x = gridMinX + (c + 0.5) * (gridMaxX - gridMinX) / cols;
                double y = gridMinY + (r + 0.5) * (gridMaxY - gridMinY) / rows;
                dronePositions->Add(Vector(x, y, 50.0));
                placed++;
            }
        }
    }

    droneMobility.SetPositionAllocator(dronePositions);
    droneMobility.Install(droneNodes);

    // -------------------------------------------------------------------------
    // CANAL COMPARTILHADO: veículos + drones no mesmo canal 802.11p
    //
    // allNodes agrupa os dois containers para que SetupWave instale
    // uma única interface WiFi em todos, permitindo comunicação direta
    // entre qualquer par (veículo-veículo, veículo-drone, drone-drone).
    // -------------------------------------------------------------------------
    NodeContainer allNodes;
    allNodes.Add(vehicleNodes);
    allNodes.Add(droneNodes);

    NetDeviceContainer devices;
    SetupWave(allNodes, devices, commRange);

    // -------------------------------------------------------------------------
    // ROTEAMENTO AODV + PILHA IP em todos os nós
    //
    // O AODV descobre rotas automaticamente. Quando dois veículos estão
    // fora do alcance direto, o AODV pode usar um drone como relay se
    // ele estiver no caminho (dentro dos 500m de ambos os lados).
    // -------------------------------------------------------------------------
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // -------------------------------------------------------------------------
    // TRÁFEGO: apenas entre veículos (drones são relay transparentes)
    //
    // interfaces.GetAddress(i) para i < numVehicles → endereço dos veículos
    // interfaces.GetAddress(numVehicles + j)         → endereço dos drones
    // (drones não recebem tráfego de aplicação)
    // -------------------------------------------------------------------------
    InstallTraffic(vehicleNodes, interfaces, numVehicles, simTime);

    FlowMonitorHelper fmHelper;
    Ptr<FlowMonitor> monitor = fmHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    SaveThroughput(fmHelper, monitor, interfaces, numVehicles, simTime, outputFile);

    Simulator::Destroy();
    return 0;
}
