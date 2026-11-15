// Network.h
// Network Failure Analyzer - FODS Course Project
// Shivam Gaikwad (2080153) and Jiteesh Ghodke (2080166)
//
// The network is stored as an undirected weighted graph.
// devices = nodes, cables = edges, latency (ms) = weight of edge
// We use adjacency list because a network has less links than n*n.

#ifndef NETWORK_H
#define NETWORK_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <stack>

using namespace std;

// one device in the network (router, switch, pc, server ...)
struct Device {
    string name;
    string type;
    bool working;
    int x, y;        // position on screen (only used by the web map)
};

// one cable between two devices
struct Link {
    int u, v;        // index of the two devices
    int latency;     // in ms, this is the weight for dijkstra
    bool working;
};

// we push every failure on a stack so that "undo" restores the last one
struct Failure {
    char kind;       // 'D' = device, 'L' = link
    int id;
};

// used for the impact ranking
struct ImpactScore {
    string name;
    int lost;        // how many devices lose connection if this one fails
};

// Disjoint Set Union (union find) - used to check connectivity quickly
// in the planned maintenance (binary search) feature
struct DSU {
    vector<int> parent, rnk;

    DSU(int n) {
        parent.resize(n);
        rnk.assign(n, 0);
        for (int i = 0; i < n; i++)
            parent[i] = i;
    }

    int find(int x) {
        if (parent[x] != x)
            parent[x] = find(parent[x]);    // path compression
        return parent[x];
    }

    void unite(int a, int b) {
        a = find(a);
        b = find(b);
        if (a == b)
            return;
        if (rnk[a] < rnk[b])               // union by rank
            swap(a, b);
        parent[b] = a;
        if (rnk[a] == rnk[b])
            rnk[a]++;
    }
};

class Network {
public:
    vector<Device> devices;
    vector<Link> links;
    vector< vector<int> > adj;    // adj[i] = ids of links connected to device i
    map<string, int> nameToId;    // to find a device by its name quickly
    stack<Failure> failStack;     // for undo
    vector<string> logs;
    string networkName;

    Network();

    // ---------- building the graph ----------
    void clearAll();
    bool loadFromFile(string fileName);
    bool saveToFile(string fileName);
    int addDevice(string name, string type, int x = -1, int y = -1);
    bool addLink(string a, string b, int latency);
    int getId(string name);
    int findLink(string a, string b);
    int otherEnd(int linkId, int node);
    bool canUse(int linkId);

    // ---------- failure simulation ----------
    bool failDevice(string name);
    bool failLink(string a, string b);
    bool restoreDevice(string name);
    bool restoreLink(string a, string b);
    bool undoLastFailure();
    void restoreAll();

    // ---------- algorithms ----------
    vector<int> bfs(int src);                                   // reachable devices
    int countComponents(vector<int> &compOf);                   // DFS
    vector<int> dijkstra(int src, int dest, int &totalCost);    // shortest path
    void allPaths(int src, int dest, vector< vector<int> > &paths, vector<int> &costs, int limit);
    void findCritical(vector<int> &critDevices, vector<int> &bridgeLinks);   // articulation points
    vector<ImpactScore> impactRanking();
    bool connectedWithout(vector<int> &plannedLinks, int k);     // DSU check
    int breakingPoint(vector<int> &plannedLinks);                // binary search

    // ---------- health numbers ----------
    int degree(int id);
    double density();

    // ---------- report ----------
    int workingDevices();
    int workingLinks();
    int largestGroup();
    double healthPercent();
    string severity();
    void addLog(string msg);
    void printNetwork();
    void printReport();

private:
    void dfs(int u, int compNo, vector<int> &compOf);
    void pathDfs(int u, int dest, vector<bool> &visited, vector<int> &path, int cost,
                 vector< vector<int> > &paths, vector<int> &costs, int limit);
    void tarjanDfs(int u, int parentLink, int &timer, vector<int> &disc, vector<int> &low,
                   vector<bool> &isCrit, vector<int> &bridgeLinks);
};

#endif
