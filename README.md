# Current multi-location implementation

The reusable C++ graph engine is in `cpp/Network.h`, and the REST server in `backend/server.cpp` is the shared source of truth for the browser and terminal. `main.cpp` is an interactive terminal client of that API, so terminal changes are saved to the same location data that the web dashboard reads. The original offline CLI is preserved in `cpp/legacy_cli.cpp`. The REST service creates independent network snapshots under `data/locations/<id>/network.json`.

To build, start, and open the dashboard on Windows, run this from the project root in PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\start-dashboard.ps1
```

The launcher builds the REST server with the installed MSYS2 UCRT64 MinGW-w64 compiler, adds its runtime DLL directory for the server process, waits for the locations API to respond, and opens `http://localhost:8080/`. If another service owns port 8080, it reports that instead of terminating that process.

To build the terminal client, start the shared server if needed, and then use the terminal menu, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-terminal-client.ps1
```

Choose the same location in the terminal and browser dashboard. Dashboard data refreshes automatically after terminal changes.

Build the REST server with a MinGW-w64 compiler that has C++ thread support enabled:

```powershell
g++ -std=c++11 -pthread -Ibackend -Icpp backend/server.cpp -o server.exe -lws2_32
```

Run `server.exe` from the project root and open `http://localhost:8080`. Some older MinGW installations lack `std::thread`/`std::mutex`; use a POSIX-thread MinGW-w64 build in that case. The offline legacy CLI can be built with `g++ -std=c++11 cpp/legacy_cli.cpp -o legacy-analyzer.exe`.

Core API endpoints use `locationId` as a query parameter: `GET /api/locations`, `GET /api/network`, `GET /api/statistics`, `GET /api/devices`, `GET /api/connections`, `GET /api/logs`, `GET /api/critical-devices`, `GET /api/path?source=<id>&destination=<id>`, `GET /api/alternative-paths?source=<id>&destination=<id>`, `POST /api/locations`, `POST /api/devices`, `POST /api/connections`, `POST /api/simulate-failure`, `POST /api/restore`, and `POST /api/restore-all`. Device and connection updates/deletes use `/api/devices/<id>` and `/api/connections/<id>`.

With the server running, exercise the main integration flow using `.\scripts\verify-api.ps1`. The script creates and removes a temporary location while checking routing, critical-device detection, failure isolation, restoration, and JSON persistence.

---
# Network Failure Analyzer — Cybersecurity Web Dashboard

A modern, interactive, cybersecurity-themed Network Operations Center (NOC) and Security Operations Center (SOC) dashboard built on top of a high-performance **C++ Graph & Network Failure Engine**.

This project transforms a terminal-based C++ network failure analyzer into a real-time web application without rewriting or discarding core Data Structures and Algorithms (DSA). All graph algorithms, failure simulations, routing computations, and topological audits are executed natively in C++.

---

## 🛡️ Key Features

- **Live C++ Network Simulation Engine**: The browser dashboard connects directly to the C++ HTTP backend via REST API; no fake or mock network data is generated in JavaScript.
- **Interactive Cytoscape.js Topology**:
  - Live visual network graph with nodes (devices) and edges (connections with latencies).
  - Real-time visual feedback: healthy devices in neon green, failed devices in neon red, critical articulation points in amber.
  - Interactive node & link inspector drawer with live simulation actions.
  - Multiple layout algorithms: Force-directed (*Cose*), Circular, Concentric, Breadthfirst (BFS), and Grid.
- **Core DSA Algorithms (Preserved from C++)**:
  - **Dijkstra's Algorithm (Min-Heap Priority Queue)**: Computes the lowest-latency path and hop counts, automatically steering around failed elements.
  - **Tarjan's Algorithm (Articulation Points & Bridges)**: Detects Single Points of Failure (SPOFs) in $O(V + E)$ time.
  - **Depth-First Search (DFS)**: Detects connected components and network partition status when bridges or nodes fail.
  - **Breadth-First Search (BFS)**: Computes reachability trees and isolated devices from any source node.
  - **Backtracking Simple Paths**: Exhaustively discovers alternative redundant routes and warns when single paths lack backup redundancy.
- **Cybersecurity SOC Control Deck**:
  - Simulate node failure (device down).
  - Simulate physical link cut (connection severance).
  - Individual device and connection restoration.
  - Master recovery (*Restore All*).
  - Dynamic severity rating (*NONE, LOW, MEDIUM, HIGH, CRITICAL*) and incident recommendations.
- **Audit Logging & Telemetry**:
  - Real-time event ledger recording failure and recovery timestamps.
  - Export audit logs to CSV (`/api/export-logs`).
  - Network state snapshot persistence (`data/network.json`).
  - Dynamic Chart.js visualizations for device availability, connection health, and latency distributions.

---

## 📁 Project Structure

```
Network-Failure-Analyzer/
│
├── main.cpp                     # Terminal client connected to the live dashboard API
│
├── cpp/
│   ├── Network.h                # Shared C++ network graph and analysis engine
│   ├── cli_main.cpp             # Existing legacy menu application
│   └── legacy_cli.cpp           # Original offline CLI saved from main.cpp
│
├── backend/
│   ├── server.cpp               # C++ HTTP REST API server
│   ├── httplib.h                # Single-header HTTP server (Winsock2 compatible)
│   └── json.hpp                 # nlohmann/json C++ JSON serialization library
│
├── frontend/
│   ├── index.html               # Cybersecurity SOC dashboard markup
│   ├── style.css                # Dark SOC theme styling (neon accents, glassmorphism)
│   ├── script.js                # Frontend controller (Cytoscape, Chart.js, REST fetch)
│   └── assets/
│       ├── cytoscape.min.js     # Vendored Cytoscape.js library
│       └── chart.umd.min.js     # Vendored Chart.js library
│
├── data/
│   └── network.json             # Live network topology state snapshot
│
├── reports/
│   └── failure_logs.csv         # Exported CSV failure audit log
│
├── scripts/
│   ├── start-dashboard.ps1      # Build/start the REST server and browser
│   └── run-terminal-client.ps1  # Start the server, then build/run the main.cpp client
│
└── README.md                    # System documentation and compilation guide
```

---

## ⚙️ Prerequisites & Dependencies

- **Operating System**: Windows 10 / 11 (or Linux / macOS).
- **C++ Compiler**: MinGW `g++` (GCC 6.3.0 or higher) supporting C++11.
- **Windows System Libraries**: Winsock2 (`-lws2_32`).
- **Web Browser**: Any modern browser (Google Chrome, Microsoft Edge, Mozilla Firefox, Brave).

> [!NOTE]
> All external libraries (`nlohmann/json`, `httplib.h`, `cytoscape.min.js`, and `chart.umd.min.js`) are fully bundled within this repository. No package managers (npm, pip, vcpkg) are required.

---

## 🔨 Windows Compilation Instructions

Open **PowerShell** or **Command Prompt** in the project directory:

```powershell
cd "c:\Second year\1st Semester\C++ assignment of the college\Network Failure Analyzer"
```

### 1. Compile the Web Backend Server
```powershell
g++ -std=c++11 -pthread -Ibackend -Icpp backend/server.cpp -o server.exe -lws2_32
```

### 2. Compile the dashboard-connected terminal client
```powershell
g++ -std=c++11 -pthread -Ibackend main.cpp -o analyzer.exe -lws2_32
```

---

## 🚀 Execution & Usage

### Option A: Running the Web Dashboard (Recommended)

1. **Build and start the C++ HTTP Backend**:
   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\start-dashboard.ps1
   ```

2. **Open the Dashboard in your Browser**:
   Open:
   ```
   http://localhost:8080
   
   ```
   *(Or run: `Start-Process "http://localhost:8080"`)*

3. The launcher keeps the REST server running in the background and checks its API before reporting ready.

---

### Option B: Running the Terminal CLI Application

Run the dashboard-connected terminal client:
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-terminal-client.ps1
```
The terminal menu writes to the same server data as the browser. Choose the same location in both interfaces; the dashboard refreshes after terminal changes.

---

## 📡 REST API Reference

| Method | Endpoint | Description |
| :--- | :--- | :--- |
| `GET` | `/api/network` | Returns complete list of devices, connections, and states. |
| `GET` | `/api/statistics` | Returns node counts, connection counts, uptime %, component count, and partition status. |
| `GET` | `/api/critical-devices` | Runs Tarjan's algorithm; returns articulation points and bridge connections. |
| `GET` | `/api/failures` | Returns list of currently down devices, severed links, and event history. |
| `GET` | `/api/path?source=A&destination=B` | Runs Dijkstra's algorithm; returns shortest route, latency (ms), and hop count. |
| `GET` | `/api/alternative-paths?source=A&destination=B` | Runs backtracking search; returns up to 5 simple alternative paths. |
| `GET` | `/api/logs` | Returns structured audit events (`id`, `timestamp`, `type`, `target`, `status`, `description`). |
| `GET` | `/api/export-logs` | Generates and downloads `failure_logs.csv`. |
| `POST` | `/api/simulate-failure` | Triggers failure. Body: `{"type":"device","name":"Switch1"}` or `{"type":"connection","source":"A","target":"B"}`. |
| `POST` | `/api/restore-device` | Restores failed node. Body: `{"name":"Switch1"}`. |
| `POST` | `/api/restore-connection` | Restores severed link. Body: `{"source":"A","target":"B"}`. |
| `POST` | `/api/restore-all` | Restores all devices and connections to baseline health. |
| `POST` | `/api/reset` | Resets topology to default sample network. |
| `POST` | `/api/add-device` | Adds a new node. Body: `{"name":"Router2"}`. |
| `POST` | `/api/add-connection` | Adds an edge. Body: `{"source":"A","target":"B","latency":5}`. |
| `POST` | `/api/import-topology` | Bulk imports a complete custom topology. Body: `{"connections":[{"source":"A","target":"B","latency":5}]}`. |

---

## 🌐 How to Add Real Network Data

You can add real network data (enterprise topologies, campus networks, custom subnets, real measured latencies) using any of the four methods below:

### Method 1: Directly from the Web Dashboard (No Code Required)
1. Open the dashboard at `http://localhost:8080`.
2. Navigate to **Device Management** in the left sidebar:
   - Click **`+ Add Device`** to create a new device (e.g. `CoreRouter`, `Firewall`, `DB-Primary`).
   - Click **`+ Add Connection`** to create a physical link between any two devices and specify its real latency in milliseconds.
3. The graph and statistics update instantly in C++ memory.

### Method 2: Import Custom JSON in the Web Dashboard
1. In the dashboard, open **Settings**.
2. Scroll to **Import Real Network Topology (JSON)**.
3. Paste your real network data (or click **"Load Enterprise Preset"** to see an example) and click **"Import to C++ Engine"**.
4. The dashboard automatically switches to the interactive topology map showing your custom network.

### Method 3: Via Configuration File (`data/network_config.json`)
You can define your entire network ahead of time in `data/network_config.json`:
```json
{
  "connections": [
    { "source": "Core-Router", "target": "Dist-Switch-1", "latency": 2 },
    { "source": "Core-Router", "target": "Dist-Switch-2", "latency": 3 },
    { "source": "Dist-Switch-1", "target": "Web-Cluster", "latency": 5 },
    { "source": "Dist-Switch-1", "target": "Database-Master", "latency": 2 },
    { "source": "Dist-Switch-2", "target": "Office-LAN", "latency": 8 }
  ]
}
```
Whenever `server.exe` starts, it automatically detects and loads `data/network_config.json`!

### Method 4: Directly in C++ Code
In `cpp/Network.h` (or your `cli_main.cpp` / `main.cpp`), call:
```cpp
net.addConnection("YourDeviceA", "YourDeviceB", latency_in_ms);
```
Recompile with:
```powershell
g++ -std=c++11 -Ibackend -Icpp backend/server.cpp -o server.exe -lws2_32
```

---

## 🧪 Verified Functional Testing

All required test scenarios have been programmatically executed against `server.exe` and verified:

1. **Static File Delivery**: `index.html`, `style.css`, `script.js`, `cytoscape.min.js`, and `chart.umd.min.js` served with HTTP 200.
2. **Initial Topology**: 7 devices and 7 connections loaded from C++ baseline.
3. **Shortest Path Calculation**: Router1 $\to$ PC1 computed via Dijkstra: `Router1 -> Switch2 -> Switch3 -> PC1` ($11\text{ ms}$, 3 hops).
4. **Alternative Routes**: Backtracking discovers both primary path ($7\text{ ms}$) and alternate path ($11\text{ ms}$) from Router1 to Server1.
5. **Critical Element Detection**: Tarjan's algorithm correctly flags `Switch2` and `Switch3` as articulation points and `Switch2-Switch3`, `Switch3-PC1`, `Switch3-PC2` as bridges.
6. **Device Failure Simulation**: Failing `Switch1` drops network availability to $85.7\%$ and updates live logs.
7. **Network Partitioning**: Severing bridge link `Switch2-Switch3` cuts PC1 off from Router1 (`Success: false, No route exists`), splitting the graph into 2 isolated DFS components with `CRITICAL` severity rating.
8. **Restoration & Recovery**: Restoring `Switch1` and `Switch2-Switch3` returns network status to `CONNECTED` with $100\%$ availability.
9. **Disconnection Detection**: When the C++ server process is terminated, the frontend status indicator switches to `BACKEND DISCONNECTED` with automatic retry polling.

---

## 👥 Credits

Developed for the **Network Failure Analyzer** C++ Project Assignment.
Engineered using C++11, `cpp-httplib`, `nlohmann/json`, Cytoscape.js, and Chart.js.


# FODS-CP

