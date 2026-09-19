#include "simulation_engine.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <iostream>

namespace archisys {

SimulationEngine::SimulationEngine() = default;

SimulationEngine::~SimulationEngine() {
    stop();
}

void SimulationEngine::addComponent(std::shared_ptr<Component> comp) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    if (comp) {
        components_[comp->getId()] = comp;
    }
}

void SimulationEngine::addEdge(int fromId, int toId) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    Component* from = getComponent(fromId);
    Component* to = getComponent(toId);
    if (!from || !to) {
        throw std::runtime_error("Cannot add edge: Unknown component ID");
    }
    from->addOutgoing(to);
    to->addIncoming(from);
}

void SimulationEngine::setEntryPoint(int componentId) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    entryPointId_ = componentId;
}

// ── Start Simulation ─────────────────────────────────────────────────────────
// Runs the simulation until completion or stopped.
void SimulationEngine::start() {
    if (components_.empty()) {
        throw std::runtime_error("Cannot start simulation: No components registered");
    }

    srand(static_cast<unsigned>(config_.seed));
    isRunning_.store(true);
    double dt = config_.tickSec > 0.0 ? config_.tickSec : 0.01;

    while (isRunning_.load() && simNowSec_ <= config_.durationSec) {
        // If request quota is reached, check if in-flight requests are drained
        if (config_.totalRequests > 0 && totalRequests_ >= config_.totalRequests) {
            bool hasInFlight = false;
            for (const auto& kv : components_) {
                if (kv.second->getQueueDepth() > 0 || kv.second->getBusyWorkers() > 0) {
                    hasInFlight = true;
                    break;
                }
            }
            if (!hasInFlight) break; // All requests completely processed
        }

        step(dt);
    }

    isRunning_.store(false);
}

// ── Step Simulation ──────────────────────────────────────────────────────────
// Executes a single discrete time increment (dtSec).
void SimulationEngine::step(double dtSec) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    if (components_.empty()) return;

    double dt = dtSec > 0.0 ? dtSec : 0.01;

    // 1. Generate new requests at entry point
    generateRequests(dt);

    // 2. Update all components in the topology
    updateComponents(dt);

    // 3. Advance simulation clock
    simNowSec_ += dt;

    // 4. Optional on-tick callback
    if (onTick) {
        onTick(getSystemMetrics());
    }
}

void SimulationEngine::stop() {
    isRunning_.store(false);
    if (simThread_.joinable()) {
        simThread_.join();
    }
}

void SimulationEngine::reset() {
    std::lock_guard<std::mutex> lock(engineMutex_);
    stop();
    simNowSec_ = 0.0;
    nextRequestId_ = 1;
    totalRequests_ = 0;
    completedRequests_ = 0;
    failedRequests_ = 0;
    droppedRequests_ = 0;
    sumLatencyMs_ = 0.0;
    allRequests_.clear();
    completedLatenciesMs_.clear();

    for (auto& kv : components_) {
        kv.second->reset();
    }
}

// ── Generate Requests (Poisson Arrival Model) ────────────────────────────────
void SimulationEngine::generateRequests(double dtSec) {
    if (config_.totalRequests > 0 && totalRequests_ >= config_.totalRequests) return;

    Component* entry = getEntryPoint();
    if (!entry) return;

    double expectedArrivals = config_.requestRatePerSec * dtSec;
    int count = static_cast<int>(expectedArrivals);
    double remainder = expectedArrivals - count;

    // Random fractional arrival
    if ((static_cast<double>(rand()) / RAND_MAX) < remainder) {
        count++;
    }

    for (int i = 0; i < count; i++) {
        if (config_.totalRequests > 0 && totalRequests_ >= config_.totalRequests) break;

        auto req = std::make_shared<Request>();
        req->id = nextRequestId_++;
        req->clientId = entryPointId_;
        req->createdAtSec = simNowSec_;
        req->status = RequestStatus::Created;

        allRequests_.push_back(req);
        totalRequests_++;

        bool received = entry->receiveRequest(req, simNowSec_);
        if (!received) {
            droppedRequests_++;
        }
    }
}

// ── Update Components ────────────────────────────────────────────────────────
void SimulationEngine::updateComponents(double dtSec) {
    for (auto& kv : components_) {
        auto finished = kv.second->update(dtSec, simNowSec_);

        for (auto& req : finished) {
            if (req->isDone()) {
                if (req->isSuccess()) {
                    completedRequests_++;
                    double roundTripMs = req->totalRoundTripMs();
                    if (roundTripMs <= 0.0) {
                        roundTripMs = (req->completedAtSec - req->createdAtSec) * 1000.0;
                    }
                    if (roundTripMs < 0.0) roundTripMs = 0.0;

                    sumLatencyMs_ += roundTripMs;
                    completedLatenciesMs_.push_back(roundTripMs);
                } else if (req->status == RequestStatus::Dropped) {
                    droppedRequests_++;
                } else {
                    failedRequests_++;
                }
            }
        }
    }
}

// ── Get System Metrics ───────────────────────────────────────────────────────
SystemMetrics SimulationEngine::getSystemMetrics() const {
    SystemMetrics m;
    m.simTimeSec = simNowSec_;
    m.totalRequests = totalRequests_;
    m.completed = completedRequests_;
    m.failed = failedRequests_;
    m.dropped = droppedRequests_;

    uint64_t inFlight = 0;
    for (const auto& kv : components_) {
        inFlight += static_cast<uint64_t>(kv.second->getQueueDepth() + kv.second->getBusyWorkers());
        m.perComponent[kv.first] = kv.second->getMetrics();
    }
    m.inFlight = inFlight;

    // Average latency
    m.avgLatencyMs = completedRequests_ > 0
        ? (sumLatencyMs_ / static_cast<double>(completedRequests_))
        : 0.0;

    // Accurate 99th Percentile (P99) Latency using std::nth_element
    if (!completedLatenciesMs_.empty()) {
        std::vector<double> latenciesCopy = completedLatenciesMs_;
        size_t idx = static_cast<size_t>(std::ceil(0.99 * latenciesCopy.size())) - 1;
        if (idx >= latenciesCopy.size()) idx = latenciesCopy.size() - 1;

        std::nth_element(latenciesCopy.begin(), latenciesCopy.begin() + idx, latenciesCopy.end());
        m.p99LatencyMs = latenciesCopy[idx];
    } else {
        m.p99LatencyMs = 0.0;
    }

    // System throughput (completed requests per simulated second)
    m.throughputPerSec = simNowSec_ > 0.0
        ? (static_cast<double>(completedRequests_) / simNowSec_)
        : 0.0;

    return m;
}

Component* SimulationEngine::getComponent(int id) const {
    auto it = components_.find(id);
    return it != components_.end() ? it->second.get() : nullptr;
}

Component* SimulationEngine::getEntryPoint() const {
    return getComponent(entryPointId_);
}

} // namespace archisys
