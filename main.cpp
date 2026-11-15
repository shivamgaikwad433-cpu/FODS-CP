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
