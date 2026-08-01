#include "simulation_engine.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <iostream>

namespace archisys {

// ── addComponent ─────────────────────────────────────────────────────────────
void SimulationEngine::addComponent(std::shared_ptr<Component> comp) {
    components_[comp->id()] = std::move(comp);
}

// ── addEdge ───────────────────────────────────────────────────────────────────
void SimulationEngine::addEdge(int fromId, int toId) {
    Component* from = getComponent(fromId);
    Component* to   = getComponent(toId);
    if (!from || !to) throw std::runtime_error("addEdge: unknown component id");
    from->addOutgoing(to);
    to->addIncoming(from);
}

// ── setEntryPoint ─────────────────────────────────────────────────────────────
void SimulationEngine::setEntryPoint(int componentId) {
    entryPointId_ = componentId;
}

// ── start ─────────────────────────────────────────────────────────────────────
// Main simulation loop — advances simNow by config_.tickSec each iteration.
// Events are processed in chronological order; components are ticked every step.
void SimulationEngine::start() {
    if (components_.empty()) throw std::runtime_error("No components registered");

    srand(static_cast<unsigned>(config_.seed));
    running_ = true;
    simNow_  = 0.0;

    // Schedule the first batch of requests
    scheduleNextRequestBatch(0.0);

    // Phase 1: inject requests until totalRequests limit, ticking the engine
    while (running_ && simNow_ <= config_.durationSec &&
           (config_.totalRequests == 0 || totalRequests_ < config_.totalRequests)) {
        while (!eventQueue_.empty() && eventQueue_.top().scheduledAt <= simNow_) {
            auto ev = eventQueue_.top(); eventQueue_.pop(); ev.action();
        }
        tick(simNow_);
        if (onTick) onTick(getSystemMetrics());
        simNow_ += config_.tickSec;
    }

    // Phase 2: drain in-flight requests after injection stops
    // Keep ticking until all components are empty or we time out (10 extra sim-seconds)
    if (running_ && config_.totalRequests > 0) {
        double drainLimit = simNow_ + 10.0;
        while (running_ && simNow_ <= drainLimit) {
            // Check if all components have empty queues and no active slots
            bool allIdle = true;
            for (auto& kv : components_) {
                if (kv.second->queueDepth() > 0) { allIdle = false; break; }
            }
            if (allIdle) break;

            while (!eventQueue_.empty() && eventQueue_.top().scheduledAt <= simNow_) {
                auto ev = eventQueue_.top(); eventQueue_.pop(); ev.action();
            }
            tick(simNow_);
            if (onTick) onTick(getSystemMetrics());
            simNow_ += config_.tickSec;
        }
    }

    running_ = false;
}

// ── stop ──────────────────────────────────────────────────────────────────────
void SimulationEngine::stop() {
    running_ = false;
}

// ── tick ──────────────────────────────────────────────────────────────────────
// Tick all components; collect completed requests and update global stats.
void SimulationEngine::tick(double simNow) {
    for (auto& kv : components_) {
        auto& comp = kv.second;
        auto finished = comp->tick(simNow, config_.tickSec);
        for (auto& req : finished) {
            if (req->isDone()) {
                if (req->isSuccess()) {
                    ++completedRequests_;
                    sumLatencyMs_ += req->totalLatencyMs;
                } else if (req->status == RequestStatus::Dropped) {
                    ++droppedRequests_;
                } else {
                    ++failedRequests_;
                }
                if (onRequestFinished) onRequestFinished(*req);
            }
        }
    }
}

// ── generateRequests ─────────────────────────────────────────────────────────
// Creates a Poisson-distributed batch of requests and injects them at the entry
// point component.
void SimulationEngine::generateRequests(double simNow) {
    Component* entry = entryPoint();
    if (!entry) return;

    // Poisson: expected requests = rate * tickSec
    double expected = config_.requestRatePerSec * config_.tickSec;
    int    count    = static_cast<int>(expected);
    // Fractional probability for one extra request
    if ((double)rand() / RAND_MAX < (expected - count)) ++count;

    for (int i = 0; i < count; ++i) {
        // Stop injecting once totalRequests limit reached
        if (config_.totalRequests > 0 && totalRequests_ >= config_.totalRequests) break;

        auto req              = std::make_shared<Request>();
        req->id               = nextRequestId_++;
        req->clientId         = entryPointId_;
        req->createdAt        = simNow;
        req->completedAt      = 0.0;
        req->status           = RequestStatus::Created;
        req->currentComponentId = -1;
        req->totalLatencyMs   = 0.0;
        req->totalQueueWaitMs = 0.0;

        allRequests_.push_back(req);
        ++totalRequests_;

        entry->receiveRequest(req, simNow);
    }

    // Schedule next batch only if limit not yet reached
    if (config_.totalRequests == 0 || totalRequests_ < config_.totalRequests) {
        scheduleNextRequestBatch(simNow + config_.tickSec);
    }
}

// ── scheduleNextRequestBatch ─────────────────────────────────────────────────
void SimulationEngine::scheduleNextRequestBatch(double simNow) {
    Event ev;
    ev.scheduledAt = simNow;
    ev.action      = [this, simNow]() { generateRequests(simNow); };
    eventQueue_.push(ev);
}

// ── getSystemMetrics ─────────────────────────────────────────────────────────
SystemMetrics SimulationEngine::getSystemMetrics() const {
    SystemMetrics m;
    m.simTimeSec    = simNow_;
    m.totalRequests = totalRequests_;
    m.completed     = completedRequests_;
    m.failed        = failedRequests_;
    m.dropped       = droppedRequests_;

    uint64_t inFlight = 0;
    for (auto& kv : components_) {
        inFlight += kv.second->queueDepth();
        m.perComponent[kv.first] = kv.second->getMetrics(simNow_);
    }
    m.inFlight = inFlight;

    m.avgLatencyMs = completedRequests_ > 0
        ? sumLatencyMs_ / completedRequests_
        : 0.0;

    m.throughputPerSec = simNow_ > 0
        ? double(completedRequests_) / simNow_
        : 0.0;

    return m;
}

// ── getComponent ─────────────────────────────────────────────────────────────
Component* SimulationEngine::getComponent(int id) const {
    auto it = components_.find(id);
    return it != components_.end() ? it->second.get() : nullptr;
}

Component* SimulationEngine::entryPoint() const {
    return getComponent(entryPointId_);
}

} // namespace archisys
