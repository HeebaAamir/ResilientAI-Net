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
#include <sstream>

using namespace std;
using namespace ns3;
NS_LOG_COMPONENT_DEFINE("FirstNetwork");

// Describes a network link before it is installed
struct LinkSpec
{
    uint32_t nodeA;  // First connected node
    uint32_t nodeB;  // Second connected node
    int weight;      // Routing cost of this link
};

// Stores all the information associated with a network link
struct LinkInfo
{
    // Identifies the two nodes connected by this link
    uint32_t nodeA;
    uint32_t nodeB;

    // Stores the network devices installed at both endpoints
    NetDeviceContainer devices;

    // Stores the IP interfaces assigned to both endpoints
    Ipv4InterfaceContainer interfaces;

    // Cost of using this link during shortest-path calculation
    int weight; 
};

void AddEdge(vector<vector<int>>& graph, int u, int v, int weight)
{
    graph[u][v] = weight;
    graph[v][u] = weight;
}

vector <int> dijkstra(vector<vector<int>> graph, int src, int destination)
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
    if (dist[destination] == INT_MAX){
    cout << "Destination is unreachable" << endl;
    // returning an empty array/vector if node is unreachable
    return{};
    }
    // Print the shortest distance to the destination 
    cout << "Shortest distance: " << dist[destination] << endl;


    // Store the shortest path
    vector<int> path;

    // Follow parent pointers backwards, starting from destination
    for (int node = destination; node != -1; node = parent[node])
    {
        path.push_back(node);
    }

    reverse(path.begin(), path.end());
    return path;

}

// Handles link failure and updates the source's route using dijkstra 
void FailLink(NodeContainer nodes, NetDeviceContainer link, vector<LinkInfo> links, shared_ptr<vector<vector<int>>> graph, uint32_t failedNodeA, uint32_t failedNodeB, uint32_t sourceNode, uint32_t destinationNode, Ipv4Address destination)
{
    
    // Get the IPv4 objects of both nodes connected by the failed link
    Ptr<Ipv4> ipv4NodeA =
        nodes.Get(failedNodeA)->GetObject<Ipv4>();

    Ptr<Ipv4> ipv4NodeB =
        nodes.Get(failedNodeB)->GetObject<Ipv4>();

    // Find the interface indices associated with the failed link
    uint32_t interfaceA =
        ipv4NodeA->GetInterfaceForDevice(link.Get(0));

    uint32_t interfaceB =
        ipv4NodeB->GetInterfaceForDevice(link.Get(1));

    // Disable both interfaces to simulate the physical link failure
    ipv4NodeA->SetDown(interfaceA);
    ipv4NodeB->SetDown(interfaceB);

    cout << "\nLINK FAILURE: Node "
         << failedNodeA << " - Node " << failedNodeB
         << " at " << Simulator::Now().GetSeconds()
         << " seconds\n";

    // Remove the failed link from the graph in both directions
    (*graph)[failedNodeA][failedNodeB] = 0;
    (*graph)[failedNodeB][failedNodeA] = 0;

    // Calculate the shortest available path from source to destination
    vector<int> path =
        dijkstra(*graph, sourceNode, destinationNode);

    // Stop if no valid path to the destination exists
    if (path.size() < 2)
    {
        cout << "ERROR: No valid alternate path found!" << endl;
        return;
    }

    // The second node in the path is the next hop
    uint32_t nextHop = path[1];

    // Find the physical link connecting the source to the selected next hop
    const LinkInfo* selectedLink = nullptr;

    for (const auto& candidate : links)
    {
        if ((candidate.nodeA == sourceNode &&
             candidate.nodeB == nextHop) ||
            (candidate.nodeA == nextHop &&
             candidate.nodeB == sourceNode))
        {
            selectedLink = &candidate;
            break;
        }
    }

    // Stop if the required outgoing link cannot be found
    if (selectedLink == nullptr)
    {
        cout << "ERROR: Link to selected next hop was not found!"
             << endl;
        return;
    }

    // Determine which device and IP interface belong to the source node
    uint32_t sourceIndex =
        (selectedLink->nodeA == sourceNode) ? 0 : 1;

    uint32_t nextHopIndex = 1 - sourceIndex;

    // Get the IPv4 object of the source node
    Ptr<Ipv4> ipv4Source =
        nodes.Get(sourceNode)->GetObject<Ipv4>();

    // Find the outgoing interface on the source node
    uint32_t newInterface =
        ipv4Source->GetInterfaceForDevice(
            selectedLink->devices.Get(sourceIndex)
        );

    // Get the gateway IP address of the next-hop node
    Ipv4Address gateway =
        selectedLink->interfaces.GetAddress(nextHopIndex);

    // Access the static routing table of the source node
    Ipv4StaticRoutingHelper routingHelper;

    Ptr<Ipv4StaticRouting> staticSource =
        routingHelper.GetStaticRouting(ipv4Source);

    // Remove the previous host route to the destination
    for (uint32_t i = staticSource->GetNRoutes(); i > 0; )
    {
        --i;

        Ipv4RoutingTableEntry route =
            staticSource->GetRoute(i);

        if (route.IsHost() && route.GetDest() == destination)
        {
            staticSource->RemoveRoute(i);
            break;
        }
    }

    // Install a new route through the next hop selected by Dijkstra
    staticSource->AddHostRouteTo(
        destination,
        gateway,
        newInterface
    );

    // Display every node in the complete selected route
    cout << "Dijkstra routing updated: ";

    for (size_t i = 0; i < path.size(); i++){
    cout << "Node " << path[i];

    if (i + 1 < path.size()){
        cout << " -> ";}
    }

cout << endl;

    // Recalculate the global routing tables after the topology change
    Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

}

void CollectMetrics(Ptr<FlowMonitor> monitor,Ptr<Ipv4FlowClassifier> classifier, double failureTime, uint32_t failureNodeA, uint32_t failureNodeB, 
    double secondFailureTime, uint32_t secondFailureNodeA, uint32_t secondFailureNodeB, uint32_t runId, double simulationEndTime, double trafficInterval){
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
    if (txPackets == 0){
    previousStats[flow.first] = stats;
    continue;
    }

    Time delay = stats.delaySum - previous.delaySum;

    double pdr = txPackets > 0 ? (double)rxPackets / txPackets * 100.0 : 0.0;

    double avgDelayMs = rxPackets > 0 ? delay.GetSeconds() / rxPackets * 1000.0 : 0.0;

    double throughputMbps = rxBytes * 8.0 / 1000000.0;

    double sampleTime = Simulator::Now().GetSeconds();

    // Label the sample positive if either failure is within the next 2 seconds
    bool firstFailureUpcoming = failureTime > sampleTime && failureTime <= sampleTime + 2.0;

    bool secondFailureUpcoming = secondFailureTime > sampleTime && secondFailureTime <= sampleTime + 2.0;

    int futureFailure = (firstFailureUpcoming || secondFailureUpcoming) ? 1 : 0;

    bool fileExists =
    ifstream("multi_failure_dataset_v3.csv").good();

    ofstream dataset("multi_failure_dataset_v3.csv", ios::app);

    //Write the header only when creating the file
    if (!fileExists){
    dataset << "runId,sampleTime,flowId,txPackets,rxPackets,rxBytes,"
            << "pdr,avgDelayMs,throughputMbps,futureFailure,"
            << "failureTime,failureNodeA,failureNodeB,"
            << "secondFailureTime,secondFailureNodeA,secondFailureNodeB,"
            << "trafficInterval\n";
}

dataset << runId << ","
        << sampleTime << ","
        << flow.first << ","
        << txPackets << ","
        << rxPackets << ","
        << rxBytes << ","
        << pdr << ","
        << avgDelayMs << ","
        << throughputMbps << ","
        << futureFailure << ","
        << failureTime << ","
        << failureNodeA << ","
        << failureNodeB << ","
        << secondFailureTime<< ","
        << secondFailureNodeA << ","
        << secondFailureNodeB << ","
        << trafficInterval << "\n";

    previousStats[flow.first] = stats;

    }
    if (Simulator::Now().GetSeconds() < simulationEndTime){
    Simulator::Schedule(
        Seconds(1.0),
        &CollectMetrics,
        monitor,
        classifier,
        failureTime,
        failureNodeA,
        failureNodeB,
        secondFailureTime,
        secondFailureNodeA,
        secondFailureNodeB,
        runId,
        simulationEndTime,
        trafficInterval
    );
}
}

// Finds the highest run ID already stored in the dataset
// and returns the next available ID.
uint32_t GetNextRunId(const string& filename)
{
    ifstream input(filename);
    string line;
    uint32_t maxRunId = 0;

    // Start from 1 if the dataset file does not exist yet
    if (!input.is_open())
    {
        return 1;
    }

    // Skip the CSV header
    getline(input, line);

    // Read the run ID from the first column of every data row
    while (getline(input, line))
    {
        string firstColumn;
        stringstream row(line);

        if (getline(row, firstColumn, ',') && !firstColumn.empty())
        {
            uint32_t currentRunId = stoul(firstColumn);
            maxRunId = max(maxRunId, currentRunId);
        }
    }

    return maxRunId + 1;
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
    double degradationInterval = 1.0;

    // Endpoints of the link that will fail
    uint32_t failureNodeA = 0;
    uint32_t failureNodeB = 1;

    // Endpoints and time for the second link failure
    double secondFailureTime = 7.0;
    uint32_t secondFailureNodeA = 0;
    uint32_t secondFailureNodeB = 2;

    uint32_t degradationSteps = 3;
    uint32_t initialDelayMs = 5;
    uint32_t delayIncrementMs = 5;
    // How long the simulation continues after the link fails
    double postFailureDuration = 5.0;

    // Configure the traffic endpoints
    uint32_t sourceNode = 0;
    uint32_t destinationNode = 5;

    // Time between consecutive UDP packets
    double trafficInterval = 0.1;

    // introducing cmd arguments to change time to showcase real time dynamic failures
    CommandLine cmd;
    cmd.AddValue("failureTime", "Time at which the link fails", failureTime);
    cmd.AddValue("degradationInterval", "Interval between degradation events",degradationInterval);
    cmd.AddValue("failureNodeA", "First endpoint of the failing link", failureNodeA);
    cmd.AddValue("failureNodeB", "Second endpoint of the failing link", failureNodeB);
    cmd.AddValue("degradationSteps", "Number of degradation events", degradationSteps);
    cmd.AddValue("initialDelayMs", "Initial degraded link delay", initialDelayMs);
    cmd.AddValue("delayIncrementMs", "Delay increase at each step", delayIncrementMs);
    // Allow the traffic endpoints to be selected from the command line
    cmd.AddValue("sourceNode", "Source node for network traffic", sourceNode);
    cmd.AddValue("destinationNode", "Destination node for network traffic", destinationNode);
    cmd.AddValue("trafficInterval", "Time between consecutive UDP packets", trafficInterval);
    cmd.AddValue("secondFailureTime", "Time at which the second link fails", secondFailureTime);
    cmd.AddValue("secondFailureNodeA", "First endpoint of the second failing link", secondFailureNodeA);
    cmd.AddValue("secondFailureNodeB", "Second endpoint of the second failing link", secondFailureNodeB);
    cmd.Parse(argc, argv);
    double simulationEndTime = secondFailureTime + postFailureDuration;

    // Validating simulation parameters before starting
    if (failureTime <= 0 ||
    degradationInterval <= 0 ||
    degradationSteps == 0 ||
    postFailureDuration <= 0 ||
    initialDelayMs == 0 ||
    delayIncrementMs == 0) {

    cerr << "ERROR: Invalid simulation parameters!" << endl;
    return 1;
    }

    // Ensureingall degradation events happen before the failure
    if (failureTime <= degradationInterval * degradationSteps) {
    cerr << "ERROR: Failure time is too early for the degradation schedule!"
         << endl;
    return 1;
    }

    // Ensure the second failure occurs after the first
    if (secondFailureTime <= failureTime){
    cerr << "ERROR: Second failure must occur after the first!"
         << endl;
    return 1;
    }


    bool fileExists = ifstream("network_dataset.csv").good();
    ofstream dataset("network_dataset.csv", ios::app);
    if(!fileExists){
        dataset << "simulationEndTime,failureTime,flowId,txPackets,rxPackets,lostPackets,"
               "pdr,avgDelayMs,throughputMbps\n";
    }

    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    
    // Configure the number of nodes and traffic endpoints
    uint32_t numNodes = 6;

    // Validate the selected traffic endpoints
    if (sourceNode >= numNodes ||
    destinationNode >= numNodes ||
    sourceNode == destinationNode){
    cerr << "ERROR: Invalid source or destination node!" << endl;
    return 1;
    }

    // Validate the selected failure link endpoints
    if (failureNodeA >= numNodes || failureNodeB >= numNodes || failureNodeA == failureNodeB){
    cerr << "ERROR: Invalid failure link endpoints!" << endl;
    return 1;
    }

    // Validate the second failure link endpoints
    if (secondFailureNodeA >= numNodes || secondFailureNodeB >= numNodes || secondFailureNodeA == secondFailureNodeB){
    cerr << "ERROR: Invalid second failure link endpoints!" << endl;
    return 1;
    }

    // Prevent the same link from failing twice
    if ((failureNodeA == secondFailureNodeA &&
     failureNodeB == secondFailureNodeB) ||
    (failureNodeA == secondFailureNodeB &&
     failureNodeB == secondFailureNodeA)){
    cerr << "ERROR: Both failures cannot target the same link!" << endl;
    return 1;
    }

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Define the connections that make up our network topology
    vector<LinkSpec> topology = {
        {0, 1, 1},  // Primary route: Node 0 -> Node 1
        {1, 5, 1},  // Primary route: Node 1 -> Node 5

        {0, 2, 4},  // Alternate route: Node 0 -> Node 2
        {2, 3, 4},  // Alternate route: Node 2 -> Node 3
        {3, 5, 4},  // Alternate route: Node 3 -> Node 5

        {0, 4, 7},  // Third route: Node 0 -> Node 4
        {4, 5, 7}   // Third route: Node 4 -> Node 5
    };


    // Configure the common point-to-point link properties
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));


    // Install Internet stack BEFORE installing links
    InternetStackHelper internet;
    internet.Install(nodes);

    // Stores all installed links and their assigned IP interfaces
    vector<LinkInfo> links;

    // Install every link described in the topology
    for (uint32_t i = 0; i < topology.size(); i++)
    {
        const auto& spec = topology[i];

        // Connect the two nodes specified by this topology entry
        NetDeviceContainer devices =
            p2p.Install(nodes.Get(spec.nodeA), nodes.Get(spec.nodeB));

        // Give each physical link its own /24 IP subnet
        Ipv4AddressHelper address;
        uint32_t subnetNumber = i + 1;

        address.SetBase(
            Ipv4Address(0x0A010000 + (subnetNumber << 8)),
            Ipv4Mask("255.255.255.0")
        );

        // Assign IP addresses to both ends of the link
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Save all information about this installed link
        links.push_back({
            spec.nodeA,
            spec.nodeB,
            devices,
            interfaces,
            spec.weight
        });
    }

    // Find a destination IP address on a link that will remain active
    Ipv4Address serverAddress;
    bool serverAddressFound = false;

    for (const auto& link : links){
    // Check whether this link is scheduled for the first failure
    bool firstFailureLink =
        (link.nodeA == failureNodeA && link.nodeB == failureNodeB) ||
        (link.nodeA == failureNodeB && link.nodeB == failureNodeA);

    // Check whether this link is scheduled for the second failure
    bool secondFailureLink =
        (link.nodeA == secondFailureNodeA &&
         link.nodeB == secondFailureNodeB) ||
        (link.nodeA == secondFailureNodeB &&
         link.nodeB == secondFailureNodeA);

    // Skip links that are scheduled to fail
    if (firstFailureLink || secondFailureLink)
    {
        continue;
    }

    // Select an active interface belonging to the destination
    if (link.nodeA == destinationNode)
    {
        serverAddress = link.interfaces.GetAddress(0);
        serverAddressFound = true;
        break;
    }

    if (link.nodeB == destinationNode)
    {
        serverAddress = link.interfaces.GetAddress(1);
        serverAddressFound = true;
        break;
    }
}

    // Stop if no surviving destination interface was found
    if (!serverAddressFound)
    {
        cerr << "ERROR: No active destination interface found!" << endl;
        return 1;
    }

    // Find the physical link selected for failure
    NetDeviceContainer failureLink;
    bool failureLinkFound = false;

    for (const auto& link : links){
    // Check both endpoint orders because links are undirected
    if ((link.nodeA == failureNodeA &&
         link.nodeB == failureNodeB) ||
        (link.nodeA == failureNodeB &&
         link.nodeB == failureNodeA)){
        failureLink = link.devices;
        failureLinkFound = true;
        break;
        }
    }

    // Stop if the requested failure link does not exist
    if (!failureLinkFound)
    {
    cerr << "ERROR: Selected failure link does not exist in topology!"
         << endl;
    return 1;
    }

    // Find the physical link selected for the second failure  
    NetDeviceContainer secondFailureLink;
    bool secondFailureLinkFound = false;

    for (const auto& link : links){
    // Check both endpoint orders because links are undirected
    if ((link.nodeA == secondFailureNodeA &&
         link.nodeB == secondFailureNodeB) ||
        (link.nodeA == secondFailureNodeB &&
         link.nodeB == secondFailureNodeA)){
        secondFailureLink = link.devices;
        secondFailureLinkFound = true;
        break;
        }
    } 

    // Stop if the second requested failure link does not exist
    if (!secondFailureLinkFound){
    cerr << "ERROR: Second failure link does not exist in topology!"
         << endl;
    return 1;
    }

    // Create an adjacency matrix using the total number of network nodes
    auto graph = make_shared<vector<vector<int>>>(
    nodes.GetN(),
    vector<int>(nodes.GetN(), 0)
    );

    // Build the graph using the registered physical links
    for (const auto& link : links){
    AddEdge(*graph, link.nodeA, link.nodeB, link.weight);
    }

    // Configure routing metrics for every physical link
    for (const auto& link : links){
    // Get the IPv4 objects of both connected nodes
    Ptr<Ipv4> ipv4A = nodes.Get(link.nodeA)->GetObject<Ipv4>();

    Ptr<Ipv4> ipv4B = nodes.Get(link.nodeB)->GetObject<Ipv4>();

    // Find the interface belonging to this link on each node
    uint32_t interfaceA = ipv4A->GetInterfaceForDevice(link.devices.Get(0));

    uint32_t interfaceB = ipv4B->GetInterfaceForDevice(link.devices.Get(1));

    // Apply the link's routing weight at both endpoints
    ipv4A->SetMetric(interfaceA, link.weight);
    ipv4B->SetMetric(interfaceB, link.weight);
    }

    // Calculate routing tables using the configured metrics
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 7. Install UDP Echo Server on Node 3
    UdpEchoServerHelper server(9);
    ApplicationContainer serverApp = server.Install(nodes.Get(destinationNode));

    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationEndTime));

    // 8. Install UDP Echo Client on Node 0
    UdpEchoClientHelper client(serverAddress, 9);

    // Generate more frequent traffic for meaningful network measurements
    client.SetAttribute("MaxPackets", UintegerValue(10000));
    client.SetAttribute("Interval", TimeValue(Seconds(trafficInterval)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(nodes.Get(sourceNode));

    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simulationEndTime));

    // 9. Run simulation
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

    // Assign a unique ID to this simulation execution
    uint32_t runId = GetNextRunId("multi_failure_dataset_v3.csv");
    cout << "Dataset run ID: " << runId << endl;

    // Gradually degrade the Node 0 - Node 1 link
    for (uint32_t i = 0; i < degradationSteps; i++){
    double eventTime = failureTime -
        (degradationSteps - i) * degradationInterval;

    uint32_t delayMs = initialDelayMs + i * delayIncrementMs;

    Simulator::Schedule(
        Seconds(eventTime),
        &ChangeLinkDelay,
        failureLink,
        MilliSeconds(delayMs)
    );
}
// Schedule the actual link failure
Simulator::Schedule(Seconds(failureTime),&FailLink, nodes, failureLink, links, graph,failureNodeA,failureNodeB,sourceNode,destinationNode,serverAddress);

// Schedule the 2nd link failure
Simulator::Schedule(Seconds(secondFailureTime), &FailLink, nodes, secondFailureLink, links, graph, secondFailureNodeA, secondFailureNodeB, sourceNode, destinationNode, serverAddress);

// Start collecting metrics at 2 seconds
    Simulator::Schedule(
    Seconds(2.0),
    &CollectMetrics,
    monitor,
    classifier,
    failureTime,
    failureNodeA,
    failureNodeB,
    secondFailureTime,
    secondFailureNodeA,
    secondFailureNodeB,
    runId,
    simulationEndTime,
    trafficInterval
);

    Simulator::Stop(Seconds(simulationEndTime + 1.0));

    cout << "Starting 6-node network simulation..." << endl;

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