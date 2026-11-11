#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace std;
using namespace ns3;
NS_LOG_COMPONENT_DEFINE("FirstNetwork");


void FailLink(NodeContainer nodes, NetDeviceContainer link)
{
    Ptr<Ipv4> ipv4Node0 = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4Node1 = nodes.Get(1)->GetObject<Ipv4>();

    uint32_t interface0 =
        ipv4Node0->GetInterfaceForDevice(link.Get(0));

    uint32_t interface1 =
        ipv4Node1->GetInterfaceForDevice(link.Get(1));

    ipv4Node0->SetDown(interface0);
    ipv4Node1->SetDown(interface1);

    Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

    cout << "\nLINK FAILURE: Node 0 - Node 1 at "
              << Simulator::Now().GetSeconds() << " seconds\n";
}


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

    // 6. // Make Path B less preferable so traffic initially uses Path A
Ptr<Ipv4> ipv4Node0 = nodes.Get(0)->GetObject<Ipv4>();
Ptr<Ipv4> ipv4Node2 = nodes.Get(2)->GetObject<Ipv4>();

uint32_t interface02 = ipv4Node0->GetInterfaceForDevice(d02.Get(0));
uint32_t interface23 = ipv4Node2->GetInterfaceForDevice(d23.Get(0));

ipv4Node0->SetMetric(interface02, 10);
ipv4Node2->SetMetric(interface23, 10);

// Calculate routing tables
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
    Simulator::Schedule(Seconds(5.0), &FailLink, nodes, d01);
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Simulator::Stop(Seconds(13.0));

    cout << "Starting 4-node diamond network simulation..."
              << endl;

    Simulator::Run();
    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile(
    "first-network-flowmon.xml",
    true,
    true
);
    Simulator::Destroy();

    cout << "Simulation completed successfully!"
              << endl;

    return 0;
}