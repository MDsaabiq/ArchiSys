#pragma once
#include "request.hpp"
#include "component.hpp"
#include "metrics.hpp"
#include "components/components.hpp"
#include <atomic>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <queue>
#include <string>
#include <cstdint>

namespace archisys {

// ── Event ─────────────────────────────────────────────────────────────────────
// Events drive the simulation. Each event has a scheduled time and a callback.
struct Event {
    double                   scheduledAt;   // simulation seconds
    std::function<void()>    action;

    bool operator>(const Event& o) const { return scheduledAt > o.scheduledAt; }
};

using EventQueue = std::priority_queue<Event, std::vector<Event>, std::greater<Event>>;

// ── SimulationConfig ──────────────────────────────────────────────────────────
struct SimulationConfig {
    double   durationSec       = 60.0;   // how long to run (sim seconds)
    double   tickSec           = 0.01;   // simulation tick step (10ms)
    double   requestRatePerSec = 100.0;  // requests generated per second
    uint64_t totalRequests     = 0;      // stop after this many (0 = unlimited)
    int      clientId          = -1;     // which node is the entry-point client
    uint64_t seed              = 42;
};

// ── SimulationEngine ──────────────────────────────────────────────────────────
// Drives the entire simulation: generates requests, ticks all components,
// collects metrics, fires events.
class SimulationEngine {
public:
    SimulationEngine() = default;
    ~SimulationEngine() = default;
    // atomic<bool> deletes copy — provide explicit move
    SimulationEngine(SimulationEngine&&) = default;
    SimulationEngine& operator=(SimulationEngine&&) = default;

    // ── Setup ──────────────────────────────────────────────────────────────

    // Register a component. Engine takes ownership.
    void addComponent(std::shared_ptr<Component> comp);

    // Add a directed edge: requests flow from → to
    void addEdge(int fromId, int toId);

    // Entry-point: requests are injected into this component
    void setEntryPoint(int componentId);

    SimulationConfig& config() { return config_; }

    // ── Lifecycle ──────────────────────────────────────────────────────────

    void start();   // begins the sim loop (blocking until durationSec)
    void stop();    // interrupt early
    bool running() const { return running_; }

    // ── Metrics snapshot (call any time during or after sim) ───────────────
    SystemMetrics getSystemMetrics() const;

    // ── Callback hooks ────────────────────────────────────────────────────
    // Called every tick with a fresh SystemMetrics snapshot.
    // Use this to stream data to the API layer.
    std::function<void(const SystemMetrics&)> onTick;

    // Called when a request completes or is dropped.
    std::function<void(const Request&)>       onRequestFinished;

private:
    void tick(double simNow);
    void generateRequests(double simNow);
    void scheduleNextRequestBatch(double simNow);

    Component* getComponent(int id) const;
    Component* entryPoint() const;

    std::unordered_map<int, std::shared_ptr<Component>> components_;
    std::vector<std::shared_ptr<Request>> allRequests_;

    SimulationConfig config_;
    EventQueue       eventQueue_;

    double            simNow_   = 0.0;
    std::atomic<bool> running_  {false};
    uint64_t nextRequestId_= 1;
    int      entryPointId_ = -1;

    // Accumulated system stats
    uint64_t totalRequests_    = 0;
    uint64_t completedRequests_= 0;
    uint64_t failedRequests_   = 0;
    uint64_t droppedRequests_  = 0;
    double   sumLatencyMs_     = 0.0;
};

} // namespace archisys
