#pragma once
#include "request.hpp"
#include "component.hpp"
#include "metrics.hpp"
#include "components.hpp"
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>

namespace archisys {

/**
 * Global configuration parameters for the simulation run.
 */
struct SimulationConfig {
    double durationSec = 60.0;        // Max simulation duration (seconds)
    double tickSec = 0.01;            // Delta time per step (0.01s = 10ms)
    double requestRatePerSec = 100.0; // Injected request rate (req/s)
    uint64_t totalRequests = 0;       // Request limit (0 = run for duration)
    int clientId = -1;                // Entry-point client ID
    uint64_t seed = 42;               // Random seed for reproducibility
};

/**
 * SimulationEngine coordinates graph topology, traffic injection,
 * discrete simulation steps, and metrics aggregation.
 * Supports concurrent background execution via std::thread.
 */
class SimulationEngine {
public:
    SimulationEngine();
    ~SimulationEngine();

    // Prevent copying
    SimulationEngine(const SimulationEngine&) = delete;
    SimulationEngine& operator=(const SimulationEngine&) = delete;

    // Topology setup
    void addComponent(std::shared_ptr<Component> comp);
    void addEdge(int fromId, int toId);
    void setEntryPoint(int componentId);

    SimulationConfig& config() { return config_; }
    const SimulationConfig& config() const { return config_; }

    // Simulation Execution
    void start();
    void step(double dtSec);
    void stop();
    void reset();

    bool isRunning() const { return isRunning_.load(); }
    double getCurrentTime() const { return simNowSec_; }

    // Telemetry
    SystemMetrics getSystemMetrics() const;

    // Callback on every tick
    std::function<void(const SystemMetrics&)> onTick;

private:
    void runSimulationLoop();
    void generateRequests(double dtSec);
    void updateComponents(double dtSec);

    Component* getComponent(int id) const;
    Component* getEntryPoint() const;

    std::unordered_map<int, std::shared_ptr<Component>> components_;
    std::vector<std::shared_ptr<Request>> allRequests_;
    std::vector<double> completedLatenciesMs_;

    SimulationConfig config_;

    double simNowSec_ = 0.0;
    std::atomic<bool> isRunning_{false};
    uint64_t nextRequestId_ = 1;
    int entryPointId_ = -1;

    // Aggregated statistics
    uint64_t totalRequests_ = 0;
    uint64_t completedRequests_ = 0;
    uint64_t failedRequests_ = 0;
    uint64_t droppedRequests_ = 0;
    double sumLatencyMs_ = 0.0;

    std::thread simThread_;
    mutable std::mutex engineMutex_;
};

} // namespace archisys
