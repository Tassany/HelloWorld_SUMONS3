#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-module.h"
#include "ns3/netanim-module.h" // Animation

using namespace ns3;

int main(int argc, char* argv[])
{
    std::string traceFile = "../examples/udp-3-nodes/sumo/mobility.tcl";

    CommandLine cmd;
    cmd.AddValue("traceFile", "Path to the NS2 mobility trace file", traceFile);
    cmd.Parse(argc, argv);

    NodeContainer cars;
    cars.Create(3);

    // Create a channel helper and phy helper, and then create the channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    
    // Create a WifiMacHelper, which is reused across STA and AP configurations
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    // Create a WifiHelper, which will use the above helpers to create
    // and install Wifi devices.  Configure a Wifi standard to use, which
    // will align various parameters in the Phy and Mac to standard defaults.
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n); 

    // Declare NetDeviceContainers to hold the container returned by the helper
    // install wifi in the cars
    NetDeviceContainer devices = wifi.Install(phy, mac, cars);

    //
    // Add the IPv4 protocol stack to the nodes in our container
    //

    OlsrHelper olsr;  // OLSR routing protocol, a proactive routing protocol for ad hoc networks

    InternetStackHelper internet;
    internet.SetRoutingHelper(olsr); 
    internet.Install(cars);

    Ipv4AddressHelper ipAddrs;
    ipAddrs.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipAddrs.Assign(devices);


    ////////////////////////////Mobility////////////////////////////////////

    // MobilityHelper mobilidade;
    // Ptr<ListPositionAllocator> posicoes = CreateObject<ListPositionAllocator>();
    // posicoes->Add(Vector(0.0,   0.0, 0.0));   // carro v0
    // posicoes->Add(Vector(100.0, 0.0, 0.0));   // carro v1 (100m à frente)
    // posicoes->Add(Vector(200.0, 0.0, 0.0));   // carro v2 (200m à frente)

    // mobilidade.SetPositionAllocator(posicoes);
    // // mobilidade.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    // mobilidade.SetMobilityModel("ns3::ConstantVelocityMobilityModel" ); // Animation
    // mobilidade.Install(cars);

    
    Ns2MobilityHelper mobility(traceFile);
    mobility.Install();

    // // Animation 
    // cars.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(20, 0, 0));
    // cars.Get(1)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(15, 0, 0));
    // cars.Get(2)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(10, 0, 0));

    ////////////////////////////////////////
    

    uint16_t porta = 9;

    // RECEPTOR: instala servidor UDP no carro v2
    UdpEchoServerHelper servidor(porta);
    ApplicationContainer appServidor = servidor.Install(cars.Get(2));
    appServidor.Start(Seconds(1.0));
    appServidor.Stop(Seconds(100.0));

    // EMISSOR: v0 envia pacotes para o IP de v2
    UdpEchoClientHelper cliente(interfaces.GetAddress(2), porta);
    cliente.SetAttribute("MaxPackets", UintegerValue(50));
    cliente.SetAttribute("Interval",   TimeValue(Seconds(1.0)));   // 1 pacote/segundo
    cliente.SetAttribute("PacketSize", UintegerValue(512));         // 512 bytes

    ApplicationContainer appCliente = cliente.Install(cars.Get(0));
    appCliente.Start(Seconds(2.0));   // começa em t=2s (OLSR precisa de tempo para convergir)
    appCliente.Stop(Seconds(100.0));

    AnimationInterface anim("animation.xml");

    // ← 3. PERSONALIZA A APARÊNCIA DOS NÓS
    anim.SetConstantPosition(cars.Get(0), 0.0,   0.0);  // posição inicial no mapa
    anim.SetConstantPosition(cars.Get(1), 100.0, 0.0);
    anim.SetConstantPosition(cars.Get(2), 200.0, 0.0);

    // Cor e tamanho de cada nó
    anim.UpdateNodeColor(cars.Get(0), 255, 0,   0);   // v0 = vermelho (emissor)
    anim.UpdateNodeColor(cars.Get(1), 0,   255, 0);   // v1 = verde    (relay)
    anim.UpdateNodeColor(cars.Get(2), 0,   0,   255); // v2 = azul     (receptor)
    anim.UpdateNodeSize(0, 10, 10);
    anim.UpdateNodeSize(1, 10, 10);
    anim.UpdateNodeSize(2, 10, 10);

    // Rótulo visível em cada nó
    anim.UpdateNodeDescription(cars.Get(0), "v0-emissor");
    anim.UpdateNodeDescription(cars.Get(1), "v1-relay");
    anim.UpdateNodeDescription(cars.Get(2), "v2-receptor");

    Simulator::Stop(Seconds(100.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;


}