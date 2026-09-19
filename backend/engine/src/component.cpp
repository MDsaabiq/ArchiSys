#include "component.hpp"
#include <algorithm>

namespace archisys {

Component::Component(int id, std::string name, std::string type)
    : id_(id), name_(std::move(name)), type_(std::move(type)) {}

void Component::addOutgoing(Component* next) {
    if (next) outgoing_.push_back(next);
}

void Component::addIncoming(Component* prev) {
    if (prev) incoming_.push_back(prev);
}

// ── Receive Request ──────────────────────────────────────────────────────────
// Accepts a request into the component's FIFO wait queue.
// Drops the request if maxQueue capacity is exceeded.
bool Component::receiveRequest(std::shared_ptr<Request> req, double simNowSec) {
    std::lock_guard<std::mutex> lock(compMutex_);

    if ((int)waitQueue_.size() >= maxQueue) {
        req->status = RequestStatus::Dropped;
        req->completedAtSec = simNowSec;
        droppedCount_++;
        return false;
    }

    req->status = RequestStatus::InQueue;
    req->currentComponentId = id_;

    RouteHop hop;
    hop.componentId = id_;
    hop.componentName = name_;
    hop.queueWaitTimeMs = 0.0;
    hop.processingTimeMs = 0.0;
    req->route.push_back(hop);

    waitQueue_.push({req, simNowSec});
    rxCount_++;
    return true;
}

// ── Calculate Processing Duration ────────────────────────────────────────────
// Calculates execution latency in milliseconds for a request.
double Component::calculateProcessTime(const std::shared_ptr<Request>& /*req*/) {
    double timeMs = procTimeMs;
    if (timeMs <= 0.0) return 0.0;

    // CPU core contention scaling:
    // When busy workers exceed available physical CPU cores, latency increases proportionally
    int busy = getBusyWorkers();
    if (cpuCores > 0 && busy > cpuCores) {
        double contentionFactor = (double)busy / cpuCores;
        if (contentionFactor > 2.5) contentionFactor = 2.5; // Cap at 2.5x
        timeMs *= contentionFactor;
    }
    return timeMs;
}

// ── Update / Discrete Tick ───────────────────────────────────────────────────
// Updates worker instances, processes FIFO queues, and forwards finished requests.
std::vector<std::shared_ptr<Request>> Component::update(double dtSec, double simNowSec) {
    std::lock_guard<std::mutex> lock(compMutex_);
    std::vector<std::shared_ptr<Request>> finishedRequests;
    double dtMs = dtSec * 1000.0;

    // 1. Maintain worker pool size to match configured instance count
    int targetWorkers = instances > 0 ? instances : 1;
    while ((int)workers_.size() < targetWorkers) {
        workers_.push_back({nullptr, 0.0, 0.0});
    }
    while ((int)workers_.size() > targetWorkers) {
        if (workers_.back().activeRequest == nullptr) {
            workers_.pop_back();
        } else {
            break;
        }
    }

    // 2. Advance processing for active workers
    for (size_t i = 0; i < workers_.size(); i++) {
        if (workers_[i].activeRequest != nullptr) {
            workers_[i].remainingTimeMs -= dtMs;
            if (workers_[i].remainingTimeMs <= 0.0) {
                // Request finished processing at this worker
                auto req = workers_[i].activeRequest;
                double actualProcMs = workers_[i].totalTimeMs;

                sumProcessingMs_ += actualProcMs;
                req->totalProcessingMs += actualProcMs;

                if (!req->route.empty()) {
                    req->route.back().processingTimeMs = actualProcMs;
                }

                txCount_++;
                finishedRequests.push_back(req);

                // Free the worker instance
                workers_[i].activeRequest = nullptr;
                workers_[i].remainingTimeMs = 0.0;
                workers_[i].totalTimeMs = 0.0;
            }
        }
    }

    // 3. Promote waiting requests from FIFO queue to available workers
    for (size_t i = 0; i < workers_.size(); i++) {
        if (workers_[i].activeRequest == nullptr && !waitQueue_.empty()) {
            auto item = waitQueue_.front();
            waitQueue_.pop();

            auto req = item.first;
            double arrivalTimeSec = item.second;
            double waitMs = (simNowSec - arrivalTimeSec) * 1000.0;
            if (waitMs < 0.0) waitMs = 0.0;

            sumQueueWaitMs_ += waitMs;
            req->totalQueueWaitMs += waitMs;

            if (!req->route.empty()) {
                req->route.back().queueWaitTimeMs = waitMs;
            }

            double processMs = calculateProcessTime(req);

            if (processMs <= 0.0) {
                // Instant pass-through (e.g. 0ms Client node)
                txCount_++;
                finishedRequests.push_back(req);
            } else {
                workers_[i].activeRequest = req;
                workers_[i].remainingTimeMs = processMs;
                workers_[i].totalTimeMs = processMs;
                req->status = RequestStatus::Processing;
            }
        }
    }

    // 4. Calculate CPU Utilization (% of busy worker instances)
    int busy = 0;
    for (const auto& w : workers_) {
        if (w.activeRequest != nullptr) busy++;
    }
    int total = (int)workers_.size();
    cpuUsagePct_ = total > 0 ? ((double)busy / total) * 100.0 : 0.0;
    if (cpuUsagePct_ > 100.0) cpuUsagePct_ = 100.0;

    // 5. Rolling throughput calculation (windowed every 1 second)
    if (simNowSec - lastMetricTimeSec_ >= 1.0) {
        double elapsedSec = simNowSec - lastMetricTimeSec_;
        throughput_ = (double)(txCount_ - lastTxCount_) / elapsedSec;
        lastTxCount_ = txCount_;
        lastMetricTimeSec_ = simNowSec;
    }

    // 6. Forward completed requests downstream
    for (auto& req : finishedRequests) {
        forwardRequest(req, simNowSec);
    }

    return finishedRequests;
}

// ── Forward Request ──────────────────────────────────────────────────────────
// Dispatches finished requests to downstream components or marks complete if terminal.
void Component::forwardRequest(std::shared_ptr<Request> req, double simNowSec) {
    if (outgoing_.empty()) {
        req->status = RequestStatus::Completed;
        req->completedAtSec = simNowSec;
        return;
    }
    // Default: Forward to the first outgoing connection
    outgoing_[0]->receiveRequest(req, simNowSec);
}

int Component::getQueueDepth() const {
    std::lock_guard<std::mutex> lock(compMutex_);
    return (int)waitQueue_.size();
}

int Component::getBusyWorkers() const {
    int count = 0;
    for (const auto& w : workers_) {
        if (w.activeRequest != nullptr) count++;
    }
    return count;
}

double Component::getCpuUsagePct() const {
    return cpuUsagePct_;
}

// ── Get Metrics ──────────────────────────────────────────────────────────────
ComponentMetrics Component::getMetrics() const {
    std::lock_guard<std::mutex> lock(compMutex_);
    ComponentMetrics m;
    m.id = id_;
    m.name = name_;
    m.type = type_;
    m.queueDepth = (int)waitQueue_.size();
    m.maxQueue = maxQueue;
    m.cpuUsagePct = cpuUsagePct_;
    m.requestsReceived = rxCount_;
    m.requestsCompleted = txCount_;
    m.requestsDropped = droppedCount_;
    m.avgProcessingMs = txCount_ > 0 ? (sumProcessingMs_ / txCount_) : 0.0;
    m.avgQueueWaitMs = txCount_ > 0 ? (sumQueueWaitMs_ / txCount_) : 0.0;
    m.throughputPerSec = throughput_;
    return m;
}

// ── Reset ────────────────────────────────────────────────────────────────────
void Component::reset() {
    std::lock_guard<std::mutex> lock(compMutex_);
    rxCount_ = 0;
    txCount_ = 0;
    droppedCount_ = 0;
    sumProcessingMs_ = 0.0;
    sumQueueWaitMs_ = 0.0;
    cpuUsagePct_ = 0.0;
    throughput_ = 0.0;
    lastTxCount_ = 0;
    lastMetricTimeSec_ = 0.0;
    while (!waitQueue_.empty()) waitQueue_.pop();
    workers_.clear();
}

} // namespace archisys
