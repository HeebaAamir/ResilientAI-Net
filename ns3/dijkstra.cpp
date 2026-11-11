#include <iostream>
#include <vector>
#include <climits>
using namespace std;

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

int main()
{   
    //dijkstra algorithm determines shortest path based on weights
    //implementing this using adjacency matrix
    //each row and column represents a node
    //value present at any point represents weight in goinf from one node to another

    const int V = 4;

    vector<vector<int>> graph = {
        {0, 1, 10, 0},
        {1, 0, 0, 1},
        {10, 0, 0, 10},
        {0, 1, 10, 0}
    };

    // Simulate failure of the link between Node 0 and Node 1
    graph[0][1] = 0;
    graph[1][0] = 0;

    // Find the shortest paths starting from Node 0
    dijkstra(graph, 0);

    return 0;
}