#include "component.hpp"
#include <algorithm>
#include <stdexcept>

namespace archisys {

Component::Component(int id, std::string name, std::string type)
    : id_(id), name_(std::move(name)), type_(std::move(type)) {}

// ── receiveRequest ────────────────────────────────────────────────────────────
bool Component::receiveRequest(std::shared_ptr<Request> req, double simNow) {
    if (static_cast<int>(waitQueue_.size()) >= maxQueue) {
        // Queue full — drop the request
        req->status = RequestStatus::Dropped;
        ++droppedCount_;
        return false;
    }
    req->status            = RequestStatus::InQueue;
    req->currentComponentId = id_;

    // Record arrival hop
    RouteHop hop;
    hop.componentId   = id_;
    hop.componentName = name_;
    hop.arrivalTime   = simNow;
    hop.departureTime = 0;
    hop.processingTime= 0;
    req->route.push_back(hop);

    waitQueue_.push(req);
    ++rxCount_;
    return true;
}

// ── tick ─────────────────────────────────────────────────────────────────────
// Each tick:
//  1. Finish any active slots whose finishAt <= simNow
//  2. Fill free slots from the wait queue
//  3. Return completed requests
std::vector<std::shared_ptr<Request>> Component::tick(double simNow, double dtSec) {
    std::vector<std::shared_ptr<Request>> completed;

    // ① Finish active slots
    for (auto& slot : activeSlots_) {
        if (slot.req && simNow >= slot.finishAt) {
            auto& req = slot.req;
            req->status = RequestStatus::Forwarded;

            double procMs = processingMs;
            sumProcessingMs_ += procMs;

            // Update the route hop departure
            if (!req->route.empty()) {
                auto& hop       = req->route.back();
                hop.departureTime  = simNow;
                hop.processingTime = procMs;
            }

            req->totalLatencyMs += procMs;
            ++txCount_;

            completed.push_back(req);
            slot.req = nullptr;  // free the slot
        }
    }

    // ② Promote from wait queue into free slots (capacity = instances)
    int freeSlots = 0;
    for (auto& slot : activeSlots_) {
        if (!slot.req) ++freeSlots;
    }
    // Ensure activeSlots_ has `instances` entries
    while (static_cast<int>(activeSlots_.size()) < instances) {
        activeSlots_.push_back({nullptr, 0.0});
        ++freeSlots;
    }

    for (auto& slot : activeSlots_) {
        if (!slot.req && !waitQueue_.empty()) {
            auto req        = waitQueue_.front(); waitQueue_.pop();
            req->status     = RequestStatus::Processing;

            // Queue wait time
            if (!req->route.empty()) {
                double waitMs = (simNow - req->route.back().arrivalTime) * 1000.0;
                req->totalQueueWaitMs += waitMs;
                sumQueueWaitMs_       += waitMs;
            }

            processRequest(req, simNow);

            // Schedule finish
            slot.req      = req;
            slot.finishAt = simNow + (processingMs / 1000.0);
        }
    }

    // ③ CPU usage: fraction of slots occupied
    int busySlots = 0;
    for (auto& slot : activeSlots_) if (slot.req) ++busySlots;
    cpuUsage_ = instances > 0 ? (double(busySlots) / instances) * 100.0 : 0.0;

    // ④ Throughput (rolling 1-second window)
    if (simNow - lastWindowTime_ >= 1.0) {
        throughputRps_  = double(txCount_ - txLastWindow_) / (simNow - lastWindowTime_);
        txLastWindow_   = txCount_;
        lastWindowTime_ = simNow;
    }

    // ⑤ Forward completed requests downstream
    for (auto& req : completed) {
        forwardRequest(req, simNow);
    }

    return completed;
}

// ── forwardRequest ────────────────────────────────────────────────────────────
void Component::forwardRequest(std::shared_ptr<Request> req, double simNow) {
    if (outgoing_.empty()) {
        // Terminal node — mark complete
        req->status      = RequestStatus::Completed;
        req->completedAt = simNow;
        return;
    }
    // Default: send to first outgoing component
    outgoing_[0]->receiveRequest(req, simNow);
}

// ── queueDepth ────────────────────────────────────────────────────────────────
int Component::queueDepth() const {
    return static_cast<int>(waitQueue_.size());
}

// ── getMetrics ────────────────────────────────────────────────────────────────
ComponentMetrics Component::getMetrics(double simNow) const {
    ComponentMetrics m;
    m.componentId      = id_;
    m.componentName    = name_;
    m.componentType    = type_;
    m.queueDepth       = static_cast<int>(waitQueue_.size());
    m.maxQueue         = maxQueue;
    m.cpuUsagePct      = cpuUsage_;
    m.requestsReceived = rxCount_;
    m.requestsCompleted= txCount_;
    m.requestsDropped  = droppedCount_;
    m.avgProcessingMs  = txCount_ > 0 ? sumProcessingMs_ / txCount_ : 0.0;
    m.avgQueueWaitMs   = txCount_ > 0 ? sumQueueWaitMs_  / txCount_ : 0.0;
    m.throughputPerSec = throughputRps_;
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
