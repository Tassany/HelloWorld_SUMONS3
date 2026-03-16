#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"


using namespace ns3;

void PacketReceived(std::string context, Ptr<const Packet> packet,
                    const Address& adressOrigin, const Address& adressFinal) {
  NS_LOG_UNCOND("✅ [" << Simulator::Now().GetSeconds() << "s] "
                << context << " received " << packet->GetSize() << " bytes"
                << " from " << adressOrigin);
}


int main(int argc, char *argv[]){
    double rangeWifi  = 150.0;   // meters 
    double simTime    = 45.0;
    double numVehicles  = 5;
    std::string mobilityFile =
    "../sumo/mobility.tcl"; 

    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss(
        "ns3::RangePropagationLossModel",
        "MaxRange", DoubleValue(rangeWifi)   // ← range de 150m
    );

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, vehicles);
    Ns2MobilityHelper mobility(mobilityFile);
    mobility.Install();

    OlsrHelper olsr;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsr);
    internet.Install(vehicles);

    Ipv4AddressHelper adress;
    adress.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = adress.Assign(devices);

    uint16_t port1=9;
    uint16_t port2=10;

    UdpEchoServerHelper serverV2(port1);
    ApplicationContainer apps1 = serverV2.Install(vehicles.Get(0));
    apps1.Start(Seconds(1.0));
    apps1.Stop(Seconds(simTime));

    UdpEchoServerHelper serverV5(port2);
    ApplicationContainer appV5 = serverV5.Install(vehicles.Get(4));
    appV5.Start(Seconds(1.0));
    appV5.Stop(Seconds(simTime));


    // v1 sends to v2 — 1 packet per second
    UdpEchoClientHelper clientV1(interfaces.GetAddress(1), port1);
    clientV1.SetAttribute("MaxPackets", UintegerValue(500));
    clientV1.SetAttribute("Interval",   TimeValue(Seconds(1.0)));
    clientV1.SetAttribute("PacketSize", UintegerValue(256));
    ApplicationContainer appV1 = clientV1.Install(vehicles.Get(0));
    appV1.Start(Seconds(5.0));   // wait 5s for OLSR to converge
    appV1.Stop(Seconds(simTime));

    // v3 sends to v5 — 1 packet every 2 seconds
    UdpEchoClientHelper clientV3(interfaces.GetAddress(4), port2);
    clientV3.SetAttribute("MaxPackets", UintegerValue(500));
    clientV3.SetAttribute("Interval",   TimeValue(Seconds(2.0)));
    clientV3.SetAttribute("PacketSize", UintegerValue(256));
    ApplicationContainer appV3 = clientV3.Install(vehicles.Get(2));
    appV3.Start(Seconds(5.0));   
    appV3.Stop(Seconds(simTime));

    Config::Connect(
        "/NodeList/1/ApplicationList/0/$ns3::UdpEchoServer/RxWithAddresses",
        MakeCallback(&PacketReceived)
    );
    Config::Connect(
        "/NodeList/4/ApplicationList/0/$ns3::UdpEchoServer/RxWithAddresses",
        MakeCallback(&PacketReceived)
    );

    // -----------------------------------------------------------
    // 8. NETANIM
    // -----------------------------------------------------------
    AnimationInterface anim("animation.xml");

    // Cores: vermelho=emissor, azul=receptor, amarelo=relay, cinza=inativo
    anim.UpdateNodeColor(vehicles.Get(0), 255, 0,   0);    // v1 vermelho
    anim.UpdateNodeColor(vehicles.Get(1), 0,   0,   255);  // v2 azul
    anim.UpdateNodeColor(vehicles.Get(2), 255, 0,   0);    // v3 vermelho
    anim.UpdateNodeColor(vehicles.Get(3), 255, 255, 0);    // v4 amarelo (relay)
    anim.UpdateNodeColor(vehicles.Get(4), 0,   0,   255);  // v5 azul

    anim.UpdateNodeDescription(vehicles.Get(0), "v1 [envia->v2]");
    anim.UpdateNodeDescription(vehicles.Get(1), "v2 [recebe]");
    anim.UpdateNodeDescription(vehicles.Get(2), "v3 [envia->v5]");
    anim.UpdateNodeDescription(vehicles.Get(3), "v4 [relay]");
    anim.UpdateNodeDescription(vehicles.Get(4), "v5 [recebe]");

    for (uint32_t i = 0; i < 5; i++) anim.UpdateNodeSize(i, 8, 8);

    // -----------------------------------------------------------
    // 9. RODAR
    // -----------------------------------------------------------
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}