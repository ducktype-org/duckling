#include<iostream>
#include<vector>
#include<queue>
#include<utility>

struct Node {
    std::vector<Node> edges;
    bool visited = false;
    unsigned int distance = 999;
};

void DFS(Node current) {
    current.visited = true;
    for (auto neighbour: current.edges) {
        if (!neighbour.visited) DFS(neighbour);
    }
}

void BFS(Node start) {
    start.visited = true;
    start.distance = 0;

    std::queue<std::pair<Node, unsigned int>> q;
    q.push(make_pair(start, 1));

    while (!q.empty()) {
        pair<Node, unsigned int> current = q.front();
        q.pop();
        current.first.visited = true;
        current.first.distance = current.second;

        for (auto neighbour: current.first.edges) {
            if (!neighbour.visited) {
                q.push(make_pair(neighbour, current.second+1));
            }
        }
    }
}
