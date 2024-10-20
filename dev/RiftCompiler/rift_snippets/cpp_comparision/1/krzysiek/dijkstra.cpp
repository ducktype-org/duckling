// https://www.geeksforgeeks.org/dijkstras-shortest-path-algorithm-greedy-algo-7/

#include <iostream>
#include <queue>
#include <list>
#include <vector>

// Define INF as a large value to represent infinity
const int INF = 0x3f3f3f3f;

// Class representing a graph using adjacency list representation
class Graph {
    int V; // Number of vertices
    std::list<std::pair<int, int>> *adj; // Adjacency list

public:
    Graph(int V) {
        this->V = V;
        adj = new std::list<std::pair<int, int>>[V];
    }

    void addEdge(int u, int v, int w) {
        adj[u].push_back({v, w});
        adj[v].push_back({u, w}); // Since the graph is undirected
    }
    
    void shortestPath(int src) {
        // Create a priority queue to store vertices being processed
        // Priority queue sorted by the first element of the pair (distance)
        std::priority_queue<std::pair<int, int>> pq;

        // Create a vector to store distances and initialize all distances as INF
        std::vector<int> dist(V, INF);

        // Insert source into priority queue and initialize its distance as 0
        pq.push({0, src});
        dist[src] = 0;

        // Process the priority queue
        while (!pq.empty()) {
            // Get the vertex with the minimum distance
            int u = pq.top().second;
            pq.pop();

            // Iterate through all adjacent vertices of the current vertex
            for (auto &neighbor : adj[u]) {
                int v = neighbor.first;
                int weight = neighbor.second;

                // If a shorter path to v is found
                if (dist[v] > dist[u] + weight) {
                    // Update distance and push new distance to the priority queue
                    dist[v] = dist[u] + weight;
                    pq.push({-dist[v], v});
                }
            }
        }

        // Print the shortest distances
        std::cout << "Vertex Distance from Source\n";
        for (int i = 0; i < V; ++i)
            std::cout << i << " \t\t " << dist[i] << "\n";
    } // Function to print shortest path from source
};
