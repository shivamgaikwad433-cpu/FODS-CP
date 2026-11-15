// main.cpp
// Terminal (menu driven) version of the Network Failure Analyzer.
// Build:  g++ -std=c++11 src/main.cpp src/Network.cpp -o analyzer.exe

#include "Network.h"
#include <limits>

void printPath(Network &net, vector<int> &path) {
    for (int i = 0; i < (int)path.size(); i++) {
        cout << net.devices[path[i]].name;
        if (i != (int)path.size() - 1)
            cout << " -> ";
    }
    cout << endl;
}

int readInt() {
    int x;
    while (!(cin >> x)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Enter a number: ";
    }
    return x;
}

void showMenu() {
    cout << "\n========= NETWORK FAILURE ANALYZER =========" << endl;
    cout << " 1. Show network" << endl;
    cout << " 2. Add device" << endl;
    cout << " 3. Add link" << endl;
    cout << " 4. Fail a device" << endl;
    cout << " 5. Fail a link" << endl;
    cout << " 6. Restore a device" << endl;
    cout << " 7. Restore a link" << endl;
    cout << " 8. Undo last failure" << endl;
    cout << " 9. Restore everything" << endl;
    cout << "10. Shortest path (Dijkstra)" << endl;
    cout << "11. Alternate paths (backtracking)" << endl;
    cout << "12. Reachable devices (BFS)" << endl;
    cout << "13. Connected groups (DFS)" << endl;
    cout << "14. Critical devices and links (Tarjan)" << endl;
    cout << "15. Impact ranking" << endl;
    cout << "16. Failure report" << endl;
    cout << "17. Show logs" << endl;
    cout << "18. Load network from file" << endl;
    cout << "19. Save network to file" << endl;
    cout << "20. Planned cable removals - when does it split? (binary search + DSU)" << endl;
    cout << " 0. Exit" << endl;
    cout << "Choice: ";
}

int main() {
    Network net;
    string fileName = "data/college.txt";

    if (!net.loadFromFile(fileName))
        cout << "Starting with empty network. Use option 18 to load a file." << endl;
    else
        cout << "Loaded " << fileName << endl;

    int choice = -1;
    while (choice != 0) {
        showMenu();
        choice = readInt();

        string a, b, type;
        int lat;

        switch (choice) {
        case 1:
            net.printNetwork();
            break;

        case 2:
            cout << "Device name: ";
            cin >> a;
            cout << "Type (router/switch/server/pc/printer/ap/firewall): ";
            cin >> type;
            if (net.addDevice(a, type) != -1) {
                net.addLog("Added device " + a);
                cout << "Device added." << endl;
            }
            else
                cout << "Name already used!" << endl;
            break;

        case 3:
            cout << "First device: ";
            cin >> a;
            cout << "Second device: ";
            cin >> b;
            cout << "Latency (ms): ";
            lat = readInt();
            if (net.addLink(a, b, lat)) {
                net.addLog("Added link " + a + " - " + b);
                cout << "Link added." << endl;
            }
            else
                cout << "Could not add link (check names / already connected)." << endl;
            break;

        case 4:
            cout << "Device to fail: ";
            cin >> a;
            if (net.failDevice(a)) {
                cout << a << " is DOWN now." << endl;
                net.printReport();
            }
            else
                cout << "Device not found or already down." << endl;
            break;

        case 5:
            cout << "First device: ";
            cin >> a;
            cout << "Second device: ";
            cin >> b;
            if (net.failLink(a, b)) {
                cout << "Link cut." << endl;
                net.printReport();
            }
            else
                cout << "Link not found or already cut." << endl;
            break;

        case 6:
            cout << "Device to restore: ";
            cin >> a;
            if (net.restoreDevice(a))
                cout << a << " is UP again." << endl;
            else
                cout << "Device not found or not failed." << endl;
            break;

        case 7:
            cout << "First device: ";
            cin >> a;
            cout << "Second device: ";
            cin >> b;
            if (net.restoreLink(a, b))
                cout << "Link restored." << endl;
            else
                cout << "Link not found or not failed." << endl;
            break;

        case 8:
            if (net.undoLastFailure())
                cout << "Last failure undone. " << net.logs.back() << endl;
            else
                cout << "Nothing to undo." << endl;
            break;

        case 9:
            net.restoreAll();
            cout << "Everything restored." << endl;
            break;

        case 10: {
            cout << "From: ";
            cin >> a;
            cout << "To: ";
            cin >> b;
            int cost;
            vector<int> path = net.dijkstra(net.getId(a), net.getId(b), cost);
            if (path.size() == 0)
                cout << "No working path between " << a << " and " << b << endl;
            else {
                cout << "Shortest path: ";
                printPath(net, path);
                cout << "Total latency: " << cost << " ms, hops: " << path.size() - 1 << endl;
            }
            break;
        }

        case 11: {
            cout << "From: ";
            cin >> a;
            cout << "To: ";
            cin >> b;
            vector< vector<int> > paths;
            vector<int> costs;
            net.allPaths(net.getId(a), net.getId(b), paths, costs, 5);
            if (paths.size() == 0)
                cout << "No path found." << endl;
            for (int i = 0; i < (int)paths.size(); i++) {
                cout << "Route " << i + 1 << " (" << costs[i] << " ms): ";
                printPath(net, paths[i]);
            }
            if (paths.size() == 1)
                cout << "Only ONE route exists, there is no backup!" << endl;
            break;
        }

        case 12: {
            cout << "Start device: ";
            cin >> a;
            vector<int> r = net.bfs(net.getId(a));
            if (r.size() == 0) {
                cout << "Device not found or it is down." << endl;
                break;
            }
            cout << "Reachable (" << r.size() << "): ";
            for (int i = 0; i < (int)r.size(); i++)
                cout << net.devices[r[i]].name << " ";
            cout << endl;
            break;
        }

        case 13: {
            vector<int> compOf;
            int c = net.countComponents(compOf);
            cout << "Number of groups: " << c << endl;
            for (int g = 0; g < c; g++) {
                cout << "  Group " << g + 1 << ": ";
                for (int i = 0; i < (int)compOf.size(); i++)
                    if (compOf[i] == g)
                        cout << net.devices[i].name << " ";
                cout << endl;
            }
            break;
        }

        case 14: {
            vector<int> crit, bridges;
            net.findCritical(crit, bridges);
            cout << "Critical devices (articulation points): ";
            for (int i = 0; i < (int)crit.size(); i++)
                cout << net.devices[crit[i]].name << " ";
            if (crit.size() == 0) cout << "none";
            cout << endl << "Critical links (bridges): ";
            for (int i = 0; i < (int)bridges.size(); i++) {
                Link &l = net.links[bridges[i]];
                cout << net.devices[l.u].name << "-" << net.devices[l.v].name << " ";
            }
            if (bridges.size() == 0) cout << "none";
            cout << endl;
            break;
        }

        case 15: {
            vector<ImpactScore> r = net.impactRanking();
            cout << "If this device fails, devices that lose connection:" << endl;
            for (int i = 0; i < (int)r.size(); i++)
                cout << "  " << i + 1 << ". " << r[i].name << " : " << r[i].lost << endl;
            break;
        }

        case 16:
            net.printReport();
            break;

        case 17:
            cout << "\n----- LOGS -----" << endl;
            for (int i = 0; i < (int)net.logs.size(); i++)
                cout << net.logs[i] << endl;
            break;

        case 18:
            cout << "File name (eg data/hospital.txt): ";
            cin >> fileName;
            if (net.loadFromFile(fileName))
                cout << "Loaded." << endl;
            break;

        case 19:
            cout << "File name: ";
            cin >> fileName;
            if (net.saveToFile(fileName))
                cout << "Saved." << endl;
            else
                cout << "Could not save." << endl;
            break;

        case 20: {
            int k;
            cout << "How many cables will be removed (in order)? ";
            k = readInt();
            vector<int> plan;
            for (int i = 0; i < k; i++) {
                cout << "Cable " << i + 1 << " (two device names): ";
                cin >> a >> b;
                int id = net.findLink(a, b);
                if (id == -1)
                    cout << "  no such cable, skipped" << endl;
                else
                    plan.push_back(id);
            }
            int step = net.breakingPoint(plan);
            if (step == -1)
                cout << "The network stays connected even after all these removals." << endl;
            else if (step == 0)
                cout << "The network is already split before removing anything." << endl;
            else {
                Link &l = net.links[plan[step - 1]];
                cout << "The network splits at step " << step << " when cable "
                     << net.devices[l.u].name << " - " << net.devices[l.v].name << " is removed." << endl;
            }
            break;
        }

        case 0:
            cout << "Bye!" << endl;
            break;

        default:
            cout << "Invalid choice" << endl;
        }
    }
    return 0;
}
