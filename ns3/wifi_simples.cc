#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimples");

int main(int argc, char* argv[])
{
    bool verbose = true;
    double simTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Ativar logs", verbose);
    cmd.AddValue("simTime", "Duração da simulação (s)", simTime);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    // --- Criar 2 nós ---
    NodeContainer nos;
    nos.Create(2);

    // --- Configurar canal e dispositivos WiFi (modo ad hoc) ---
    YansWifiChannelHelper canal = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(canal.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer dispositivos = wifi.Install(phy, mac, nos);

    // --- Mobilidade: nós parados, a 50 m de distância ---
    MobilityHelper mobilidade;
    Ptr<ListPositionAllocator> posicoes = CreateObject<ListPositionAllocator>();
    posicoes->Add(Vector(0.0, 0.0, 0.0));   // Nó 0
    posicoes->Add(Vector(50.0, 0.0, 0.0));  // Nó 1

    mobilidade.SetPositionAllocator(posicoes);
    mobilidade.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilidade.Install(nos);

    // --- Pilha de protocolos IP ---
    InternetStackHelper internet;
    internet.Install(nos);

    Ipv4AddressHelper endereco;
    endereco.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = endereco.Assign(dispositivos);

    // --- Aplicação: Servidor UDP Echo no Nó 1 ---
    uint16_t porta = 9;
    UdpEchoServerHelper servidor(porta);
    ApplicationContainer appServidor = servidor.Install(nos.Get(1));
    appServidor.Start(Seconds(1.0));
    appServidor.Stop(Seconds(simTime));

    // --- Aplicação: Cliente UDP Echo no Nó 0 ---
    UdpEchoClientHelper cliente(interfaces.GetAddress(1), porta);
    cliente.SetAttribute("MaxPackets", UintegerValue(5));
    cliente.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    cliente.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer appCliente = cliente.Install(nos.Get(0));
    appCliente.Start(Seconds(2.0));
    appCliente.Stop(Seconds(simTime));

    // --- Captura de pacotes (opcional) ---
    phy.EnablePcapAll("wifi-simples");

    NS_LOG_INFO("Iniciando simulação...");
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Simulação concluída.");

    return 0;
}
