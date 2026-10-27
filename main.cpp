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