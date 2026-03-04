#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-module.h"

using namespace ns3;


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


int main(int argc, char* argv[]){
   
    uint32_t    numVehicles  = 30;
    std::string mobilityFile = "../../sumo/mobility.tcl";
    double      simTime      = 100.0;   // s
    double      commRange    = 500.0;   // m  (paper Tabela I)
    std::string outputFile   = "../../results/throughput.csv";
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

    NetDeviceContainer devices;
    SetupWave(nodes, devices, commRange);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.0.0.0");
}