#include "simulation_engine.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <iostream>

namespace archisys {

SimulationEngine::SimulationEngine() = default;

SimulationEngine::SimulationEngine(SimulationEngine&& o) noexcept {
    std::lock_guard<std::mutex> lk(o.engineMutex_);
    components_         = std::move(o.components_);
    allRequests_        = std::move(o.allRequests_);
    completedLatencies_ = std::move(o.completedLatencies_);
    config_             = o.config_;
    simNow_             = o.simNow_;
    running_.store(o.running_.load());
    nextRequestId_      = o.nextRequestId_;
    entryPointId_       = o.entryPointId_;
    totalRequests_      = o.totalRequests_;
    completedRequests_  = o.completedRequests_;
    failedRequests_     = o.failedRequests_;
    droppedRequests_    = o.droppedRequests_;
    sumLatencyMs_       = o.sumLatencyMs_;
    onTick              = std::move(o.onTick);
}

SimulationEngine& SimulationEngine::operator=(SimulationEngine&& o) noexcept {
    if (this != &o) {
        std::scoped_lock lock(engineMutex_, o.engineMutex_);
        components_         = std::move(o.components_);
        allRequests_        = std::move(o.allRequests_);
        completedLatencies_ = std::move(o.completedLatencies_);
        config_             = o.config_;
        simNow_             = o.simNow_;
        running_.store(o.running_.load());
        nextRequestId_      = o.nextRequestId_;
        entryPointId_       = o.entryPointId_;
        totalRequests_      = o.totalRequests_;
        completedRequests_  = o.completedRequests_;
        failedRequests_     = o.failedRequests_;
        droppedRequests_    = o.droppedRequests_;
        sumLatencyMs_       = o.sumLatencyMs_;
        onTick              = std::move(o.onTick);
    }
    return *this;
}

// ── addComponent ─────────────────────────────────────────────────────────────
void SimulationEngine::addComponent(std::shared_ptr<Component> comp) {
    std::lock_guard<std::mutex> lk(engineMutex_);
    components_[comp->id()] = std::move(comp);
}

// ── addEdge ───────────────────────────────────────────────────────────────────
void SimulationEngine::addEdge(int fromId, int toId) {
    std::lock_guard<std::mutex> lk(engineMutex_);
    Component* from = getComponent(fromId);
    Component* to   = getComponent(toId);
    if (!from || !to) throw std::runtime_error("addEdge: Unknown component ID in architecture edge");
    from->addOutgoing(to);
    to->addIncoming(from);
}

// ── setEntryPoint ─────────────────────────────────────────────────────────────
void SimulationEngine::setEntryPoint(int componentId) {
    std::lock_guard<std::mutex> lk(engineMutex_);
    entryPointId_ = componentId;
}

// ── start ─────────────────────────────────────────────────────────────────────
// Main Simulation Lifecycle:
// Iterates through discrete time increments (config_.tickSec) executing the 5 core
// simulation phases until the duration expires, target requests are satisfied,
// or stop() is requested.
void SimulationEngine::start() {
    if (components_.empty()) throw std::runtime_error("Cannot start: No components registered in engine");

    srand(static_cast<unsigned>(config_.seed));
    running_.store(true);
    double dt = std::max(0.001, config_.tickSec);

    // Main injection and processing loop
    while (running_.load() && simNow_ <= config_.durationSec) {
        // Stop if totalRequests limit is reached and everything is drained
        if (config_.totalRequests > 0 && totalRequests_ >= config_.totalRequests) {
            // Check if any in-flight requests remain in queues or worker slots
            bool hasInFlight = false;
            for (const auto& kv : components_) {
                if (kv.second->queueDepth() > 0 || kv.second->busySlots() > 0) {
                    hasInFlight = true;
                    break;
                }
            }
            if (!hasInFlight) break; // Finished all requests
        }

        // Phase 1: Request generation (Client injection)
        generateRequests(dt);

        // Phase 2: Component state progression & request forwarding
        updateComponents(dt);

        // Phase 3: Telemetry streaming hook
        if (onTick) {
            onTick(getSystemMetrics());
        }

        // Phase 4: Advance simulation clock
        advanceSimulationTime(dt);
    }

    running_.store(false);
}

// ── step ──────────────────────────────────────────────────────────────────────
// Executes a single discrete tick of the simulation.
void SimulationEngine::step(double dtSec) {
    if (components_.empty()) return;
    double dt = dtSec > 0.0 ? dtSec : config_.tickSec;

    generateRequests(dt);
    updateComponents(dt);
    advanceSimulationTime(dt);

    if (onTick) {
        onTick(getSystemMetrics());
    }
}

// ── stop ──────────────────────────────────────────────────────────────────────
void SimulationEngine::stop() {
    running_.store(false);
}

// ── reset ─────────────────────────────────────────────────────────────────────
void SimulationEngine::reset() {
    std::lock_guard<std::mutex> lk(engineMutex_);
    running_.store(false);
    simNow_ = 0.0;
    nextRequestId_ = 1;
    totalRequests_ = 0;
    completedRequests_ = 0;
    failedRequests_ = 0;
    droppedRequests_ = 0;
    sumLatencyMs_ = 0.0;
    allRequests_.clear();
    completedLatencies_.clear();
    for (auto& kv : components_) {
        kv.second->resetStats();
    }
}

// ── Phase 1: generateRequests ────────────────────────────────────────────────
// Simulates user request traffic arrivals during the current tick interval [simNow, simNow + dt].
// Uses a Poisson-distributed random variable matching requestRatePerSec.
void SimulationEngine::generateRequests(double dtSec) {
    // If request ceiling is met, do not generate further requests
    if (config_.totalRequests > 0 && totalRequests_ >= config_.totalRequests) return;

    Component* entry = entryPoint();
    if (!entry) return;

    double expected = config_.requestRatePerSec * dtSec;
    int count = static_cast<int>(expected);
    // Fractional Poisson arrival probability
    double remainder = expected - count;
    if ((static_cast<double>(rand()) / RAND_MAX) < remainder) {
        ++count;
    }

    for (int i = 0; i < count; ++i) {
        if (config_.totalRequests > 0 && totalRequests_ >= config_.totalRequests) break;

        auto req                = std::make_shared<Request>();
        req->id                 = nextRequestId_++;
        req->clientId           = entryPointId_;
        req->createdAt          = simNow_;
        req->completedAt        = 0.0;
        req->status             = RequestStatus::Created;
        req->currentComponentId = -1;
        req->totalLatencyMs     = 0.0;
        req->totalQueueWaitMs   = 0.0;

        allRequests_.push_back(req);
        ++totalRequests_;

        // Place into entry-point component
        bool received = entry->receiveRequest(req, simNow_);
        if (!received) {
            ++droppedRequests_;
        }
    }
}

// ── Phase 2: updateComponents ────────────────────────────────────────────────
// Ticks every component in the architecture graph, schedules waiting requests,
// evaluates worker completion, and routes messages downstream.
void SimulationEngine::updateComponents(double dtSec) {
    for (auto& kv : components_) {
        auto& comp = kv.second;
        auto completed = comp->tick(simNow_, dtSec);

        for (auto& req : completed) {
            if (req->isDone()) {
                if (req->isSuccess()) {
                    ++completedRequests_;
                    // End-to-end round trip latency: (completedAt - createdAt) * 1000.0 ms
                    double roundTripMs = (req->completedAt - req->createdAt) * 1000.0;
                    if (roundTripMs < 0.0) roundTripMs = req->totalLatencyMs + req->totalQueueWaitMs;
                    sumLatencyMs_ += roundTripMs;
                    completedLatencies_.push_back(roundTripMs);
                } else if (req->status == RequestStatus::Dropped) {
                    ++droppedRequests_;
                } else {
                    ++failedRequests_;
                }
            }
        }
    }
}

// ── Phase 4: advanceSimulationTime ───────────────────────────────────────────
void SimulationEngine::advanceSimulationTime(double dtSec) {
    simNow_ += dtSec;
}

// ── getSystemMetrics ─────────────────────────────────────────────────────────
// Calculates system-level averages, throughput, and P99 latency percentiles.
SystemMetrics SimulationEngine::getSystemMetrics() const {
    std::lock_guard<std::mutex> lk(engineMutex_);
    SystemMetrics m;
    m.simTimeSec      = simNow_;
    m.totalRequests   = totalRequests_;
    m.completed       = completedRequests_;
    m.failed          = failedRequests_;
    m.dropped         = droppedRequests_;

    // Calculate in-flight requests currently residing across all components
    uint64_t inFlight = 0;
    for (const auto& kv : components_) {
        inFlight += static_cast<uint64_t>(kv.second->queueDepth() + kv.second->busySlots());
        m.perComponent[kv.first] = kv.second->getMetrics(simNow_);
    }
    m.inFlight = inFlight;

    // Average end-to-end latency
    m.avgLatencyMs = completedRequests_ > 0
        ? (sumLatencyMs_ / static_cast<double>(completedRequests_))
        : 0.0;

    // Accurate 99th Percentile (P99) Latency calculation
    if (!completedLatencies_.empty()) {
        std::vector<double> copy = completedLatencies_;
        size_t idx = static_cast<size_t>(std::ceil(0.99 * copy.size())) - 1;
        if (idx >= copy.size()) idx = copy.size() - 1;
        std::nth_element(copy.begin(), copy.begin() + idx, copy.end());
        m.p99LatencyMs = copy[idx];
    } else {
        m.p99LatencyMs = 0.0;
    }

    // System-wide throughput (completed requests per simulation second)
    m.throughputPerSec = simNow_ > 0.0
        ? (static_cast<double>(completedRequests_) / simNow_)
        : 0.0;

    return m;
}

Component* SimulationEngine::getComponent(int id) const {
    auto it = components_.find(id);
    return it != components_.end() ? it->second.get() : nullptr;
}

Component* SimulationEngine::entryPoint() const {
    return getComponent(entryPointId_);
}

} // namespace archisys
