// server.cpp
// A very small HTTP server so that the web page (web folder) can use our C++ analyzer.
// The browser sends GET requests like /api/fail-device?name=Router1 and we reply with JSON.
// It handles one request at a time, so no threads are needed (works on old MinGW also).
//
// Build (Windows): g++ -std=c++11 src/server.cpp src/Network.cpp -o server.exe -lws2_32
// Build (Linux)  : g++ -std=c++11 src/server.cpp src/Network.cpp -o server
// Then open http://localhost:8080
// Build (WebAssembly for GitHub Pages): see build_pages.sh

#include "Network.h"
#include <sstream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <ctime>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define closesocket close
#endif

const int PORT = 8080;

Network net;
string currentFile = "college";

// ---------------- small helper functions ----------------

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

// converts "Lab%20PC" to "Lab PC"
string urlDecode(string s) {
    string out = "";
    for (int i = 0; i < (int)s.size(); i++) {
        if (s[i] == '%' && i + 2 < (int)s.size()) {
            out += (char)(hexValue(s[i + 1]) * 16 + hexValue(s[i + 2]));
            i += 2;
        }
        else if (s[i] == '+')
            out += ' ';
        else
            out += s[i];
    }
    return out;
}

// gets value of key from "a=1&b=2"
string getParam(string query, string key) {
    stringstream ss(query);
    string part;
    while (getline(ss, part, '&')) {
        int eq = part.find('=');
        if (eq == (int)string::npos)
            continue;
        if (part.substr(0, eq) == key)
            return urlDecode(part.substr(eq + 1));
    }
    return "";
}

// device names go into a text file separated by spaces, so only allow simple names
bool validName(string s) {
    if (s.size() == 0 || s.size() > 20)
        return false;
    for (int i = 0; i < (int)s.size(); i++) {
        char c = s[i];
        if (!isalnum((unsigned char)c) && c != '_' && c != '-')
            return false;
    }
    return true;
}

string jsonStr(string s) {
    string out = "\"";
    for (int i = 0; i < (int)s.size(); i++) {
        if (s[i] == '"' || s[i] == '\\')
            out += '\\';
        out += s[i];
    }
    return out + "\"";
}

string boolStr(bool b) {
    return b ? "true" : "false";
}

string pathJson(vector<int> &path) {
    string s = "[";
    for (int i = 0; i < (int)path.size(); i++) {
        if (i > 0) s += ",";
        s += jsonStr(net.devices[path[i]].name);
    }
    return s + "]";
}

// list of all .txt network files inside data folder
vector<string> listNetworks() {
    vector<string> names;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("data\\*.txt", &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            string f = fd.cFileName;
            names.push_back(f.substr(0, f.size() - 4));
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR *dir = opendir("data");
    if (dir) {
        struct dirent *e;
        while ((e = readdir(dir)) != NULL) {
            string f = e->d_name;
            if (f.size() > 4 && f.substr(f.size() - 4) == ".txt")
                names.push_back(f.substr(0, f.size() - 4));
        }
        closedir(dir);
    }
#endif
    return names;
}

// ---------------- JSON of the whole network ----------------

string stateJson(string msg, bool ok) {
    vector<int> compOf;
    int groups = net.countComponents(compOf);
    vector<int> crit, bridges;
    net.findCritical(crit, bridges);

    vector<bool> isCrit(net.devices.size(), false);
    for (int i = 0; i < (int)crit.size(); i++)
        isCrit[crit[i]] = true;
    vector<bool> isBridge(net.links.size(), false);
    for (int i = 0; i < (int)bridges.size(); i++)
        isBridge[bridges[i]] = true;

    stringstream js;
    js << "{";
    js << "\"ok\":" << boolStr(ok) << ",";
    js << "\"msg\":" << jsonStr(msg) << ",";
    js << "\"name\":" << jsonStr(net.networkName) << ",";
    js << "\"file\":" << jsonStr(currentFile) << ",";

    js << "\"devices\":[";
    for (int i = 0; i < (int)net.devices.size(); i++) {
        Device &d = net.devices[i];
        if (i > 0) js << ",";
        js << "{\"name\":" << jsonStr(d.name) << ",\"type\":" << jsonStr(d.type)
           << ",\"working\":" << boolStr(d.working) << ",\"x\":" << d.x << ",\"y\":" << d.y
           << ",\"critical\":" << boolStr(isCrit[i]) << ",\"group\":" << compOf[i]
           << ",\"degree\":" << net.degree(i) << "}";
    }
    js << "],";

    js << "\"links\":[";
    for (int i = 0; i < (int)net.links.size(); i++) {
        Link &l = net.links[i];
        if (i > 0) js << ",";
        js << "{\"a\":" << jsonStr(net.devices[l.u].name) << ",\"b\":" << jsonStr(net.devices[l.v].name)
           << ",\"latency\":" << l.latency << ",\"working\":" << boolStr(l.working)
           << ",\"usable\":" << boolStr(net.canUse(i)) << ",\"bridge\":" << boolStr(isBridge[i]) << "}";
    }
    js << "],";

    js.setf(ios::fixed);
    js.precision(1);
    js << "\"stats\":{\"devices\":" << net.devices.size() << ",\"devicesUp\":" << net.workingDevices()
       << ",\"links\":" << net.links.size() << ",\"linksUp\":" << net.workingLinks()
       << ",\"groups\":" << groups << ",\"cut\":" << net.workingDevices() - net.largestGroup()
       << ",\"health\":" << net.healthPercent() << ",\"severity\":" << jsonStr(net.severity());
    js.precision(2);
    js << ",\"density\":" << net.density() << "},";

    vector<ImpactScore> rank = net.impactRanking();
    js << "\"ranking\":[";
    for (int i = 0; i < (int)rank.size(); i++) {
        if (i > 0) js << ",";
        js << "{\"name\":" << jsonStr(rank[i].name) << ",\"lost\":" << rank[i].lost << "}";
    }
    js << "],";

    // only send the last 60 log lines
    js << "\"logs\":[";
    int start = net.logs.size() > 60 ? net.logs.size() - 60 : 0;
    for (int i = start; i < (int)net.logs.size(); i++) {
        if (i > start) js << ",";
        js << jsonStr(net.logs[i]);
    }
    js << "],";
    js << "\"canUndo\":" << boolStr(!net.failStack.empty());
    js << "}";
    return js.str();
}

// ---------------- API ----------------

string handleApi(string path, string query, int &status) {
    status = 200;

    if (path == "/api/state")
        return stateJson("", true);

    if (path == "/api/networks") {
        vector<string> names = listNetworks();
        string s = "{\"current\":" + jsonStr(currentFile) + ",\"list\":[";
        for (int i = 0; i < (int)names.size(); i++) {
            if (i > 0) s += ",";
            s += jsonStr(names[i]);
        }
        return s + "]}";
    }

    if (path == "/api/load") {
        string name = getParam(query, "name");
        if (!validName(name) || !net.loadFromFile("data/" + name + ".txt"))
            return stateJson("Could not load network " + name, false);
        currentFile = name;
        return stateJson("Loaded " + net.networkName, true);
    }

    if (path == "/api/save") {
        bool ok = net.saveToFile("data/" + currentFile + ".txt");
        return stateJson(ok ? "Saved to data/" + currentFile + ".txt" : "Save failed", ok);
    }

    if (path == "/api/fail-device") {
        string name = getParam(query, "name");
        bool ok = net.failDevice(name);
        return stateJson(ok ? net.logs.back() : name + " is not found or already down", ok);
    }

    if (path == "/api/restore-device") {
        string name = getParam(query, "name");
        bool ok = net.restoreDevice(name);
        return stateJson(ok ? name + " is back online" : name + " is not down", ok);
    }

    if (path == "/api/fail-link") {
        string a = getParam(query, "a"), b = getParam(query, "b");
        bool ok = net.failLink(a, b);
        return stateJson(ok ? net.logs.back() : "Link not found or already cut", ok);
    }

    if (path == "/api/restore-link") {
        string a = getParam(query, "a"), b = getParam(query, "b");
        bool ok = net.restoreLink(a, b);
        return stateJson(ok ? "Link " + a + " - " + b + " repaired" : "Link is not cut", ok);
    }

    if (path == "/api/undo") {
        bool ok = net.undoLastFailure();
        return stateJson(ok ? net.logs.back() : "Nothing to undo", ok);
    }

    if (path == "/api/restore-all") {
        net.restoreAll();
        return stateJson("Everything restored", true);
    }

    if (path == "/api/add-device") {
        string name = getParam(query, "name"), type = getParam(query, "type");
        int x = atoi(getParam(query, "x").c_str());
        int y = atoi(getParam(query, "y").c_str());
        if (!validName(name))
            return stateJson("Use only letters, numbers, _ and - in the name", false);
        if (net.addDevice(name, type, x, y) == -1)
            return stateJson("A device called " + name + " already exists", false);
        net.addLog("Added device " + name + " (" + type + ")");
        return stateJson("Added " + name, true);
    }

    if (path == "/api/add-link") {
        string a = getParam(query, "a"), b = getParam(query, "b");
        int lat = atoi(getParam(query, "latency").c_str());
        if (!net.addLink(a, b, lat))
            return stateJson("Could not connect " + a + " and " + b, false);
        net.addLog("Added link " + a + " - " + b + " (" + to_string(lat) + " ms)");
        return stateJson("Connected " + a + " and " + b, true);
    }

    if (path == "/api/move") {
        int id = net.getId(getParam(query, "name"));
        if (id != -1) {
            net.devices[id].x = atoi(getParam(query, "x").c_str());
            net.devices[id].y = atoi(getParam(query, "y").c_str());
        }
        return "{\"ok\":true}";
    }

    if (path == "/api/path") {
        int src = net.getId(getParam(query, "from"));
        int dest = net.getId(getParam(query, "to"));
        int cost;
        vector<int> best = net.dijkstra(src, dest, cost);

        vector< vector<int> > paths;
        vector<int> costs;
        net.allPaths(src, dest, paths, costs, 5);

        stringstream js;
        js << "{\"found\":" << boolStr(best.size() > 0) << ",\"path\":" << pathJson(best)
           << ",\"cost\":" << cost << ",\"hops\":" << (best.size() > 0 ? best.size() - 1 : 0)
           << ",\"alternates\":[";
        for (int i = 0; i < (int)paths.size(); i++) {
            if (i > 0) js << ",";
            js << "{\"path\":" << pathJson(paths[i]) << ",\"cost\":" << costs[i] << "}";
        }
        js << "]}";
        return js.str();
    }

    // planned maintenance: links=A:B,C:D,...  (cables in the order they will be removed)
    if (path == "/api/plan") {
        vector<int> plan;
        stringstream ss(getParam(query, "links"));
        string item;
        while (getline(ss, item, ',')) {
            int colon = item.find(':');
            if (colon == (int)string::npos)
                continue;
            int id = net.findLink(item.substr(0, colon), item.substr(colon + 1));
            if (id != -1)
                plan.push_back(id);
        }
        int step = net.breakingPoint(plan);
        string s = "{\"step\":" + to_string(step) + ",\"count\":" + to_string(plan.size());
        if (step > 0) {
            Link &l = net.links[plan[step - 1]];
            s += ",\"a\":" + jsonStr(net.devices[l.u].name) + ",\"b\":" + jsonStr(net.devices[l.v].name);
        }
        return s + "}";
    }

    if (path == "/api/reach") {
        int src = net.getId(getParam(query, "from"));
        vector<int> r = net.bfs(src);
        vector<bool> seen(net.devices.size(), false);
        for (int i = 0; i < (int)r.size(); i++)
            seen[r[i]] = true;
        vector<int> notReached;
        for (int i = 0; i < (int)net.devices.size(); i++)
            if (!seen[i])
                notReached.push_back(i);
        return "{\"reachable\":" + pathJson(r) + ",\"unreachable\":" + pathJson(notReached) + "}";
    }

    status = 404;
    return "{\"ok\":false,\"msg\":\"unknown api\"}";
}

#ifdef __EMSCRIPTEN__
// ---------------- WebAssembly version (for GitHub Pages) ----------------
// The same analyzer compiled with Emscripten. Instead of a socket server,
// the page calls handle_api("/api/state", "") directly and gets the same JSON back.
#include <emscripten.h>

string lastReply;

extern "C" {
EMSCRIPTEN_KEEPALIVE
const char *handle_api(const char *path, const char *query) {
    int status;
    lastReply = handleApi(path, query, status);
    return lastReply.c_str();
}
}

int main() {
    net.loadFromFile("data/" + currentFile + ".txt");
    return 0;
}

#else
// ---------------- static files (html, css, js) ----------------

string contentType(string path) {
    if (path.find(".html") != string::npos) return "text/html; charset=utf-8";
    if (path.find(".css") != string::npos) return "text/css";
    if (path.find(".js") != string::npos) return "application/javascript";
    if (path.find(".png") != string::npos) return "image/png";
    if (path.find(".svg") != string::npos) return "image/svg+xml";
    if (path.find(".woff2") != string::npos) return "font/woff2";
    return "text/plain";
}

bool readFile(string fileName, string &data) {
    ifstream fin(fileName.c_str(), ios::binary);
    if (!fin)
        return false;
    stringstream ss;
    ss << fin.rdbuf();
    data = ss.str();
    return true;
}

void sendReply(SOCKET client, int status, string type, string body) {
    stringstream head;
    head << "HTTP/1.1 " << status << (status == 200 ? " OK" : " Not Found") << "\r\n";
    head << "Content-Type: " << type << "\r\n";
    head << "Content-Length: " << body.size() << "\r\n";
    head << "Cache-Control: no-store\r\n";
    head << "Connection: close\r\n\r\n";
    string all = head.str() + body;

    int sent = 0;
    while (sent < (int)all.size()) {
        int n = send(client, all.c_str() + sent, all.size() - sent, 0);
        if (n <= 0)
            break;
        sent += n;
    }
}

void handleRequest(SOCKET client, string request) {
    // first line looks like: GET /api/state?x=1 HTTP/1.1
    stringstream ss(request);
    string method, url;
    ss >> method >> url;

    string path = url, query = "";
    int q = url.find('?');
    if (q != (int)string::npos) {
        path = url.substr(0, q);
        query = url.substr(q + 1);
    }

    if (path.substr(0, 5) == "/api/") {
        int status;
        string body = handleApi(path, query, status);
        sendReply(client, status, "application/json", body);
        return;
    }

    if (path == "/")
        path = "/index.html";
    string data;
    if (path.find("..") == string::npos && readFile("web" + path, data))
        sendReply(client, 200, contentType(path), data);
    else
        sendReply(client, 404, "text/plain", "Not found");
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    if (!net.loadFromFile("data/" + currentFile + ".txt")) {
        cout << "Run this program from the project folder (the one with data and web inside)." << endl;
        return 1;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // only this computer can connect
    addr.sin_port = htons(PORT);

    if (bind(server, (sockaddr *)&addr, sizeof(addr)) != 0) {
        cout << "Port " << PORT << " is busy. Close the other server and try again." << endl;
        return 1;
    }
    listen(server, 16);
    cout << "Network Failure Analyzer server running" << endl;
    cout << "Open http://localhost:" << PORT << " in your browser (Ctrl+C to stop)" << endl;

    // browsers keep some connections open without sending anything,
    // so we use select() to only read from sockets that have data
    vector<SOCKET> clients;
    vector<string> buffers;
    vector<time_t> openedAt;

    while (true) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(server, &readSet);
        SOCKET maxFd = server;
        for (int i = 0; i < (int)clients.size(); i++) {
            FD_SET(clients[i], &readSet);
            if (clients[i] > maxFd)
                maxFd = clients[i];
        }

        timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        if (select(maxFd + 1, &readSet, NULL, NULL, &tv) < 0)
            continue;

        if (FD_ISSET(server, &readSet) && clients.size() < 50) {
            SOCKET c = accept(server, NULL, NULL);
            if (c != INVALID_SOCKET) {
                clients.push_back(c);
                buffers.push_back("");
                openedAt.push_back(time(0));
            }
        }

        for (int i = clients.size() - 1; i >= 0; i--) {
            bool done = false;
            if (FD_ISSET(clients[i], &readSet)) {
                char buf[4096];
                int n = recv(clients[i], buf, sizeof(buf), 0);
                if (n <= 0)
                    done = true;
                else {
                    buffers[i].append(buf, n);
                    if (buffers[i].find("\r\n\r\n") != string::npos) {
                        handleRequest(clients[i], buffers[i]);
                        done = true;
                    }
                }
            }
            if (time(0) - openedAt[i] > 15)   // idle for too long
                done = true;

            if (done) {
                closesocket(clients[i]);
                clients.erase(clients.begin() + i);
                buffers.erase(buffers.begin() + i);
                openedAt.erase(openedAt.begin() + i);
            }
        }
    }
    return 0;
}
#endif
