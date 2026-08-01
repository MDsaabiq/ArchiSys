// main.cpp — ArchiSys C++ Simulation Engine
// Reads architecture.json from the frontend export, runs the sim, prints metrics.
//
// Build:
//   mkdir build && cd build
//   cmake .. && cmake --build .
//
// Run:
//   ./archisys_sim ../architecture.json
//   ./archisys_sim             (uses built-in demo: Server → Database)

#include "simulation_engine.hpp"
#include "components/components.hpp"
#include "json_parser.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>

using namespace archisys;

// ── Pretty-print a SystemMetrics snapshot ────────────────────────────────────
void printMetrics(const SystemMetrics& m) {
    std::cout
        << "\r  t=" << std::fixed << std::setprecision(1) << m.simTimeSec << "s"
        << "  total=" << m.totalRequests
        << "  done=" << m.completed
        << "  drop=" << m.dropped
        << "  latency=" << std::setprecision(1) << m.avgLatencyMs << "ms"
        << "  tput=" << std::setprecision(0) << m.throughputPerSec << "req/s"
        << "  inflight=" << m.inFlight
        << "       " << std::flush;
}

// ── Final report ─────────────────────────────────────────────────────────────
void printReport(const SystemMetrics& m) {
    std::cout << "\n\n=== SIMULATION REPORT ===\n";
    std::cout << "  Duration:        " << m.simTimeSec << "s\n";
    std::cout << "  Total Requests:  " << m.totalRequests << "\n";
    std::cout << "  Completed:       " << m.completed << "\n";
    std::cout << "  Failed:          " << m.failed << "\n";
    std::cout << "  Dropped:         " << m.dropped << "\n";
    std::cout << "  Avg Latency:     " << std::fixed << std::setprecision(2) << m.avgLatencyMs << " ms\n";
    std::cout << "  Throughput:      " << m.throughputPerSec << " req/s\n";

    std::cout << "\n  Per-component:\n";
    for (auto& kv : m.perComponent) {
        const auto& cm = kv.second;
        std::cout
            << "    [" << cm.componentType << "] " << cm.componentName
            << "  cpu=" << std::setprecision(1) << cm.cpuUsagePct << "%"
            << "  queue=" << cm.queueDepth
            << "  done=" << cm.requestsCompleted
            << "  drop=" << cm.requestsDropped
            << "  avgLat=" << cm.avgProcessingMs << "ms"
            << "\n";
    }
    std::cout << "=========================\n";
}

int main(int argc, char* argv[]) {
    SimulationEngine engine;

    if (argc >= 2) {
        // ── Load architecture from JSON file ─────────────────────────────
        std::string path = argv[1];
        std::ifstream f(path);
        if (!f.is_open()) {
            std::cerr << "Cannot open file: " << path << "\n";
            return 1;
        }
        nlohmann::json j;
        try {
            f >> j;
            engine = JsonParser::fromJson(j);
        } catch (const std::exception& ex) {
            std::cerr << "JSON parse error: " << ex.what() << "\n";
            return 1;
        }
        std::cout << "Loaded architecture from: " << path << "\n";
    } else {
        // ── Built-in demo: Client → LoadBalancer → Server → Redis → Database ─
        std::cout << "No JSON file provided. Running built-in demo.\n";
        std::cout << "Architecture: LoadBalancer → Server → Redis → Database\n\n";

        auto lb   = std::make_shared<LoadBalancer>(1, "LB-1");
        auto srv1 = std::make_shared<Server>(2, "Server-A");
        auto srv2 = std::make_shared<Server>(3, "Server-B");
        auto cache= std::make_shared<RedisCache>(4, "Redis-1");
        auto db   = std::make_shared<Database>(5, "DB-Primary");

        // Config
        lb->algorithm     = LBAlgorithm::LeastConnections;
        srv1->processingMs= 30.0;
        srv2->processingMs= 45.0;
        cache->hitRatio   = 0.75;
        db->queryLatencyMs= 15.0;

        // Wiring
        engine.addComponent(lb);
        engine.addComponent(srv1);
        engine.addComponent(srv2);
        engine.addComponent(cache);
        engine.addComponent(db);

        engine.addEdge(lb->id(), srv1->id());
        engine.addEdge(lb->id(), srv2->id());
        engine.addEdge(srv1->id(), cache->id());
        engine.addEdge(srv2->id(), cache->id());
        engine.addEdge(cache->id(), db->id());

        engine.setEntryPoint(lb->id());
    }

    // ── Simulation config ─────────────────────────────────────────────────
    engine.config().durationSec       = 10.0;   // 10 simulated seconds
    engine.config().tickSec           = 0.01;   // 10ms ticks
    engine.config().requestRatePerSec = 200.0;  // 200 requests/sec

    // ── Hook: print progress every 0.5s ──────────────────────────────────
    double lastPrint = 0.0;
    engine.onTick = [&](const SystemMetrics& m) {
        if (m.simTimeSec - lastPrint >= 0.5) {
            printMetrics(m);
            lastPrint = m.simTimeSec;
        }
    };

    std::cout << "Running simulation...\n";
    engine.start();

    printReport(engine.getSystemMetrics());
    return 0;
}
