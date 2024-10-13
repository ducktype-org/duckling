#include <iostream>
#include <vector>

// Funkcja do wykonywania DFS
void DFS(int node, std::vector<std::vector<int>>& adj, std::vector<bool>& visited) {
    // Oznacz bieżący wierzchołek jako odwiedzony
    visited[node] = true;
    std::cout << node << " ";  // Możemy tutaj wykonać jakąś akcję, np. wypisać wierzchołek

    // Rekurencyjnie odwiedź wszystkie nieodwiedzone sąsiady bieżącego wierzchołka
    for (int neighbor : adj[node]) {
        if (!visited[neighbor]) {
            DFS(neighbor, adj, visited);
        }
    }
}

int main() {
    // Liczba wierzchołków (vertices) i krawędzi (edges)
    int vertices, edges;
    std::cout << "Enter the number of vertices and the number of edges: ";
    std::cin >> vertices >> edges;

    // Lista sąsiedztwa do reprezentacji grafu
    std::vector<std::vector<int>> adj(vertices);

    // Odczytujemy krawędzie
    std::cout << "enter edges (one edge in one line):\n";
    for (int i = 0; i < edges; i++) {
        int u, v;
        std::cin >> u >> v;
        if (u < 0 || u >= vertices || v < 0 || v >= vertices) {
            std::cerr << "Invalid edge: " << u << " " << v << "\n";
            return 1;
        }
        adj[u].push_back(v);
        adj[v].push_back(u);  // Ponieważ graf jest nieskierowany, dodajemy obie krawędzie
    }

    // Tablica odwiedzin (false oznacza, że wierzchołek nie został jeszcze odwiedzony)
    std::vector<bool> visited(vertices, false);

    // Wykonanie DFS dla całego grafu (na wypadek grafu nie spójnego)
    for (int i = 0; i < vertices; i++) {
        if (!visited[i]) {
            std::cout << "DFS begins from the vertex " << i << ": ";
            DFS(i, adj, visited);
            std::cout << "\n";
        }
    }

    return 0;
}
