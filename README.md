# Network Failure Analyzer

FODS course project by **Shivam Gaikwad** (Roll No 2080153) and **Jiteesh Ghodke** (Roll No 2080166).

**Live demo:** https://shivamgaikwad433-cpu.github.io/FODS-CP/ (runs the same C++ analyzer compiled to WebAssembly, nothing to install)

Open source under the [MIT License](LICENSE).

The program stores a computer network as a graph (devices are nodes, cables are edges, latency in ms is the weight). You can fail a device or cut a cable and the analyzer shows which devices lose connection, how bad the failure is, and which other routes still work.

There are two ways to use it:

1. **Terminal version** (`analyzer.exe`): a menu driven program.
2. **Web version** (`server.exe` + `web/` folder): the same C++ code runs as a small local server and the browser shows the network as a 2D map.

## Data structures and algorithms used

| What | Where it is used |
|---|---|
| Adjacency list (`vector<vector<int>>`) | storing the network graph |
| `map<string,int>` | finding a device id from its name |
| BFS (queue) | devices reachable from a device |
| DFS (recursion) | counting connected groups after a failure |
| Dijkstra (priority queue / min heap) | lowest latency path between two devices |
| Backtracking | listing alternate routes |
| Tarjan's algorithm (disc/low arrays) | articulation points (single points of failure) and bridge cables |
| Stack | undo last failure |
| Bubble sort | sorting routes by cost and devices by impact |
| DSU (union-find) + binary search | planned maintenance: first cable removal in a list that splits the network |
| Degree / density | health numbers for each device and the whole network |

## Folder structure

```
src/Network.h      class and structs
src/Network.cpp    all graph algorithms
src/main.cpp       terminal menu
src/server.cpp     small HTTP server for the web version
web/               index.html, style.css, app.js (map drawing)
data/              network files (college, hospital, office)
build.bat          builds both programs
run.bat            builds the server and opens the browser
```

## How to run

Needs a C++ compiler (MinGW g++ works, even old 6.3).

**Web version:** double click `run.bat`, or:

```
g++ -std=c++11 src/server.cpp src/Network.cpp -o server.exe -lws2_32
server.exe
```

then open http://localhost:8080. Run it from the project folder so it can find `data/` and `web/`.

**Terminal version:**

```
g++ -std=c++11 src/main.cpp src/Network.cpp -o analyzer.exe
analyzer.exe
```

## Network file format

```
NAME College Campus
DEVICE <name> <type> <x> <y>
LINK <device1> <device2> <latency_ms>
```

Types: router, switch, server, pc, printer, ap, firewall. `x y` is the position on the web map and can be left out.

## Severity rules

- **NONE**: nothing has failed
- **LOW**: something failed but every other device is still connected
- **MEDIUM**: less than 20% of devices are cut off
- **HIGH**: 20% to 50% cut off
- **CRITICAL**: more than 50% cut off

## Web version controls

- Click a device or cable to select it. `F` fails or repairs it, `U` undoes the last failure.
- Drag devices to move them. **Untangle** re-arranges the map in rows so fewer cables cross.
- Orange `!` marks a single point of failure. A red X on a blue box means that device is cut off from the main network.
- Intel tab also has **Planned maintenance**: list cables in the order you will unplug them and the analyzer finds the step where the network splits.
- Route tab: Dijkstra's best route and the other routes. Intel tab: critical devices, bridge cables, impact ranking and groups.

## GitHub Pages version (docs/)

GitHub Pages can only host static files, so it cannot run `server.exe`. Instead `server.cpp` has a small `#ifdef __EMSCRIPTEN__` part, and the same `Network.cpp` is compiled to WebAssembly with Emscripten. The page then calls the analyzer directly in the browser and gets the same JSON as from the server.

Rebuild `docs/` after changing the C++ code, the web files or the data files:

```
docker run --rm -v "%cd%:/src" -w /src emscripten/emsdk:3.1.74 sh build_pages.sh
```

(or `sh build_pages.sh` if Emscripten is installed). In the online demo, Save only lasts until the tab is closed.

To turn Pages on: repository Settings → Pages → Source "Deploy from a branch" → branch `main`, folder `/docs`.

## License

MIT, see [LICENSE](LICENSE). You can use, change and share this code; please keep the copyright notice.

