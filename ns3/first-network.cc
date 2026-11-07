#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("FirstNetwork");

int main()
{
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // 1. Create four nodes
    NodeContainer nodes;
    nodes.Create(4);

    // 2. Configure point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // 3. Connect the nodes in a diamond topology
    NetDeviceContainer d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d13 = p2p.Install(nodes.Get(1), nodes.Get(3));
    NetDeviceContainer d02 = p2p.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));

    // 4. Install the Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // 5. Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i01 = address.Assign(d01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i13 = address.Assign(d13);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i02 = address.Assign(d02);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = address.Assign(d23);

    // 6. Calculate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();


    // 7. Install UDP Echo Server on Node 3
    UdpEchoServerHelper server(9);
    ApplicationContainer serverApp = server.Install(nodes.Get(3));

    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(12.0));

    // 8. Install UDP Echo Client on Node 0
    UdpEchoClientHelper client(i13.GetAddress(1), 9);

    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));

    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(12.0));

    // 9. Run simulation
    Simulator::Stop(Seconds(13.0));

    std::cout << "Starting 4-node diamond network simulation..."
              << std::endl;

    Simulator::Run();
    Simulator::Destroy();

    std::cout << "Simulation completed successfully!"
              << std::endl;

    return 0;
}