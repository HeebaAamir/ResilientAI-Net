#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include <fstream>
#include <vector>
#include <memory>
#include <climits>
#include <algorithm>
#include <map>

using namespace std;
using namespace ns3;
NS_LOG_COMPONENT_DEFINE("FirstNetwork");

void AddEdge(vector<vector<int>>& graph, int u, int v, int weight)
{
    graph[u][v] = weight;
    graph[v][u] = weight;
}

vector <int> dijkstra(vector<vector<int>> graph, int src)
{
    int V = graph.size();

    // Stores the shortest distance discovered from source to each node
    // Initially all distances are infinity (INT_MAX)
    vector<int> dist(V, INT_MAX);

    // Tracks whether the shortest distance to a node is finalized
    // Initially, no node has been visited
    vector<bool> visited(V, false);

    // Stores the previous node in the shortest path
    // Initially, every node has no known parent
    vector<int> parent(V, -1);

    // Distance of source node to itself is alwayss 0
    dist[src] = 0;


    // Repeat until we have processed all possible nodes
    for (int count = 0; count < V - 1; count++)
    {
        // Stores the node with the smallest unvisited distance
        int u = -1;

        // Search for the unvisited node with minimum distance
        for (int i = 0; i < V; i++)
        {
            if (!visited[i] &&
                (u == -1 || dist[i] < dist[u]))
            {
                u = i;
            }
        }

        // Stop if no reachable unvisited node remains
        if (u == -1 || dist[u] == INT_MAX)
            break;

        // Mark the selected node as visited
        visited[u] = true;

        // Examine every possible neighbor of the selected node
        for (int v = 0; v < V; v++)
        {
            // Check whether an edge exists and the neighbor is unvisited
            if (graph[u][v] != 0 && !visited[v])
            {
                // Calculate the distance through the current node
                int newDist = dist[u] + graph[u][v];

                // If this route is shorter, update the distance
                if (newDist < dist[v])
                {
                    dist[v] = newDist;

                    // Remember how we reached this neighbor
                    parent[v] = u;
                }
            }
        }

    }

    // Check to see if the destination is unreachable
    // prevents program to take a path that doesn't exist
    if (dist[V - 1] == INT_MAX){
    cout << "Destination is unreachable" << endl;
    // returning an empty array/vector if node is unreachable
    return{};
    }
    // Print the shortest distance to the destination 
    cout << "Shortest distance: " << dist[V - 1] << endl;


    // Store the shortest path
    vector<int> path;

    // Follow parent pointers backwards, starting from destination
    for (int node = V - 1; node != -1; node = parent[node])
    {
        path.push_back(node);
    }

    reverse(path.begin(), path.end());
    return path;

    // Print the path in the correct direction
    cout << "Shortest path: ";

    for (int i = path.size() - 1; i >= 0; i--)
    {
        cout << path[i];

        if (i != 0)
            cout << " -> ";
    }

    cout << endl;
}

void FailLink(NodeContainer nodes, NetDeviceContainer link, NetDeviceContainer d02, Ipv4InterfaceContainer i02, shared_ptr<vector<vector<int>>> graph)
{
    Ptr<Ipv4> ipv4Node0 = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4Node1 = nodes.Get(1)->GetObject<Ipv4>();

    uint32_t interface0 =
        ipv4Node0->GetInterfaceForDevice(link.Get(0));

    uint32_t interface1 =
        ipv4Node1->GetInterfaceForDevice(link.Get(1));

    ipv4Node0->SetDown(interface0);
    ipv4Node1->SetDown(interface1);

    cout << "\nLINK FAILURE: Node 0 - Node 1 at " << Simulator::Now().GetSeconds() << " seconds\n";

    // Remove the failed link from our Dijkstra graph
    (*graph)[0][1] = 0;
    (*graph)[1][0] = 0;
    vector <int> path = dijkstra(*graph, 0);
    int nextHop = path[1];

    // Access Node 0's static routing table
Ipv4StaticRoutingHelper routingHelper;
Ptr<Ipv4StaticRouting> static0 =
    routingHelper.GetStaticRouting(ipv4Node0);

// Remove the old route to Node 3
Ipv4Address destination("10.1.2.2");

for (uint32_t i = static0->GetNRoutes(); i > 0; )
{
    --i;
    Ipv4RoutingTableEntry route = static0->GetRoute(i);

    if (route.IsHost() && route.GetDest() == destination)
    {
        static0->RemoveRoute(i);
        break;
    }
}

// Install the route selected by Dijkstra
if (nextHop == 2)
{
    uint32_t newInterface =
        ipv4Node0->GetInterfaceForDevice(d02.Get(0));

    static0->AddHostRouteTo(
        destination,
        i02.GetAddress(1),
        newInterface
    );

    cout << "Dijkstra routing updated: Node 0 -> Node 2 -> Node 3"
         << endl;
}

    Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

    
}

void CollectMetrics(Ptr<FlowMonitor> monitor,Ptr<Ipv4FlowClassifier> classifier, double failureTime){
    auto flowStats = monitor->GetFlowStats();
    static map<FlowId, FlowMonitor::FlowStats> previousStats;

    for (const auto &flow : flowStats){
    auto stats = flow.second;
    auto tuple = classifier->FindFlow(flow.first);

    if (tuple.destinationPort != 9){
        continue;
    }
    auto previous = previousStats[flow.first];

    uint64_t txPackets = stats.txPackets - previous.txPackets;
    uint64_t rxPackets = stats.rxPackets - previous.rxPackets;
    uint64_t rxBytes = stats.rxBytes - previous.rxBytes;
    Time delay = stats.delaySum - previous.delaySum;

    double pdr = txPackets > 0 ? (double)rxPackets / txPackets * 100.0 : 0.0;

    double avgDelayMs = rxPackets > 0 ? delay.GetSeconds() / rxPackets * 1000.0 : 0.0;

    double throughputMbps = rxBytes * 8.0 / 1000000.0;

    double sampleTime = Simulator::Now().GetSeconds();

    int futureFailure =(failureTime > sampleTime && failureTime <= sampleTime + 2.0) ? 1 : 0;

    bool fileExists =
    ifstream("time_window_dataset.csv").good();

    ofstream dataset("time_window_dataset.csv", ios::app);

    if (!fileExists){
    dataset << "sampleTime,txPackets,rxPackets,rxBytes,"
            << "pdr,avgDelayMs,throughputMbps,futureFailure,failureTime\n";
}

dataset << sampleTime << ","
        << txPackets << ","
        << rxPackets << ","
        << rxBytes << ","
        << pdr << ","
        << avgDelayMs << ","
        << throughputMbps << ","
        << futureFailure << ","
        << failureTime << "\n";

    previousStats[flow.first] = stats;

    }
    if (Simulator::Now().GetSeconds() < 12.0){
    Simulator::Schedule(
        Seconds(1.0),
        &CollectMetrics,
        monitor,
        classifier,
        failureTime
    );
}
}

void ChangeLinkDelay(NetDeviceContainer link, Time newDelay)
{
    Ptr<PointToPointChannel> channel =
        DynamicCast<PointToPointChannel>(
            link.Get(0)->GetChannel()
        );

    channel->SetAttribute("Delay", TimeValue(newDelay));

    cout << "Link delay changed to "
         << newDelay.GetMilliSeconds() << " ms at "
         << Simulator::Now().GetSeconds() << " seconds"
         << endl;
}

int main(int argc, char *argv[])
{
    //setting a default value for time after which failure will occur
    double failureTime = 5.0;
    // introducing cmd arguments to change time to showcase real time dynamic failures
    CommandLine cmd;
    cmd.AddValue("failureTime", "Time at which the link fails", failureTime);
    cmd.Parse(argc, argv);
    auto graph = make_shared<vector<vector<int>>>(4, vector<int>(4, 0));

    AddEdge(*graph, 0, 1, 1);
    AddEdge(*graph, 1, 3, 1);
    AddEdge(*graph, 0, 2, 10);
    AddEdge(*graph, 2, 3, 10);


    bool fileExists = ifstream("network_dataset.csv").good();
    ofstream dataset("network_dataset.csv", ios::app);
    if(!fileExists){
        dataset << "simulationEndTime,failureTime,flowId,txPackets,rxPackets,lostPackets,"
               "pdr,avgDelayMs,throughputMbps\n";
    }

    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // 1. Create four nodes
    NodeContainer nodes;
    nodes.Create(4);

    // 2. Configure point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // 3. p2p helper pointer installs nodes 13,02 and 23
    NetDeviceContainer d13 = p2p.Install(nodes.Get(1), nodes.Get(3));
    NetDeviceContainer d02 = p2p.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));

    PointToPointHelper p2p01;
    p2p01.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p01.SetChannelAttribute("Delay", StringValue("2ms"));

    // p2p01 helper pointer installs 01 node
    NetDeviceContainer d01 = p2p01.Install(nodes.Get(0), nodes.Get(1));

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

    // 6. Make Path B less preferable so traffic initially uses Path A
Ptr<Ipv4> ipv4Node0 = nodes.Get(0)->GetObject<Ipv4>();
Ptr<Ipv4> ipv4Node2 = nodes.Get(2)->GetObject<Ipv4>();

uint32_t interface02 = ipv4Node0->GetInterfaceForDevice(d02.Get(0));
uint32_t interface23 = ipv4Node2->GetInterfaceForDevice(d23.Get(0));

ipv4Node0->SetMetric(interface02, 10);
ipv4Node2->SetMetric(interface23, 10);

// Calculate routing tables
Ipv4GlobalRoutingHelper::PopulateRoutingTables();

Ipv4StaticRoutingHelper routingHelper;

Ptr<Ipv4StaticRouting> static0 =
    routingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());

Ptr<Ipv4StaticRouting> static2 =
    routingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());

Ptr<Ipv4StaticRouting> static3 =
    routingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());

// Get interface indices
uint32_t if01 = nodes.Get(0)->GetObject<Ipv4>()->GetInterfaceForDevice(d01.Get(0));
uint32_t if13 = nodes.Get(3)->GetObject<Ipv4>()->GetInterfaceForDevice(d13.Get(1));
uint32_t if23Node2 = nodes.Get(2)->GetObject<Ipv4>()->GetInterfaceForDevice(d23.Get(0));
uint32_t if23Node3 = nodes.Get(3)->GetObject<Ipv4>()->GetInterfaceForDevice(d23.Get(1));

// Node 0 -> Node 3, initially through Node 1
static0->AddHostRouteTo(i13.GetAddress(1), i01.GetAddress(1), if01);

// Node 3 -> Node 0, return through Node 1
static3->AddHostRouteTo(i01.GetAddress(0), i13.GetAddress(0), if13);

// Node 2 -> Node 3, needed for the alternate path
static2->AddHostRouteTo(i13.GetAddress(1), i23.GetAddress(1), if23Node2);

// Node 3 -> Node 0's alternate address, through Node 2
static3->AddHostRouteTo(i02.GetAddress(0), i23.GetAddress(0), if23Node3);


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
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

    // Gradually degrade the Node 0 - Node 1 link
    Simulator::Schedule(Seconds(4.0), &ChangeLinkDelay, d01, MilliSeconds(5));

    Simulator::Schedule( Seconds(5.0), &ChangeLinkDelay, d01, MilliSeconds(10));

    Simulator::Schedule(Seconds(6.0), &ChangeLinkDelay, d01, MilliSeconds(20));

// Schedule the actual link failure
Simulator::Schedule(Seconds(failureTime),&FailLink, nodes, d01, d02, i02, graph
);
// Start collecting metrics at 2 seconds
    Simulator::Schedule(
    Seconds(2.0),
    &CollectMetrics,
    monitor,
    classifier,
    failureTime
);

    Simulator::Stop(Seconds(13.0));

    cout << "Starting 4-node diamond network simulation..." << endl;

    Simulator::Run();
    monitor->CheckForLostPackets();
    auto flowStats = monitor->GetFlowStats();

    for (const auto &flow : flowStats)
{
    auto stats = flow.second;

    auto tuple = classifier->FindFlow(flow.first);
    if (tuple.destinationPort != 9){
    continue;
    }  

    cout << "Flow " << flow.first << ": "
     << tuple.sourceAddress << " -> "
     << tuple.destinationAddress
     << " | Source port: " << tuple.sourcePort
     << " | Destination port: " << tuple.destinationPort
     << endl;

    double pdr = 0.0;
    double avgDelayMs = 0.0;
    double throughputMbps = 0.0;

    if (stats.txPackets > 0)
    {
        pdr = (double)stats.rxPackets / stats.txPackets * 100.0;
    }

    if (stats.rxPackets > 0)
    {
        avgDelayMs =
            stats.delaySum.GetSeconds() / stats.rxPackets * 1000.0;

        double duration =
            (stats.timeLastRxPacket - stats.timeFirstTxPacket).GetSeconds();

        if (duration > 0)
        {
            throughputMbps =
                (stats.rxBytes * 8.0) / duration / 1000000.0;
        }
    }

    dataset << Simulator::Now().GetSeconds() << ","
            << failureTime << ","
            << flow.first << ","
            << stats.txPackets << ","
            << stats.rxPackets << ","
            << stats.lostPackets << ","
            << pdr << ","
            << avgDelayMs << ","
            << throughputMbps << "\n";
}

    
dataset.close();
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