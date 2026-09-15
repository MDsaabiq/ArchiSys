#include "component.hpp"
#include <algorithm>
#include <cmath>

namespace archisys {

Component::Component(int id, std::string name, std::string type)
    : id_(id), name_(std::move(name)), type_(std::move(type)) {}

// ── receiveRequest ────────────────────────────────────────────────────────────
// Called when a request arrives at this component's door.
// If the internal wait queue has reached its max capacity (maxQueue), the request
// cannot be buffered and is dropped immediately (overflow drop).
bool Component::receiveRequest(std::shared_ptr<Request> req, double simNow) {
    if (static_cast<int>(waitQueue_.size()) >= maxQueue) {
        req->status = RequestStatus::Dropped;
        req->completedAt = simNow;
        ++droppedCount_;
        return false;
    }

    req->status = RequestStatus::InQueue;
    req->currentComponentId = id_;

    // Log arrival hop on the request's journey
    RouteHop hop;
    hop.componentId      = id_;
    hop.componentName    = name_;
    hop.arrivalTime      = simNow;
    hop.processingStart  = 0.0;
    hop.departureTime    = 0.0;
    hop.queueWaitTimeMs  = 0.0;
    hop.processingTimeMs = 0.0;
    req->route.push_back(hop);

    waitQueue_.push(req);
    ++rxCount_;
    return true;
}

// ── computeProcessingDurationMs ───────────────────────────────────────────────
// Default base processing duration based on user-configured processingMs (procTime).
// If cpuCores is configured and concurrent active slots exceed cores, slight CPU
// contention scaling is applied.
double Component::computeProcessingDurationMs(const std::shared_ptr<Request>& /*req*/, double /*simNow*/) const {
    double duration = processingMs;
    // Client components have 0ms latency
    if (duration <= 0.0) return 0.0;

    int busy = busySlots();
    if (cpuCores > 0 && busy > cpuCores) {
        // High core contention: processing takes proportionally longer
        double contentionFactor = static_cast<double>(busy) / static_cast<double>(cpuCores);
        duration *= std::min(2.5, contentionFactor); // cap contention overhead at 2.5x
    }
    return duration;
}

// ── tick ─────────────────────────────────────────────────────────────────────
// Executes one simulation time step (tick):
//
// 1. Completion check: Any request currently occupying an active worker slot
//    whose scheduled finishAt has arrived is marked finished and freed from the slot.
//
// 2. Scheduling: Available free worker slots (up to 'instances') pull waiting
//    requests from the FIFO waitQueue. Queue wait time (processingStart - arrivalTime)
//    is recorded and the slot finish timestamp (simNow + procDuration) is set.
//
// 3. Metrics updates: Active worker slot occupancy determines instant CPU utilization;
//    rolling throughput is calculated based on completed transactions.
//
// 4. Forwarding: Completed requests are dispatched to downstream components.
std::vector<std::shared_ptr<Request>> Component::tick(double simNow, double /*dtSec*/) {
    std::vector<std::shared_ptr<Request>> completed;

    // ① Check for requests that finished processing in active worker slots
    for (auto& slot : activeSlots_) {
        if (slot.req != nullptr && simNow >= slot.finishAt) {
            auto req = slot.req;
            req->status = RequestStatus::Forwarded;

            double actualProcMs = (simNow - slot.startedAt) * 1000.0;
            if (actualProcMs < 0.0) actualProcMs = 0.0;

            sumProcessingMs_ += actualProcMs;
            req->totalLatencyMs += actualProcMs;

            // Finalize the current hop telemetry
            if (!req->route.empty()) {
                auto& hop = req->route.back();
                hop.departureTime    = simNow;
                hop.processingTimeMs = actualProcMs;
            }

            ++txCount_;
            completed.push_back(req);
            slot.req = nullptr; // Free this worker instance slot
        }
    }

    // ② Ensure activeSlots_ has exactly `instances` slots allocated
    int targetInstances = std::max(1, instances);
    while (static_cast<int>(activeSlots_.size()) < targetInstances) {
        activeSlots_.push_back({nullptr, 0.0, 0.0});
    }
    while (static_cast<int>(activeSlots_.size()) > targetInstances) {
        // If instances shrank and a slot is empty, pop it
        if (!activeSlots_.back().req) {
            activeSlots_.pop_back();
        } else {
            break;
        }
    }

    // ③ Promote waiting requests from FIFO queue into available worker slots
    for (auto& slot : activeSlots_) {
        if (slot.req == nullptr && !waitQueue_.empty()) {
            auto req = waitQueue_.front();
            waitQueue_.pop();

            req->status = RequestStatus::Processing;

            // Compute exact queue waiting duration
            if (!req->route.empty()) {
                double waitMs = (simNow - req->route.back().arrivalTime) * 1000.0;
                if (waitMs < 0.0) waitMs = 0.0;
                req->totalQueueWaitMs += waitMs;
                sumQueueWaitMs_       += waitMs;
                req->route.back().processingStart  = simNow;
                req->route.back().queueWaitTimeMs  = waitMs;
            }

            // Component-specific start hook
            onStartProcessing(req, simNow);

            // Compute execution duration for this specific request
            double procDurationMs = computeProcessingDurationMs(req, simNow);
            double procDurationSec = procDurationMs / 1000.0;

            slot.req       = req;
            slot.startedAt = simNow;
            slot.finishAt  = simNow + procDurationSec;

            // If processing time is 0 (e.g. Client or immediate pass-through), finish in same tick
            if (procDurationMs <= 0.0) {
                req->status = RequestStatus::Forwarded;
                if (!req->route.empty()) {
                    auto& hop = req->route.back();
                    hop.departureTime    = simNow;
                    hop.processingTimeMs = 0.0;
                }
                ++txCount_;
                completed.push_back(req);
                slot.req = nullptr;
            }
        }
    }

    // ④ CPU Utilization: Percentage of active worker instances currently busy
    int busy = busySlots();
    int totalSlots = std::max(1, static_cast<int>(activeSlots_.size()));
    cpuUsage_ = (static_cast<double>(busy) / static_cast<double>(totalSlots)) * 100.0;
    if (cpuUsage_ > 100.0) cpuUsage_ = 100.0;

    // ⑤ Rolling throughput calculation (windowed over 1 simulated second)
    if (simNow - lastWindowTime_ >= 1.0) {
        throughputRps_  = static_cast<double>(txCount_ - txLastWindow_) / (simNow - lastWindowTime_);
        txLastWindow_   = txCount_;
        lastWindowTime_ = simNow;
    }

    // ⑥ Forward all finished requests downstream to next components
    for (auto& req : completed) {
        forwardRequest(req, simNow);
    }

    return completed;
}

// ── forwardRequest ────────────────────────────────────────────────────────────
// Dispatches a request that just finished processing at this component.
// If there are no outgoing edges, this component is the terminal sink and the
// request completes its overall journey.
void Component::forwardRequest(std::shared_ptr<Request> req, double simNow) {
    if (outgoing_.empty()) {
        // Terminal node reached — mark request completed
        req->status      = RequestStatus::Completed;
        req->completedAt = simNow;
        return;
    }
    // Default forwarder: send to first downstream connected component
    outgoing_[0]->receiveRequest(req, simNow);
}

int Component::busySlots() const {
    int count = 0;
    for (const auto& slot : activeSlots_) {
        if (slot.req != nullptr) ++count;
    }
    return count;
}

// ── getMetrics ────────────────────────────────────────────────────────────────
// Returns a live telemetry snapshot of this component's performance metrics.
ComponentMetrics Component::getMetrics(double /*simNow*/) const {
    ComponentMetrics m;
    m.componentId       = id_;
    m.componentName     = name_;
    m.componentType     = type_;
    m.queueDepth        = static_cast<int>(waitQueue_.size());
    m.maxQueue          = maxQueue;
    m.cpuUsagePct       = cpuUsage_;
    m.requestsReceived  = rxCount_;
    m.requestsCompleted = txCount_;
    m.requestsDropped   = droppedCount_;
    m.avgProcessingMs   = txCount_ > 0 ? (sumProcessingMs_ / txCount_) : 0.0;
    m.avgQueueWaitMs    = txCount_ > 0 ? (sumQueueWaitMs_ / txCount_) : 0.0;
    m.throughputPerSec  = throughputRps_;
    return m;
}

// ── resetStats ────────────────────────────────────────────────────────────────
void Component::resetStats() {
    rxCount_ = txCount_ = droppedCount_ = 0;
    sumProcessingMs_ = sumQueueWaitMs_ = 0.0;
    cpuUsage_ = 0.0;
    throughputRps_ = 0.0;
    txLastWindow_ = 0;
    lastWindowTime_ = 0.0;
    while (!waitQueue_.empty()) waitQueue_.pop();
    activeSlots_.clear();
}

} // namespace archisys
