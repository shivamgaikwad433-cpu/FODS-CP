// Terminal controller for the live Network Failure Analyzer dashboard.
// All reads and changes use the same C++ REST API and saved data as the browser.

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "backend/httplib.h"
#include "backend/json.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

using json = nlohmann::json;

class DashboardApi {
    httplib::Client client_;
    std::string location_;

    static std::string encode(const std::string& value) {
        std::ostringstream out;
        out << std::uppercase << std::hex;
        for (std::string::const_iterator i = value.begin(); i != value.end(); ++i) {
            const unsigned char c = static_cast<unsigned char>(*i);
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out << c;
            else out << '%' << static_cast<int>(c);
        }
        return out.str();
    }

    bool send(const std::string& method, const std::string& path,
              const json& body, json& result) {
        httplib::Result response;
        if (method == "GET") response = client_.Get(path);
        else if (method == "POST") response = client_.Post(path, body.dump(), "application/json");
        else if (method == "PUT") response = client_.Put(path, body.dump(), "application/json");
        else if (method == "DELETE") response = client_.Delete(path);

        if (!response) {
            std::cout << "Could not connect to the dashboard server at http://localhost:8080.\n"
                      << "Start it from the project folder with:\n"
                      << "  powershell -NoProfile -ExecutionPolicy Bypass -File .\\scripts\\start-dashboard.ps1\n";
            return false;
        }

        result = json::parse(response->body, nullptr, false);
        if (response->status < 200 || response->status >= 300) {
            const std::string message = result.is_object()
                ? result.value("error", "HTTP " + std::to_string(response->status))
                : "HTTP " + std::to_string(response->status);
            std::cout << "Dashboard API error: " << message << "\n";
            return false;
        }
        if (result.is_discarded()) result = json::object();
        return true;
    }

public:
    explicit DashboardApi(const std::string& location)
        : client_("127.0.0.1", 8080), location_(location) {
        client_.set_connection_timeout(2, 0);
        client_.set_read_timeout(5, 0);
    }

    void setLocation(const std::string& location) { location_ = location; }
    const std::string& location() const { return location_; }
    std::string query() const { return "?locationId=" + encode(location_); }

    bool get(const std::string& path, json& result) {
        return send("GET", path, json::object(), result);
    }
    bool post(const std::string& path, const json& body, json& result) {
        return send("POST", path, body, result);
    }
    bool put(const std::string& path, const json& body, json& result) {
        return send("PUT", path, body, result);
    }
    bool remove(const std::string& path, json& result) {
        return send("DELETE", path, json::object(), result);
    }
    static std::string pathId(const std::string& id) { return encode(id); }
};

static std::string ask(const std::string& prompt, const std::string& fallback = "") {
    std::cout << prompt;
    if (!fallback.empty()) std::cout << " [" << fallback << "]";
    std::cout << ": ";
    std::string value;
    std::getline(std::cin, value);
    return value.empty() ? fallback : value;
}

static int askInt(const std::string& prompt, int minimum, int maximum) {
    for (;;) {
        const std::string text = ask(prompt);
        std::istringstream input(text);
        int value;
        char extra;
        if (input >> value && !(input >> extra) && value >= minimum && value <= maximum) return value;
        std::cout << "Enter a number from " << minimum << " to " << maximum << ".\n";
    }
}

static std::string queryValue(const std::string& key, const std::string& value) {
    std::string encoded;
    const char hex[] = "0123456789ABCDEF";
    for (std::string::const_iterator i = value.begin(); i != value.end(); ++i) {
        const unsigned char c = static_cast<unsigned char>(*i);
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') encoded += static_cast<char>(c);
        else { encoded += '%'; encoded += hex[c >> 4]; encoded += hex[c & 15]; }
    }
    return key + "=" + encoded;
}

static std::string deviceName(const json& network, const std::string& id) {
    for (json::const_iterator i = network["devices"].begin(); i != network["devices"].end(); ++i)
        if (i->value("id", "") == id) return i->value("name", id);
    return id;
}

static bool chooseLocation(DashboardApi& api, std::string& selected) {
    json locations;
    if (!api.get("/api/locations", locations) || !locations.is_array() || locations.empty()) {
        std::cout << "No dashboard locations are available.\n";
        return false;
    }
    std::cout << "\nLocations in the shared dashboard:\n";
    for (size_t i = 0; i < locations.size(); ++i)
        std::cout << "  " << (i + 1) << ". " << locations[i].value("name", "Unnamed")
                  << " (" << locations[i].value("id", "") << ")\n";
    const std::string defaultId = selected.empty() ? "school" : selected;
    const std::string choice = ask("Choose a number or location ID; Enter keeps the current location", defaultId);
    std::istringstream input(choice);
    size_t number;
    char extra;
    if ((input >> number) && !(input >> extra) && number >= 1 && number <= locations.size()) {
        selected = locations[number - 1].value("id", "school");
    } else {
        bool found = false;
        for (size_t i = 0; i < locations.size(); ++i) {
            if (locations[i].value("id", "") == choice) {
                selected = choice;
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << "That location ID was not found. Keeping " << defaultId << ".\n";
            selected = defaultId;
        }
    }
    api.setLocation(selected);
    std::cout << "Using " << selected << ". Select the same location in the dashboard to see these changes.\n";
    return true;
}

static bool loadNetwork(DashboardApi& api, json& network) {
    return api.get("/api/network" + api.query(), network);
}

static void showOverview(DashboardApi& api) {
    json stats, network;
    if (!api.get("/api/statistics" + api.query(), stats) || !loadNetwork(api, network)) return;
    std::cout << "\n=== " << network.value("locationName", api.location()) << " network ===\n"
              << "Devices: " << stats.value("totalDevices", 0)
              << " total, " << stats.value("activeDevices", 0) << " active, "
              << stats.value("failedDevices", 0) << " failed\n"
              << "Connections: " << stats.value("totalConnections", 0)
              << " total, " << stats.value("failedConnections", 0) << " failed\n"
              << "Availability: " << stats.value("availability", 0.0) << "%"
              << " | Connected components: " << stats.value("components", 0) << "\n\nDevices:\n";
    for (json::const_iterator i = network["devices"].begin(); i != network["devices"].end(); ++i)
        std::cout << "  " << i->value("id", "") << " | " << i->value("name", "")
                  << " | " << i->value("type", "") << " | " << i->value("status", "") << "\n";
    std::cout << "Connections:\n";
    for (json::const_iterator i = network["connections"].begin(); i != network["connections"].end(); ++i)
        std::cout << "  " << i->value("id", "") << " | " << i->value("source", "")
                  << " <-> " << i->value("target", "") << " | " << i->value("latency", 0)
                  << " ms | " << i->value("status", "") << "\n";
}

static void listDevices(DashboardApi& api, json& network) {
    if (!loadNetwork(api, network)) return;
    std::cout << "\nDevices in " << network.value("locationName", api.location()) << ":\n";
    for (json::const_iterator i = network["devices"].begin(); i != network["devices"].end(); ++i)
        std::cout << "  " << i->value("id", "") << " | " << i->value("name", "")
                  << " | " << i->value("type", "") << " | " << i->value("status", "") << "\n";
}

static std::string makeId(const std::string& value) {
    std::string id;
    for (std::string::const_iterator i = value.begin(); i != value.end(); ++i) {
        const unsigned char c = static_cast<unsigned char>(*i);
        if (std::isalnum(c)) id += static_cast<char>(std::tolower(c));
        else if (!id.empty() && id[id.size() - 1] != '-') id += '-';
    }
    while (!id.empty() && id[id.size() - 1] == '-') id.erase(id.size() - 1);
    return id;
}

static void manageDevices(DashboardApi& api) {
    std::cout << "\n1. List devices\n2. Add device\n3. Edit device\n4. Delete device\n0. Back\n";
    const int choice = askInt("Device action", 0, 4);
    json network, response;
    if (choice == 1) { listDevices(api, network); return; }
    if (choice == 2) {
        const std::string name = ask("Device name");
        if (name.empty()) return;
        const std::string id = ask("Unique device ID", makeId(name));
        const std::string type = ask("Device type", "computer");
        if (api.post("/api/devices" + api.query(), {{"id", id}, {"name", name}, {"type", type}}, response))
            std::cout << "Device added. The dashboard will refresh shortly.\n";
    } else if (choice == 3) {
        listDevices(api, network);
        const std::string id = ask("Device ID to edit");
        const std::string name = ask("New device name");
        const std::string type = ask("New device type");
        if (api.put("/api/devices/" + DashboardApi::pathId(id) + api.query(),
                    {{"name", name}, {"type", type}}, response))
            std::cout << "Device updated. The dashboard will refresh shortly.\n";
    } else if (choice == 4) {
        listDevices(api, network);
        const std::string id = ask("Device ID to delete");
        if (ask("Type YES to confirm deleting the device and its attached connections") != "YES") return;
        if (api.remove("/api/devices/" + DashboardApi::pathId(id) + api.query(), response))
            std::cout << "Device deleted. The dashboard will refresh shortly.\n";
    }
}

static void listConnections(DashboardApi& api, json& network) {
    if (!loadNetwork(api, network)) return;
    std::cout << "\nConnections in " << network.value("locationName", api.location()) << ":\n";
    for (json::const_iterator i = network["connections"].begin(); i != network["connections"].end(); ++i)
        std::cout << "  " << i->value("id", "") << " | " << i->value("source", "")
                  << " <-> " << i->value("target", "") << " | " << i->value("latency", 0)
                  << " ms | " << i->value("status", "") << "\n";
}

static void manageConnections(DashboardApi& api) {
    std::cout << "\n1. List connections\n2. Add connection\n3. Change latency\n4. Delete connection\n0. Back\n";
    const int choice = askInt("Connection action", 0, 4);
    json network, response;
    if (choice == 1) { listConnections(api, network); return; }
    if (choice == 2) {
        listDevices(api, network);
        const std::string source = ask("Source device ID");
        const std::string target = ask("Destination device ID");
        const std::string id = ask("Unique connection ID", makeId(source + "-" + target));
        const int latency = askInt("Latency / cost in ms", 0, 1000000);
        if (api.post("/api/connections" + api.query(),
                     {{"id", id}, {"source", source}, {"target", target}, {"latency", latency}}, response))
            std::cout << "Connection added. The dashboard will refresh shortly.\n";
    } else if (choice == 3) {
        listConnections(api, network);
        const std::string id = ask("Connection ID");
        const int latency = askInt("New latency / cost in ms", 0, 1000000);
        if (api.put("/api/connections/" + DashboardApi::pathId(id) + api.query(),
                    {{"latency", latency}}, response))
            std::cout << "Connection updated. The dashboard will refresh shortly.\n";
    } else if (choice == 4) {
        listConnections(api, network);
        const std::string id = ask("Connection ID to delete");
        if (ask("Type YES to confirm deleting the connection") != "YES") return;
        if (api.remove("/api/connections/" + DashboardApi::pathId(id) + api.query(), response))
            std::cout << "Connection deleted. The dashboard will refresh shortly.\n";
    }
}

static void simulateFailure(DashboardApi& api) {
    json network;
    if (!loadNetwork(api, network)) return;
    std::cout << "\n1. Fail a device\n2. Fail a connection\n";
    const int typeChoice = askInt("Failure type", 1, 2);
    if (typeChoice == 1) listDevices(api, network); else listConnections(api, network);
    const std::string id = ask(typeChoice == 1 ? "Device ID" : "Connection ID");
    json response;
    if (api.post("/api/simulate-failure" + api.query(),
                  {{"id", id}, {"type", typeChoice == 1 ? "device" : "connection"}}, response))
        std::cout << "Failure recorded. The dashboard will show it on its next refresh.\n";
}

static void restoreOne(DashboardApi& api) {
    json network;
    if (!loadNetwork(api, network)) return;
    std::cout << "\nFailed devices:\n";
    for (json::const_iterator i = network["devices"].begin(); i != network["devices"].end(); ++i)
        if (i->value("status", "") == "failed") std::cout << "  " << i->value("id", "") << " | device\n";
    std::cout << "Failed connections:\n";
    for (json::const_iterator i = network["connections"].begin(); i != network["connections"].end(); ++i)
        if (i->value("status", "") == "failed") std::cout << "  " << i->value("id", "") << " | connection\n";
    std::cout << "1. Restore device\n2. Restore connection\n";
    const int typeChoice = askInt("Target type", 1, 2);
    const std::string id = ask("Failed target ID");
    json response;
    if (api.post("/api/restore" + api.query(),
                 {{"id", id}, {"type", typeChoice == 1 ? "device" : "connection"}}, response))
        std::cout << "Target restored. The dashboard will refresh shortly.\n";
}

static void restoreAll(DashboardApi& api) {
    json response;
    if (api.post("/api/restore-all" + api.query(), json::object(), response))
        std::cout << "All devices and connections restored in " << api.location() << ".\n";
}

static void findPath(DashboardApi& api) {
    json network;
    if (!loadNetwork(api, network)) return;
    std::cout << "\nChoose endpoints from the device list:\n";
    for (json::const_iterator i = network["devices"].begin(); i != network["devices"].end(); ++i)
        std::cout << "  " << i->value("id", "") << " | " << i->value("name", "")
                  << " | " << i->value("status", "") << "\n";
    const std::string source = ask("Start device ID");
    const std::string destination = ask("Destination device ID");
    const std::string path = "/api/path" + api.query() + "&" + queryValue("source", source)
                           + "&" + queryValue("destination", destination);
    json route;
    if (!api.get(path, route)) return;
    std::cout << "\nShortest route from " << deviceName(network, source) << " to "
              << deviceName(network, destination) << ":\n  ";
    for (size_t i = 0; i < route["devices"].size(); ++i) {
        if (i) std::cout << " -> ";
        const std::string id = route["devices"][i].get<std::string>();
        std::cout << deviceName(network, id);
    }
    std::cout << "\nCost: " << route.value("latency", 0) << " ms | Hops: "
              << route.value("hops", 0) << "\n";
}

static void showCritical(DashboardApi& api) {
    json critical;
    if (!api.get("/api/critical-devices" + api.query(), critical)) return;
    std::cout << "\nCritical devices (articulation points):\n";
    if (critical["devices"].empty()) std::cout << "  None\n";
    for (json::const_iterator i = critical["devices"].begin(); i != critical["devices"].end(); ++i)
        std::cout << "  " << i->get<std::string>() << "\n";
    std::cout << "Critical connections (bridges):\n";
    if (critical["connections"].empty()) std::cout << "  None\n";
    for (json::const_iterator i = critical["connections"].begin(); i != critical["connections"].end(); ++i)
        std::cout << "  " << i->get<std::string>() << "\n";
}

static void showReachable(DashboardApi& api) {
    json network;
    if (!loadNetwork(api, network)) return;
    for (json::const_iterator i = network["devices"].begin(); i != network["devices"].end(); ++i)
        std::cout << "  " << i->value("id", "") << " | " << i->value("name", "") << "\n";
    const std::string source = ask("Source device ID");
    json result;
    if (!api.get("/api/reachable" + api.query() + "&" + queryValue("source", source), result)) return;
    std::cout << "\nReachable from " << deviceName(network, source) << ":\n";
    for (json::const_iterator i = result["reachable"].begin(); i != result["reachable"].end(); ++i)
        std::cout << "  " << deviceName(network, i->get<std::string>()) << "\n";
    std::cout << "Working but disconnected:\n";
    if (result["disconnected"].empty()) std::cout << "  None\n";
    for (json::const_iterator i = result["disconnected"].begin(); i != result["disconnected"].end(); ++i)
        std::cout << "  " << deviceName(network, i->get<std::string>()) << "\n";
}

static void showLogs(DashboardApi& api) {
    json logs;
    if (!api.get("/api/logs" + api.query(), logs)) return;
    std::cout << "\nRecent events for " << api.location() << ":\n";
    if (logs.empty()) std::cout << "  No events recorded.\n";
    for (json::const_iterator i = logs.begin(); i != logs.end(); ++i)
        std::cout << "  " << i->value("timestamp", "") << " | "
                  << i->value("description", i->value("type", "Event")) << " | "
                  << i->value("status", "") << "\n";
}

static void printMenu(const std::string& location) {
    std::cout << "\n=== Network Failure Analyzer | " << location << " ===\n"
              << "1. Network overview\n"
              << "2. Manage devices\n"
              << "3. Manage connections\n"
              << "4. Simulate failure\n"
              << "5. Restore one failed target\n"
              << "6. Restore all failures\n"
              << "7. Find shortest path\n"
              << "8. Critical device analysis\n"
              << "9. Reachability analysis\n"
              << "10. Event log\n"
              << "11. Switch location\n"
              << "0. Exit\n";
}

int main() {
    std::string location = "school";
    DashboardApi api(location);
    json locations;
    if (!api.get("/api/locations", locations)) return 1;
    if (!chooseLocation(api, location)) return 1;

    for (;;) {
        printMenu(location);
        const int choice = askInt("Select action", 0, 11);
        if (choice == 0) break;
        switch (choice) {
            case 1: showOverview(api); break;
            case 2: manageDevices(api); break;
            case 3: manageConnections(api); break;
            case 4: simulateFailure(api); break;
            case 5: restoreOne(api); break;
            case 6: restoreAll(api); break;
            case 7: findPath(api); break;
            case 8: showCritical(api); break;
            case 9: showReachable(api); break;
            case 10: showLogs(api); break;
            case 11: if (chooseLocation(api, location)) std::cout << "Location changed.\n"; break;
        }
    }
    std::cout << "Terminal client closed. Dashboard data remains saved.\n";
    return 0;
}

// Network Failure Analyzer - LEVEL 3 (user interface and reports)
// Level 1 + Level 2 features, plus:
//  - visual graph exported as network.html (failed parts in red)
//  - reachable / disconnected device counts
//  - failure summary report (events, impact, severity, recommendation)
//  - restore a failed device or connection and re-analyze automatically
// Compiles with any C++ standard (C++11 or newer).

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <queue>
#include <set>
#include <algorithm>
#include <climits>
#include <cmath>
#include <unordered_map>
using namespace std;

typedef pair<int, int> pii;

static string num(int x) {
    ostringstream o;
    o << x;
    return o.str();
}

class Network {
private:
    vector<string> names;
    unordered_map<string, int> idOf;
    vector<vector<pii> > adj;              // adj[u] = list of (neighbour, latency)
    vector<bool> deviceDown;
    set<pii> linkDown;
    vector<string> events;                 // failure / restore history

    struct Metrics {
        int working;
        int groups;
        int reachablePairs;
    };

    // ---------- helpers ----------
    static pii keyOf(int a, int b) { return make_pair(min(a, b), max(a, b)); }

    int findId(const string &name) const {
        unordered_map<string, int>::const_iterator it = idOf.find(name);
        return it == idOf.end() ? -1 : it->second;
    }

    bool hasLink(int a, int b) const {
        for (size_t i = 0; i < adj[a].size(); i++)
            if (adj[a][i].first == b) return true;
        return false;
    }

    bool linkWorks(int a, int b) const {
        return !deviceDown[a] && !deviceDown[b] && linkDown.count(keyOf(a, b)) == 0;
    }

    void clearFailures() {                 // silent reset (no log entry)
        for (size_t i = 0; i < deviceDown.size(); i++) deviceDown[i] = false;
        linkDown.clear();
    }

    void dfs(int u, vector<bool> &visited, vector<int> &component) const {
        visited[u] = true;
        component.push_back(u);
        for (size_t i = 0; i < adj[u].size(); i++) {
            int v = adj[u][i].first;
            if (!visited[v] && linkWorks(u, v)) dfs(v, visited, component);
        }
    }

    // ---------- Dijkstra (min-heap) ----------
    void dijkstra(int src, vector<int> &dist, vector<int> &par) const {
        dist.assign(names.size(), INT_MAX);
        par.assign(names.size(), -1);
        priority_queue<pii, vector<pii>, greater<pii> > pq;
        dist[src] = 0;
        pq.push(make_pair(0, src));
        while (!pq.empty()) {
            int d = pq.top().first;
            int u = pq.top().second;
            pq.pop();
            if (d > dist[u]) continue;
            for (size_t i = 0; i < adj[u].size(); i++) {
                int v = adj[u][i].first, w = adj[u][i].second;
                if (!linkWorks(u, v)) continue;
                if (dist[u] + w < dist[v]) {
                    dist[v] = dist[u] + w;
                    par[v] = u;
                    pq.push(make_pair(dist[v], v));
                }
            }
        }
    }

    int pathCost(int s, int t) const {
        if (deviceDown[s] || deviceDown[t]) return -1;
        vector<int> dist, par;
        dijkstra(s, dist, par);
        return dist[t] == INT_MAX ? -1 : dist[t];
    }

    // ---------- all simple paths (backtracking) ----------
    void allPaths(int u, int t, vector<bool> &visited, vector<int> &path, int cost,
                  vector<pair<int, vector<int> > > &results) const {
        if (results.size() >= 1000) return;
        if (u == t) { results.push_back(make_pair(cost, path)); return; }
        visited[u] = true;
        for (size_t i = 0; i < adj[u].size(); i++) {
            int v = adj[u][i].first, w = adj[u][i].second;
            if (!visited[v] && linkWorks(u, v)) {
                path.push_back(v);
                allPaths(v, t, visited, path, cost + w, results);
                path.pop_back();
            }
        }
        visited[u] = false;
    }

    // ---------- Tarjan: articulation points + bridges ----------
    void tarjan(int u, int parent, int &timer, vector<int> &disc, vector<int> &low,
                vector<bool> &isAP, vector<pii> &bridges) const {
        disc[u] = low[u] = ++timer;
        int children = 0;
        for (size_t i = 0; i < adj[u].size(); i++) {
            int v = adj[u][i].first;
            if (!linkWorks(u, v) || v == parent) continue;
            if (disc[v] != -1) {
                low[u] = min(low[u], disc[v]);
            } else {
                children++;
                tarjan(v, u, timer, disc, low, isAP, bridges);
                low[u] = min(low[u], low[v]);
                if (low[v] > disc[u]) bridges.push_back(make_pair(u, v));
                if (parent != -1 && low[v] >= disc[u]) isAP[u] = true;
            }
        }
        if (parent == -1 && children > 1) isAP[u] = true;
    }

    void findCritical(vector<bool> &isAP, vector<pii> &bridges) const {
        int n = (int)names.size(), timer = 0;
        vector<int> disc(n, -1), low(n, 0);
        isAP.assign(n, false);
        bridges.clear();
        for (int u = 0; u < n; u++)
            if (!deviceDown[u] && disc[u] == -1) tarjan(u, -1, timer, disc, low, isAP, bridges);
    }

    Metrics computeMetrics() const {
        Metrics m;
        m.working = 0;
        for (size_t i = 0; i < names.size(); i++) if (!deviceDown[i]) m.working++;
        vector<vector<int> > groups;
        m.groups = findComponents(groups);
        m.reachablePairs = 0;
        for (size_t g = 0; g < groups.size(); g++) {
            int s = (int)groups[g].size();
            m.reachablePairs += s * (s - 1) / 2;
        }
        return m;
    }

    // metrics of the healthy network (all failures temporarily removed)
    Metrics baseline() {
        vector<bool> savedDown = deviceDown;
        set<pii> savedLinks = linkDown;
        clearFailures();
        Metrics m = computeMetrics();
        deviceDown = savedDown;
        linkDown = savedLinks;
        return m;
    }

    void printPath(const vector<int> &path) const {
        for (size_t i = 0; i < path.size(); i++)
            cout << names[path[i]] << (i + 1 < path.size() ? " -> " : "");
    }

    static string esc(const string &s) {      // make text safe for HTML
        string r;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '&') r += "&amp;";
            else if (s[i] == '<') r += "&lt;";
            else if (s[i] == '>') r += "&gt;";
            else r += s[i];
        }
        return r;
    }

public:
    // ================= LEVEL 1 =================
    int addDevice(const string &name) {
        int id = findId(name);
        if (id != -1) return id;
        id = (int)names.size();
        idOf[name] = id;
        names.push_back(name);
        adj.push_back(vector<pii>());
        deviceDown.push_back(false);
        return id;
    }

    void addConnection(const string &a, const string &b, int latency) {
        int u = addDevice(a), v = addDevice(b);
        adj[u].push_back(make_pair(v, latency));
        adj[v].push_back(make_pair(u, latency));
    }

    void display() const {
        cout << "\n--- Adjacency List (neighbour:latency) ---\n";
        for (size_t u = 0; u < names.size(); u++) {
            cout << names[u] << (deviceDown[u] ? " [DOWN]" : "") << " -> ";
            for (size_t i = 0; i < adj[u].size(); i++) {
                int v = adj[u][i].first;
                cout << names[v] << ":" << adj[u][i].second
                     << (linkWorks((int)u, v) ? "" : "(X)") << "  ";
            }
            cout << "\n";
        }
    }

    bool failDevice(const string &name) {
        int u = findId(name);
        if (u == -1) return false;
        if (!deviceDown[u]) {
            deviceDown[u] = true;
            events.push_back("Device failed: " + name);
        }
        return true;
    }

    bool failConnection(const string &a, const string &b) {
        int u = findId(a), v = findId(b);
        if (u == -1 || v == -1 || !hasLink(u, v)) return false;
        if (linkDown.insert(keyOf(u, v)).second)
            events.push_back("Connection failed: " + a + " - " + b);
        return true;
    }

    void reachableFrom(const string &name) const {
        int src = findId(name);
        if (src == -1) { cout << "Device not found.\n"; return; }
        if (deviceDown[src]) { cout << name << " is down.\n"; return; }
        vector<bool> visited(names.size(), false);
        queue<int> q;
        q.push(src);
        visited[src] = true;
        vector<int> order;
        while (!q.empty()) {
            int u = q.front();
            q.pop();
            order.push_back(u);
            for (size_t i = 0; i < adj[u].size(); i++) {
                int v = adj[u][i].first;
                if (!visited[v] && linkWorks(u, v)) { visited[v] = true; q.push(v); }
            }
        }
        cout << "Reachable from " << name << " (BFS): ";
        for (size_t i = 0; i < order.size(); i++) cout << names[order[i]] << " ";
        cout << "\nNot reachable: ";
        bool any = false;
        for (size_t v = 0; v < names.size(); v++)
            if (!visited[v]) { cout << names[v] << (deviceDown[v] ? "(down) " : " "); any = true; }
        if (!any) cout << "none";
        cout << "\n";
    }

    int findComponents(vector<vector<int> > &groups) const {
        vector<bool> visited(names.size(), false);
        groups.clear();
        for (size_t u = 0; u < names.size(); u++) {
            if (!visited[u] && !deviceDown[u]) {
                vector<int> component;
                dfs((int)u, visited, component);
                groups.push_back(component);
            }
        }
        return (int)groups.size();
    }

    void report() const {
        int total = (int)names.size(), down = 0;
        for (size_t i = 0; i < names.size(); i++) if (deviceDown[i]) down++;
        vector<vector<int> > groups;
        int count = findComponents(groups);
        cout << "\n========== FAILURE ANALYSIS REPORT ==========\n";
        cout << "Total devices        : " << total << "\n";
        cout << "Working devices      : " << total - down << "\n";
        cout << "Failed devices       : " << down;
        if (down > 0) {
            cout << " (";
            for (size_t i = 0; i < names.size(); i++) if (deviceDown[i]) cout << names[i] << " ";
            cout << ")";
        }
        cout << "\nFailed connections   : " << linkDown.size() << "\n";
        cout << "Isolated groups      : " << count << "\n";
        for (size_t g = 0; g < groups.size(); g++) {
            cout << "  Group " << g + 1 << ": ";
            for (size_t i = 0; i < groups[g].size(); i++) cout << names[groups[g][i]] << " ";
            cout << "\n";
        }
        if (count <= 1) cout << "STATUS: Network is CONNECTED.\n";
        else cout << "STATUS: Network is PARTITIONED into " << count << " parts!\n";
        cout << "=============================================\n";
    }

    // ================= LEVEL 2 =================
    void shortestPath(const string &a, const string &b) const {
        int s = findId(a), t = findId(b);
        if (s == -1 || t == -1) { cout << "Device not found.\n"; return; }
        if (deviceDown[s] || deviceDown[t]) { cout << "A selected device is down.\n"; return; }
        vector<int> dist, par;
        dijkstra(s, dist, par);
        if (dist[t] == INT_MAX) {
            cout << "NO ROUTE from " << a << " to " << b << " (network failure).\n";
            return;
        }
        vector<int> path;
        for (int v = t; v != -1; v = par[v]) path.push_back(v);
        reverse(path.begin(), path.end());
        cout << "Shortest route (" << dist[t] << " ms): ";
        printPath(path);
        cout << "\n";
    }

    void alternativeRoutes(const string &a, const string &b) const {
        int s = findId(a), t = findId(b);
        if (s == -1 || t == -1) { cout << "Device not found.\n"; return; }
        if (deviceDown[s] || deviceDown[t]) { cout << "A selected device is down.\n"; return; }
        vector<bool> visited(names.size(), false);
        vector<int> path(1, s);
        vector<pair<int, vector<int> > > results;
        allPaths(s, t, visited, path, 0, results);
        if (results.empty()) { cout << "NO ROUTE from " << a << " to " << b << ".\n"; return; }
        sort(results.begin(), results.end());
        cout << results.size() << " route(s) from " << a << " to " << b << ":\n";
        for (size_t i = 0; i < results.size() && i < 5; i++) {
            cout << "  " << (i == 0 ? string("[BEST] ") : "[ALT " + num((int)i) + "] ")
                 << results[i].first << " ms : ";
            printPath(results[i].second);
            cout << "\n";
        }
        if (results.size() == 1) cout << "WARNING: only one route exists (no backup path)!\n";
    }

    void criticalAnalysis() const {
        vector<bool> isAP;
        vector<pii> bridges;
        findCritical(isAP, bridges);
        cout << "\n--- Critical Devices (removal disconnects the network) ---\n";
        bool any = false;
        for (size_t u = 0; u < names.size(); u++)
            if (isAP[u]) { cout << "  " << names[u] << "\n"; any = true; }
        if (!any) cout << "  none\n";
        cout << "--- Critical Connections (bridges) ---\n";
        any = false;
        for (size_t i = 0; i < bridges.size(); i++) {
            cout << "  " << names[bridges[i].first] << " - " << names[bridges[i].second] << "\n";
            any = true;
        }
        if (!any) cout << "  none\n";
    }

    void compare(const string &a, const string &b) {
        int s = findId(a), t = findId(b);
        if (s == -1 || t == -1) { cout << "Device not found.\n"; return; }
        Metrics after = computeMetrics();
        int costAfter = pathCost(s, t);
        vector<bool> savedDown = deviceDown;
        set<pii> savedLinks = linkDown;
        clearFailures();
        Metrics before = computeMetrics();
        int costBefore = pathCost(s, t);
        deviceDown = savedDown;
        linkDown = savedLinks;

        cout << "\n====== BEFORE vs AFTER FAILURE ======\n";
        cout << "Working devices      : " << before.working << "  ->  " << after.working << "\n";
        cout << "Isolated groups      : " << before.groups << "  ->  " << after.groups << "\n";
        cout << "Device pairs that can talk: " << before.reachablePairs << "  ->  "
             << after.reachablePairs << "\n";
        cout << "Route " << a << " -> " << b << " : ";
        if (costBefore == -1) cout << "none"; else cout << costBefore << " ms";
        cout << "  ->  ";
        if (costAfter == -1) cout << "NO ROUTE"; else cout << costAfter << " ms";
        cout << "\n";
        if (before.reachablePairs > 0) {
            int lost = before.reachablePairs - after.reachablePairs;
            cout << "Communication lost   : " << (lost * 100) / before.reachablePairs << "%\n";
        }
        cout << "=====================================\n";
    }

    // ================= LEVEL 3 =================

    // --- restore components, then re-analyze automatically ---
    bool restoreDevice(const string &name) {
        int u = findId(name);
        if (u == -1 || !deviceDown[u]) return false;
        deviceDown[u] = false;
        events.push_back("Device restored: " + name);
        return true;
    }

    bool restoreConnection(const string &a, const string &b) {
        int u = findId(a), v = findId(b);
        if (u == -1 || v == -1) return false;
        if (linkDown.erase(keyOf(u, v)) == 0) return false;
        events.push_back("Connection restored: " + a + " - " + b);
        return true;
    }

    void restoreAll() {
        clearFailures();
        events.push_back("All failures restored");
    }

    // --- reachable / disconnected counts from a chosen device ---
    void reachabilitySummary(const string &name) const {
        int src = findId(name);
        if (src == -1) { cout << "Device not found.\n"; return; }
        if (deviceDown[src]) { cout << name << " is down.\n"; return; }
        vector<bool> visited(names.size(), false);
        queue<int> q;
        q.push(src);
        visited[src] = true;
        while (!q.empty()) {
            int u = q.front();
            q.pop();
            for (size_t i = 0; i < adj[u].size(); i++) {
                int v = adj[u][i].first;
                if (!visited[v] && linkWorks(u, v)) { visited[v] = true; q.push(v); }
            }
        }
        int reach = 0, cut = 0, failed = 0;
        for (size_t v = 0; v < names.size(); v++) {
            if ((int)v == src) continue;
            if (deviceDown[v]) failed++;
            else if (visited[v]) reach++;
            else cut++;
        }
        cout << "\n--- Reachability from " << name << " ---\n";
        cout << "Reachable devices     : " << reach << "\n";
        cout << "Disconnected devices  : " << cut << "  (working, but cut off)\n";
        cout << "Failed devices        : " << failed << "\n";
        int others = (int)names.size() - 1;
        if (others > 0) cout << "Reachable share       : " << (reach * 100) / others << "% of all other devices\n";
    }

    // --- failure summary report ---
    void failureSummary() {
        int total = (int)names.size();
        if (total == 0) { cout << "Network is empty.\n"; return; }
        int down = 0;
        for (size_t i = 0; i < names.size(); i++) if (deviceDown[i]) down++;

        Metrics now = computeMetrics();
        Metrics base = baseline();
        int lostPct = 0;
        if (base.reachablePairs > 0)
            lostPct = ((base.reachablePairs - now.reachablePairs) * 100) / base.reachablePairs;

        vector<vector<int> > groups;
        findComponents(groups);
        size_t largest = 0;
        for (size_t g = 1; g < groups.size(); g++)
            if (groups[g].size() > groups[largest].size()) largest = g;

        vector<bool> isAP;
        vector<pii> bridges;
        findCritical(isAP, bridges);

        string severity;
        if (down == 0 && linkDown.empty()) severity = "NONE (no failures)";
        else if (lostPct == 0) severity = "LOW (nothing lost)";
        else if (lostPct < 30) severity = "MEDIUM";
        else if (lostPct < 70) severity = "HIGH";
        else severity = "CRITICAL";

        cout << "\n============== FAILURE SUMMARY ==============\n";
        cout << "Event history (" << events.size() << "):\n";
        if (events.empty()) cout << "  (no events yet)\n";
        for (size_t i = 0; i < events.size(); i++) cout << "  " << i + 1 << ". " << events[i] << "\n";

        cout << "\nFailed devices      : " << down << " of " << total << "\n";
        cout << "Failed connections  : " << linkDown.size() << "\n";
        cout << "Network status      : ";
        if (now.groups <= 1) cout << "CONNECTED\n";
        else cout << "PARTITIONED into " << now.groups << " groups\n";
        if (!groups.empty()) {
            cout << "Largest group       : " << groups[largest].size() << " device(s)\n";
        }
        cout << "Isolated devices    : ";
        bool any = false;
        for (size_t g = 0; g < groups.size(); g++)
            if (groups[g].size() == 1 && now.groups > 1) { cout << names[groups[g][0]] << " "; any = true; }
        if (!any) cout << "none";
        cout << "\nCommunication lost  : " << lostPct << "%  ("
             << now.reachablePairs << " of " << base.reachablePairs << " device pairs still work)\n";
        cout << "Severity            : " << severity << "\n";

        cout << "Critical devices left: ";
        any = false;
        for (size_t u = 0; u < names.size(); u++)
            if (isAP[u]) { cout << names[u] << " "; any = true; }
        if (!any) cout << "none";
        cout << "\n";

        cout << "Recommendation      : ";
        if (down == 0 && linkDown.empty()) cout << "No failures. Network is healthy.";
        else if (now.groups > 1) cout << "Restore the failed parts or add backup links to reconnect the isolated groups.";
        else cout << "Network still connected, but redundancy is reduced. Restore failed parts soon.";
        cout << "\n=============================================\n";
    }

    // --- visual graph: writes an HTML file with an SVG drawing ---
    bool exportVisual(const string &file) const {
        int n = (int)names.size();
        if (n == 0) return false;
        ofstream out(file.c_str());
        if (!out) return false;

        vector<vector<int> > groups;
        int groupCount = findComponents(groups);
        vector<int> groupOf(n, -1);
        for (size_t g = 0; g < groups.size(); g++)
            for (size_t i = 0; i < groups[g].size(); i++) groupOf[groups[g][i]] = (int)g;

        const char *palette[] = {"#2563eb", "#15803d", "#7c3aed", "#0e7490", "#b45309", "#4b5563"};
        const int paletteSize = 6;
        const char *RED = "#dc2626";

        int down = 0;
        for (int i = 0; i < n; i++) if (deviceDown[i]) down++;

        // positions on a circle
        const double PI = 3.14159265358979;
        vector<int> px(n), py(n);
        for (int i = 0; i < n; i++) {
            if (n == 1) { px[i] = 400; py[i] = 300; continue; }
            double ang = 2 * PI * i / n - PI / 2;
            px[i] = (int)(400 + 230 * cos(ang));
            py[i] = (int)(300 + 230 * sin(ang));
        }

        out << "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">"
            << "<title>Network Failure Analyzer</title>\n"
            << "<style>body{font-family:Arial,sans-serif;margin:20px;background:#f8fafc;color:#111}"
            << ".chip{display:inline-block;padding:6px 12px;margin:3px;border-radius:6px;background:#e2e8f0}"
            << ".bad{background:#fee2e2;color:#991b1b}.good{background:#dcfce7;color:#166534}"
            << ".dot{display:inline-block;width:12px;height:12px;border-radius:50%;margin-right:6px}"
            << "svg{background:#fff;border:1px solid #cbd5e1;border-radius:8px}</style></head><body>\n";
        out << "<h2>Network Failure Analyzer - Visual Graph</h2>\n<div>";
        out << "<span class=\"chip\">Total devices: " << n << "</span>";
        out << "<span class=\"chip good\">Working: " << n - down << "</span>";
        out << "<span class=\"chip " << (down ? "bad" : "good") << "\">Failed devices: " << down << "</span>";
        out << "<span class=\"chip " << (linkDown.empty() ? "good" : "bad") << "\">Failed connections: "
            << linkDown.size() << "</span>";
        out << "<span class=\"chip " << (groupCount <= 1 ? "good" : "bad") << "\">"
            << (groupCount <= 1 ? "Status: CONNECTED" : "Status: PARTITIONED into " + num(groupCount) + " groups")
            << "</span></div>\n";

        out << "<svg width=\"800\" height=\"600\" viewBox=\"0 0 800 600\">\n";

        // connections
        for (int u = 0; u < n; u++) {
            for (size_t i = 0; i < adj[u].size(); i++) {
                int v = adj[u][i].first, w = adj[u][i].second;
                if (u >= v) continue;                 // draw each link once
                bool ok = linkWorks(u, v);
                out << "<line x1=\"" << px[u] << "\" y1=\"" << py[u] << "\" x2=\"" << px[v]
                    << "\" y2=\"" << py[v] << "\" stroke=\"" << (ok ? "#6b7280" : RED)
                    << "\" stroke-width=\"" << (ok ? 2 : 3) << "\""
                    << (ok ? "" : " stroke-dasharray=\"8,6\"") << "/>\n";
                out << "<text x=\"" << (px[u] + px[v]) / 2 << "\" y=\"" << (py[u] + py[v]) / 2 + 4
                    << "\" font-size=\"12\" text-anchor=\"middle\" fill=\"" << (ok ? "#374151" : RED)
                    << "\" stroke=\"white\" stroke-width=\"3\" paint-order=\"stroke\">" << w << "ms</text>\n";
            }
        }

        // devices
        for (int u = 0; u < n; u++) {
            const char *color = deviceDown[u] ? RED : palette[groupOf[u] % paletteSize];
            out << "<circle cx=\"" << px[u] << "\" cy=\"" << py[u] << "\" r=\"30\" fill=\"" << color
                << "\" stroke=\"white\" stroke-width=\"3\"/>\n";
            out << "<text x=\"" << px[u] << "\" y=\"" << py[u] + 4
                << "\" font-size=\"11\" font-weight=\"bold\" text-anchor=\"middle\" fill=\"white\">"
                << esc(names[u]) << "</text>\n";
            if (deviceDown[u])
                out << "<text x=\"" << px[u] << "\" y=\"" << py[u] + 46
                    << "\" font-size=\"11\" font-weight=\"bold\" text-anchor=\"middle\" fill=\""
                    << RED << "\">DOWN</text>\n";
        }
        out << "</svg>\n";

        // legend and groups
        out << "<p><span class=\"dot\" style=\"background:" << RED << "\"></span>Failed device"
            << " &nbsp; <span style=\"color:" << RED << "\">- - -</span> Failed connection"
            << " &nbsp; Same color = same connected group</p>\n";
        for (size_t g = 0; g < groups.size(); g++) {
            out << "<div><span class=\"dot\" style=\"background:" << palette[g % paletteSize]
                << "\"></span>Group " << g + 1 << " (" << groups[g].size() << "): ";
            for (size_t i = 0; i < groups[g].size(); i++) out << esc(names[groups[g][i]]) << " ";
            out << "</div>\n";
        }
        out << "</body></html>\n";
        out.close();
        return true;
    }
};

int main() {
    Network net;

    // sample network: connection with latency in ms (change to your own)
    net.addConnection("Router1", "Switch1", 5);
    net.addConnection("Router1", "Switch2", 7);
    net.addConnection("Switch1", "Server1", 2);
    net.addConnection("Switch2", "Server1", 4);
    net.addConnection("Switch2", "Switch3", 3);
    net.addConnection("Switch3", "PC1", 1);
    net.addConnection("Switch3", "PC2", 1);

    int choice;
    do {
        cout << "\n======= NETWORK FAILURE ANALYZER (Level 3) =======\n"
             << " 1. Add connection (with latency)\n"
             << " 2. Display adjacency list\n"
             << " 3. Fail a device\n"
             << " 4. Fail a connection\n"
             << " 5. Reachable devices from X (BFS)\n"
             << " 6. Failure analysis report (DFS components)\n"
             << " 7. Shortest route A -> B (Dijkstra)\n"
             << " 8. Alternative routes A -> B\n"
             << " 9. Critical devices and connections\n"
             << "10. Compare before vs after failure\n"
             << "11. Restore ALL failures\n"
             << "12. Restore one device\n"
             << "13. Restore one connection\n"
             << "14. Reachable / disconnected count from X\n"
             << "15. Failure summary\n"
             << "16. Visual graph (creates network.html)\n"
             << " 0. Exit\nChoice: ";
        if (!(cin >> choice)) break;

        string a, b;
        int w;
        switch (choice) {
        case 1:
            cout << "Device1 Device2 Latency(ms): ";
            cin >> a >> b >> w;
            net.addConnection(a, b, w);
            break;
        case 2: net.display(); break;
        case 3:
            cout << "Device to fail: ";
            cin >> a;
            if (!net.failDevice(a)) cout << "Device not found.\n";
            else { cout << a << " is now DOWN.\n"; net.report(); }
            break;
        case 4:
            cout << "Connection to fail (two device names): ";
            cin >> a >> b;
            if (!net.failConnection(a, b)) cout << "No such connection.\n";
            else { cout << a << " - " << b << " is now DOWN.\n"; net.report(); }
            break;
        case 5:
            cout << "Source device: ";
            cin >> a;
            net.reachableFrom(a);
            break;
        case 6: net.report(); break;
        case 7:
            cout << "From To: ";
            cin >> a >> b;
            net.shortestPath(a, b);
            break;
        case 8:
            cout << "From To: ";
            cin >> a >> b;
            net.alternativeRoutes(a, b);
            break;
        case 9: net.criticalAnalysis(); break;
        case 10:
            cout << "Compare route From To: ";
            cin >> a >> b;
            net.compare(a, b);
            break;
        case 11:
            net.restoreAll();
            cout << "All devices and connections restored.\n";
            net.report();
            break;
        case 12:
            cout << "Device to restore: ";
            cin >> a;
            if (!net.restoreDevice(a)) cout << "Device not found or not failed.\n";
            else { cout << a << " is back UP. Re-analyzing...\n"; net.report(); }
            break;
        case 13:
            cout << "Connection to restore (two device names): ";
            cin >> a >> b;
            if (!net.restoreConnection(a, b)) cout << "Connection not found or not failed.\n";
            else { cout << a << " - " << b << " is back UP. Re-analyzing...\n"; net.report(); }
            break;
        case 14:
            cout << "Source device: ";
            cin >> a;
            net.reachabilitySummary(a);
            break;
        case 15: net.failureSummary(); break;
        case 16:
            if (net.exportVisual("network.html"))
                cout << "Saved network.html - open it in your web browser (double-click the file).\n";
            else
                cout << "Could not create network.html\n";
            break;
        }
    } while (choice != 0);

    return 0;
}
