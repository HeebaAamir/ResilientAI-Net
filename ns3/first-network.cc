#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <vector>
#include <memory>
#include <climits>

using namespace std;
using namespace ns3;
NS_LOG_COMPONENT_DEFINE("FirstNetwork");

void AddEdge(vector<vector<int>>& graph, int u, int v, int weight)
{
    graph[u][v] = weight;
    graph[v][u] = weight;
}

void dijkstra(vector<vector<int>> graph, int src)
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
    return;
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

void FailLink(NodeContainer nodes, NetDeviceContainer link, shared_ptr<vector<vector<int>>> graph)
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
    dijkstra(*graph, 0);

    Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

    
}


int main()
{
    auto graph = make_shared<vector<vector<int>>>(4, vector<int>(4, 0));

    AddEdge(*graph, 0, 1, 1);
    AddEdge(*graph, 1, 3, 1);
    AddEdge(*graph, 0, 2, 10);
    AddEdge(*graph, 2, 3, 10);

    ofstream dataset("network_dataset.csv");
    dataset << "time,flowId,txPackets,rxPackets,lostPackets\n";
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
    Simulator::Schedule(Seconds(5.0), &FailLink, nodes, d01, graph);
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Simulator::Stop(Seconds(13.0));

    cout << "Starting 4-node diamond network simulation..."
              << endl;

    Simulator::Run();
    monitor->CheckForLostPackets();
    auto flowStats = monitor->GetFlowStats();

    for (const auto &flow : flowStats)
    {
    dataset << Simulator::Now().GetSeconds() << ","
            << flow.first << ","
            << flow.second.txPackets << ","
            << flow.second.rxPackets << ","
            << flow.second.lostPackets << "\n";
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