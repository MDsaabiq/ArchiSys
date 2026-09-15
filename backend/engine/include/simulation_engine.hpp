#pragma once
#include "request.hpp"
#include "component.hpp"
#include "metrics.hpp"
#include "components.hpp"
#include <atomic>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <string>
#include <cstdint>
#include <mutex>

namespace archisys {

/**
 * Global configuration parameters for the simulation execution.
 */
struct SimulationConfig {
    double   durationSec       = 60.0;  // Total simulation time limit in seconds
    double   tickSec           = 0.01;  // Simulation time delta per tick (0.01s = 10ms)
    double   requestRatePerSec = 100.0; // Injected request rate (req/s)
    uint64_t totalRequests     = 0;     // Max requests to inject (0 = unlimited, run for duration)
    int      clientId          = -1;    // ID of entry point Client node
    uint64_t seed              = 42;    // Deterministic random seed
};

/**
 * SimulationEngine is the central discrete-event/discrete-tick coordinator.
 * It manages components, network graph topology, request generation,
 * simulation ticks, and telemetry aggregation.
 */
class SimulationEngine {
public:
    SimulationEngine();
    ~SimulationEngine() = default;

    // Non-copyable due to atomic state
    SimulationEngine(const SimulationEngine&) = delete;
    SimulationEngine& operator=(const SimulationEngine&) = delete;

    // Movable
    SimulationEngine(SimulationEngine&&) noexcept;
    SimulationEngine& operator=(SimulationEngine&&) noexcept;

    // ── Topology Construction ────────────────────────────────────────────────
    void addComponent(std::shared_ptr<Component> comp);
    void addEdge(int fromId, int toId);
    void setEntryPoint(int componentId);

    SimulationConfig& config() { return config_; }
    const SimulationConfig& config() const { return config_; }

    // ── Simulation Lifecycle Control ─────────────────────────────────────────

    /**
     * Runs the simulation loop until completion (duration reached, totalRequests
     * processed, or stop() called).
     */
    void start();

    /**
     * Executes a single discrete simulation tick (dtSec).
     */
    void step(double dtSec);

    /**
     * Signals the running simulation loop to halt immediately.
     */
    void stop();

    /**
     * Resets simulation clocks, requests, and component states.
     */
    void reset();

    bool isRunning() const { return running_.load(); }
    double currentTime() const { return simNow_; }

    // ── Metrics & Telemetry ──────────────────────────────────────────────────
    SystemMetrics getSystemMetrics() const;

    // Optional callback executed every simulation tick
    std::function<void(const SystemMetrics&)> onTick;

private:
    // Core simulation phases (executed in strict sequence every tick)
    void generateRequests(double dtSec);
    void updateComponents(double dtSec);
    void checkDraining();
    void calculateMetrics();
    void advanceSimulationTime(double dtSec);

    Component* getComponent(int id) const;
    Component* entryPoint() const;

    std::unordered_map<int, std::shared_ptr<Component>> components_;
    std::vector<std::shared_ptr<Request>> allRequests_;
    std::vector<double> completedLatencies_;

    SimulationConfig config_;

    double            simNow_   = 0.0;
    std::atomic<bool> running_  {false};
    uint64_t          nextRequestId_ = 1;
    int               entryPointId_  = -1;

    // Cumulative system counters
    uint64_t totalRequests_     = 0;
    uint64_t completedRequests_ = 0;
    uint64_t failedRequests_    = 0;
    uint64_t droppedRequests_   = 0;
    double   sumLatencyMs_      = 0.0;

    mutable std::mutex engineMutex_;
};

} // namespace archisys
