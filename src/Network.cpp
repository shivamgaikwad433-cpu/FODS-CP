// Network.cpp
// All graph functions of the Network Failure Analyzer.
// DSA used: adjacency list, BFS (queue), DFS (recursion), Dijkstra (priority queue / min heap),
// backtracking for alternate paths, Tarjan's algorithm for articulation points and bridges,
// stack for undo, bubble sort for ranking, DSU + binary search for planned maintenance.

#include "Network.h"
#include <fstream>
#include <sstream>
#include <queue>
#include <ctime>
#include <climits>
#include <algorithm>

Network::Network() {
    networkName = "Untitled Network";
}

void Network::clearAll() {
    devices.clear();
    links.clear();
    adj.clear();
    nameToId.clear();
    logs.clear();
    while (!failStack.empty())
        failStack.pop();
}

// file format (see data folder):
// NAME <network name>
// DEVICE <name> <type> [x y]
// LINK <device1> <device2> <latency>
// lines starting with # are comments
bool Network::loadFromFile(string fileName) {
    ifstream fin(fileName.c_str());
    if (!fin) {
        cout << "Could not open file " << fileName << endl;
        return false;
    }
    clearAll();

    string line;
    while (getline(fin, line)) {
        if (line.size() == 0 || line[0] == '#')
            continue;

        stringstream ss(line);
        string cmd;
        ss >> cmd;

        if (cmd == "NAME") {
            getline(ss, networkName);
            if (networkName.size() > 0 && networkName[0] == ' ')
                networkName = networkName.substr(1);
        }
        else if (cmd == "DEVICE") {
            string name, type;
            int x = -1, y = -1;
            ss >> name >> type;
            if (!(ss >> x >> y)) {
                x = -1;
                y = -1;
            }
            addDevice(name, type, x, y);
        }
        else if (cmd == "LINK") {
            string a, b;
            int lat = 1;
            ss >> a >> b >> lat;
            addLink(a, b, lat);
        }
    }
    fin.close();

    logs.clear();
    addLog("Loaded network '" + networkName + "' with " + to_string(devices.size()) +
           " devices and " + to_string(links.size()) + " links");
    return true;
}

bool Network::saveToFile(string fileName) {
    ofstream fout(fileName.c_str());
    if (!fout)
        return false;

    fout << "# saved by Network Failure Analyzer" << endl;
    fout << "NAME " << networkName << endl;
    for (int i = 0; i < (int)devices.size(); i++) {
        fout << "DEVICE " << devices[i].name << " " << devices[i].type;
        if (devices[i].x >= 0)
            fout << " " << devices[i].x << " " << devices[i].y;
        fout << endl;
    }
    for (int i = 0; i < (int)links.size(); i++) {
        fout << "LINK " << devices[links[i].u].name << " " << devices[links[i].v].name
             << " " << links[i].latency << endl;
    }
    fout.close();
    addLog("Network saved to " + fileName);
    return true;
}

// returns the id of new device, or -1 if the name is already used
int Network::addDevice(string name, string type, int x, int y) {
    if (name == "" || nameToId.count(name) > 0)
        return -1;

    Device d;
    d.name = name;
    d.type = type;
    d.working = true;
    d.x = x;
    d.y = y;

    devices.push_back(d);
    adj.push_back(vector<int>());
    int id = devices.size() - 1;
    nameToId[name] = id;
    return id;
}

bool Network::addLink(string a, string b, int latency) {
    int u = getId(a);
    int v = getId(b);
    if (u == -1 || v == -1 || u == v || latency <= 0)
        return false;
    if (findLink(a, b) != -1)    // already connected
        return false;

    Link l;
    l.u = u;
    l.v = v;
    l.latency = latency;
    l.working = true;
    links.push_back(l);

    int id = links.size() - 1;
    adj[u].push_back(id);    // undirected graph so add to both sides
    adj[v].push_back(id);
    return true;
}

int Network::getId(string name) {
    if (nameToId.count(name) == 0)
        return -1;
    return nameToId[name];
}

int Network::findLink(string a, string b) {
    int u = getId(a);
    int v = getId(b);
    if (u == -1 || v == -1)
        return -1;
    for (int i = 0; i < (int)adj[u].size(); i++) {
        int id = adj[u][i];
        if (otherEnd(id, u) == v)
            return id;
    }
    return -1;
}

int Network::otherEnd(int linkId, int node) {
    if (links[linkId].u == node)
        return links[linkId].v;
    return links[linkId].u;
}

// a link can carry data only if the cable and both devices are working
bool Network::canUse(int linkId) {
    Link &l = links[linkId];
    return l.working && devices[l.u].working && devices[l.v].working;
}

// ================= FAILURE SIMULATION =================

bool Network::failDevice(string name) {
    int id = getId(name);
    if (id == -1 || !devices[id].working)
        return false;

    devices[id].working = false;
    Failure f;
    f.kind = 'D';
    f.id = id;
    failStack.push(f);

    int cut = workingDevices() - largestGroup();
    addLog("FAILED device " + name + " -> " + to_string(cut) + " device(s) cut off, severity " + severity());
    return true;
}

bool Network::failLink(string a, string b) {
    int id = findLink(a, b);
    if (id == -1 || !links[id].working)
        return false;

    links[id].working = false;
    Failure f;
    f.kind = 'L';
    f.id = id;
    failStack.push(f);

    int cut = workingDevices() - largestGroup();
    addLog("FAILED link " + a + " - " + b + " -> " + to_string(cut) + " device(s) cut off, severity " + severity());
    return true;
}

bool Network::restoreDevice(string name) {
    int id = getId(name);
    if (id == -1 || devices[id].working)
        return false;
    devices[id].working = true;
    addLog("RESTORED device " + name);
    return true;
}

bool Network::restoreLink(string a, string b) {
    int id = findLink(a, b);
    if (id == -1 || links[id].working)
        return false;
    links[id].working = true;
    addLog("RESTORED link " + a + " - " + b);
    return true;
}

// pops the stack until we find something that is still failed
// (it may have been restored manually before)
bool Network::undoLastFailure() {
    while (!failStack.empty()) {
        Failure f = failStack.top();
        failStack.pop();

        if (f.kind == 'D' && !devices[f.id].working) {
            return restoreDevice(devices[f.id].name);
        }
        if (f.kind == 'L' && !links[f.id].working) {
            return restoreLink(devices[links[f.id].u].name, devices[links[f.id].v].name);
        }
    }
    return false;
}

void Network::restoreAll() {
    for (int i = 0; i < (int)devices.size(); i++)
        devices[i].working = true;
    for (int i = 0; i < (int)links.size(); i++)
        links[i].working = true;
    while (!failStack.empty())
        failStack.pop();
    addLog("RESTORED everything, network back to normal");
}

// ================= ALGORITHMS =================

// BFS from src, returns all devices that can be reached
vector<int> Network::bfs(int src) {
    vector<int> result;
    if (src < 0 || src >= (int)devices.size() || !devices[src].working)
        return result;

    vector<bool> visited(devices.size(), false);
    queue<int> q;
    q.push(src);
    visited[src] = true;

    while (!q.empty()) {
        int u = q.front();
        q.pop();
        result.push_back(u);

        for (int i = 0; i < (int)adj[u].size(); i++) {
            int linkId = adj[u][i];
            int v = otherEnd(linkId, u);
            if (!visited[v] && canUse(linkId)) {
                visited[v] = true;
                q.push(v);
            }
        }
    }
    return result;
}

void Network::dfs(int u, int compNo, vector<int> &compOf) {
    compOf[u] = compNo;
    for (int i = 0; i < (int)adj[u].size(); i++) {
        int linkId = adj[u][i];
        int v = otherEnd(linkId, u);
        if (compOf[v] == -1 && canUse(linkId))
            dfs(v, compNo, compOf);
    }
}

// counts connected groups of working devices using DFS
// compOf[i] = group number of device i (-1 if device is down)
int Network::countComponents(vector<int> &compOf) {
    compOf.assign(devices.size(), -1);
    int count = 0;
    for (int i = 0; i < (int)devices.size(); i++) {
        if (devices[i].working && compOf[i] == -1) {
            dfs(i, count, compOf);
            count++;
        }
    }
    return count;
}

// Dijkstra using min heap (priority queue of pair<distance, node>)
// returns the path from src to dest, empty if no path
vector<int> Network::dijkstra(int src, int dest, int &totalCost) {
    vector<int> path;
    totalCost = -1;
    int n = devices.size();
    if (src < 0 || dest < 0 || src >= n || dest >= n)
        return path;
    if (!devices[src].working || !devices[dest].working)
        return path;

    vector<int> dist(n, INT_MAX);
    vector<int> parent(n, -1);
    priority_queue< pair<int, int>, vector< pair<int, int> >, greater< pair<int, int> > > pq;

    dist[src] = 0;
    pq.push(make_pair(0, src));

    while (!pq.empty()) {
        int d = pq.top().first;
        int u = pq.top().second;
        pq.pop();

        if (d > dist[u])   // old entry, skip it
            continue;
        if (u == dest)
            break;

        for (int i = 0; i < (int)adj[u].size(); i++) {
            int linkId = adj[u][i];
            if (!canUse(linkId))
                continue;
            int v = otherEnd(linkId, u);
            int newDist = dist[u] + links[linkId].latency;
            if (newDist < dist[v]) {
                dist[v] = newDist;
                parent[v] = u;
                pq.push(make_pair(newDist, v));
            }
        }
    }

    if (dist[dest] == INT_MAX)
        return path;

    // go back from dest to src using parent array
    for (int cur = dest; cur != -1; cur = parent[cur])
        path.push_back(cur);
    reverse(path.begin(), path.end());
    totalCost = dist[dest];
    return path;
}

// backtracking: tries every simple path from u to dest
void Network::pathDfs(int u, int dest, vector<bool> &visited, vector<int> &path, int cost,
                      vector< vector<int> > &paths, vector<int> &costs, int limit) {
    if ((int)paths.size() >= limit)
        return;
    if (u == dest) {
        paths.push_back(path);
        costs.push_back(cost);
        return;
    }
    visited[u] = true;
    for (int i = 0; i < (int)adj[u].size(); i++) {
        int linkId = adj[u][i];
        int v = otherEnd(linkId, u);
        if (!visited[v] && canUse(linkId)) {
            path.push_back(v);
            pathDfs(v, dest, visited, path, cost + links[linkId].latency, paths, costs, limit);
            path.pop_back();    // backtrack
        }
    }
    visited[u] = false;
}

// finds alternate routes and sorts them by cost (bubble sort)
void Network::allPaths(int src, int dest, vector< vector<int> > &paths, vector<int> &costs, int limit) {
    paths.clear();
    costs.clear();
    if (src < 0 || dest < 0 || !devices[src].working || !devices[dest].working)
        return;

    vector<bool> visited(devices.size(), false);
    vector<int> path;
    path.push_back(src);
    pathDfs(src, dest, visited, path, 0, paths, costs, 200);

    int n = paths.size();
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (costs[j] > costs[j + 1]) {
                swap(costs[j], costs[j + 1]);
                swap(paths[j], paths[j + 1]);
            }
        }
    }
    if ((int)paths.size() > limit) {
        paths.resize(limit);
        costs.resize(limit);
    }
}

// Tarjan's algorithm
// disc[u] = time when u was first visited
// low[u]  = lowest disc reachable from subtree of u using one back edge
void Network::tarjanDfs(int u, int parentLink, int &timer, vector<int> &disc, vector<int> &low,
                        vector<bool> &isCrit, vector<int> &bridgeLinks) {
    timer++;
    disc[u] = timer;
    low[u] = timer;
    int children = 0;

    for (int i = 0; i < (int)adj[u].size(); i++) {
        int linkId = adj[u][i];
        if (linkId == parentLink || !canUse(linkId))
            continue;
        int v = otherEnd(linkId, u);

        if (disc[v] == 0) {
            children++;
            tarjanDfs(v, linkId, timer, disc, low, isCrit, bridgeLinks);
            low[u] = min(low[u], low[v]);

            // u is not root and v cannot go above u -> u is articulation point
            if (parentLink != -1 && low[v] >= disc[u])
                isCrit[u] = true;
            // v cannot even reach u by other way -> this link is a bridge
            if (low[v] > disc[u])
                bridgeLinks.push_back(linkId);
        }
        else {
            low[u] = min(low[u], disc[v]);   // back edge
        }
    }

    // root is articulation point if it has more than one child in DFS tree
    if (parentLink == -1 && children > 1)
        isCrit[u] = true;
}

void Network::findCritical(vector<int> &critDevices, vector<int> &bridgeLinks) {
    int n = devices.size();
    vector<int> disc(n, 0), low(n, 0);
    vector<bool> isCrit(n, false);
    int timer = 0;
    critDevices.clear();
    bridgeLinks.clear();

    for (int i = 0; i < n; i++) {
        if (devices[i].working && disc[i] == 0)
            tarjanDfs(i, -1, timer, disc, low, isCrit, bridgeLinks);
    }
    for (int i = 0; i < n; i++) {
        if (isCrit[i])
            critDevices.push_back(i);
    }
}

// for every working device: fail it for a moment, see how many
// devices lose connection, then bring it back. Sorted high to low.
vector<ImpactScore> Network::impactRanking() {
    vector<ImpactScore> ranking;
    int before = largestGroup();

    for (int i = 0; i < (int)devices.size(); i++) {
        if (!devices[i].working)
            continue;
        devices[i].working = false;
        int after = largestGroup();
        devices[i].working = true;

        ImpactScore s;
        s.name = devices[i].name;
        s.lost = before - 1 - after;
        if (s.lost < 0)
            s.lost = 0;
        ranking.push_back(s);
    }

    // bubble sort, biggest impact first
    int n = ranking.size();
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (ranking[j].lost < ranking[j + 1].lost) {
                ImpactScore temp = ranking[j];
                ranking[j] = ranking[j + 1];
                ranking[j + 1] = temp;
            }
        }
    }
    return ranking;
}

// ================= PLANNED MAINTENANCE =================
// The admin gives a list of cables that will be taken down one after another.
// We want the first step at which the network splits.
// If removing the first k cables splits the network, removing more will also
// keep it split, so the answer is monotonic and we can use binary search.

// true if all working devices are still in one group after removing
// the first k cables of the plan (checked with DSU)
bool Network::connectedWithout(vector<int> &plannedLinks, int k) {
    vector<bool> removed(links.size(), false);
    for (int i = 0; i < k; i++)
        removed[plannedLinks[i]] = true;

    DSU dsu(devices.size());
    for (int i = 0; i < (int)links.size(); i++) {
        if (canUse(i) && !removed[i])
            dsu.unite(links[i].u, links[i].v);
    }

    int root = -1;
    for (int i = 0; i < (int)devices.size(); i++) {
        if (!devices[i].working)
            continue;
        if (root == -1)
            root = dsu.find(i);
        else if (dsu.find(i) != root)
            return false;
    }
    return true;
}

// returns how many planned removals it takes to split the network
// (1 means the first cable already splits it), or -1 if it never splits
int Network::breakingPoint(vector<int> &plannedLinks) {
    int n = plannedLinks.size();
    if (!connectedWithout(plannedLinks, 0))
        return 0;                       // already split before we start
    if (connectedWithout(plannedLinks, n))
        return -1;                      // still connected after all removals

    int low = 1, high = n, answer = n;
    while (low <= high) {
        int mid = (low + high) / 2;
        if (!connectedWithout(plannedLinks, mid)) {
            answer = mid;               // splits here, try smaller
            high = mid - 1;
        }
        else
            low = mid + 1;
    }
    return answer;
}

// number of working cables at this device
int Network::degree(int id) {
    int d = 0;
    for (int i = 0; i < (int)adj[id].size(); i++)
        if (canUse(adj[id][i]))
            d++;
    return d;
}

// density = links / maximum possible links  (max = n(n-1)/2)
double Network::density() {
    int n = workingDevices();
    if (n < 2)
        return 0;
    return workingLinks() * 2.0 / (n * (n - 1));
}

// ================= REPORT =================

int Network::workingDevices() {
    int c = 0;
    for (int i = 0; i < (int)devices.size(); i++)
        if (devices[i].working)
            c++;
    return c;
}

int Network::workingLinks() {
    int c = 0;
    for (int i = 0; i < (int)links.size(); i++)
        if (canUse(i))
            c++;
    return c;
}

// size of the biggest connected group (this is the "main" network)
int Network::largestGroup() {
    vector<int> compOf;
    int comps = countComponents(compOf);
    if (comps == 0)
        return 0;

    vector<int> size(comps, 0);
    for (int i = 0; i < (int)compOf.size(); i++)
        if (compOf[i] != -1)
            size[compOf[i]]++;

    int best = 0;
    for (int i = 0; i < comps; i++)
        best = max(best, size[i]);
    return best;
}

// percentage of devices that are still in the main network
double Network::healthPercent() {
    if (devices.size() == 0)
        return 100.0;
    return largestGroup() * 100.0 / devices.size();
}

string Network::severity() {
    bool anyFailure = false;
    for (int i = 0; i < (int)devices.size(); i++)
        if (!devices[i].working)
            anyFailure = true;
    for (int i = 0; i < (int)links.size(); i++)
        if (!links[i].working)
            anyFailure = true;
    if (!anyFailure)
        return "NONE";

    int cut = workingDevices() - largestGroup();   // working but cut off
    double cutPercent = 0;
    if (devices.size() > 0)
        cutPercent = cut * 100.0 / devices.size();

    if (cut == 0)
        return "LOW";
    if (cutPercent < 20)
        return "MEDIUM";
    if (cutPercent < 50)
        return "HIGH";
    return "CRITICAL";
}

void Network::addLog(string msg) {
    time_t now = time(0);
    tm *t = localtime(&now);
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", t);
    logs.push_back(string("[") + buf + "] " + msg);
}

void Network::printNetwork() {
    cout << "\nNetwork: " << networkName << endl;
    cout << "Devices (" << devices.size() << "):" << endl;
    for (int i = 0; i < (int)devices.size(); i++) {
        cout << "  " << i << ". " << devices[i].name << " [" << devices[i].type << "] "
             << (devices[i].working ? "UP" : "DOWN") << endl;
        cout << "      connected to: ";
        for (int j = 0; j < (int)adj[i].size(); j++) {
            Link &l = links[adj[i][j]];
            cout << devices[otherEnd(adj[i][j], i)].name << "(" << l.latency << "ms"
                 << (l.working ? "" : ", CUT") << ") ";
        }
        cout << endl;
    }
}

void Network::printReport() {
    vector<int> compOf;
    int comps = countComponents(compOf);
    vector<int> crit, bridges;
    findCritical(crit, bridges);

    cout << "\n========== FAILURE REPORT ==========" << endl;
    cout << "Network        : " << networkName << endl;
    cout << "Devices up     : " << workingDevices() << " / " << devices.size() << endl;
    cout << "Links up       : " << workingLinks() << " / " << links.size() << endl;
    cout << "Groups (DFS)   : " << comps << (comps > 1 ? "  <-- network is SPLIT" : "") << endl;
    cout << "Health         : " << healthPercent() << " %" << endl;
    cout << "Severity       : " << severity() << endl;
    cout << "Density        : " << density() << endl;

    cout << "Degree         : ";
    for (int i = 0; i < (int)devices.size(); i++)
        if (devices[i].working)
            cout << devices[i].name << "(" << degree(i) << ") ";
    cout << endl;

    cout << "Failed devices : ";
    bool none = true;
    for (int i = 0; i < (int)devices.size(); i++) {
        if (!devices[i].working) {
            cout << devices[i].name << " ";
            none = false;
        }
    }
    if (none) cout << "none";
    cout << endl;

    cout << "Critical points: ";
    for (int i = 0; i < (int)crit.size(); i++)
        cout << devices[crit[i]].name << " ";
    if (crit.size() == 0) cout << "none";
    cout << endl;

    string sev = severity();
    cout << "Recommendation : ";
    if (sev == "NONE")
        cout << "All good. Add backup links for the critical points above." << endl;
    else if (sev == "LOW")
        cout << "Traffic is using other routes. Fix the failed parts when possible." << endl;
    else if (sev == "MEDIUM")
        cout << "Some devices are cut off. Restore the failed device/link soon." << endl;
    else
        cout << "Major outage! Restore the failed core device first, then add redundancy." << endl;
    cout << "====================================" << endl;
}
