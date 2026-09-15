#pragma once
#include "request.hpp"
#include "metrics.hpp"
#include <string>
#include <vector>
#include <queue>
#include <memory>
#include <algorithm>

namespace archisys {

/**
 * ActiveSlot represents a single concurrent worker thread / instance in a component.
 */
struct ActiveSlot {
    std::shared_ptr<Request> req       = nullptr;
    double                   startedAt = 0.0; // Simulation time (sec) when processing began
    double                   finishAt  = 0.0; // Simulation time (sec) when processing finishes
};

/**
 * Base Component class for every simulated node in ArchiSys.
 * Manages incoming FIFO queues, worker instance concurrency slots,
 * latency profiling, and downstream request forwarding.
 */
class Component {
public:
    Component(int id, std::string name, std::string type);
    virtual ~Component() = default;

    // ── Topology & Identity ──────────────────────────────────────────────────
    int                id()   const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& type() const { return type_; }

    void addOutgoing(Component* c) { outgoing_.push_back(c); }
    void addIncoming(Component* c) { incoming_.push_back(c); }
    const std::vector<Component*>& outgoing() const { return outgoing_; }
    const std::vector<Component*>& incoming() const { return incoming_; }

    // ── Configuration Parameters (User controllable) ─────────────────────────
    int    maxQueue     = 100;   // Max queue capacity before drops occur
    double processingMs = 50.0;  // Base processing execution time (ms) per request
    int    instances    = 1;      // Number of concurrent processing worker slots
    int    cpuCores     = 1;      // Allocated CPU cores (affects CPU contention under high concurrency)

    // ── Simulation Lifecycle ─────────────────────────────────────────────────

    /**
     * Executes one discrete simulation tick.
     * 1. Completes any worker slots whose finishAt <= simNow
     * 2. Schedules waiting requests from FIFO queue into free worker slots
     * 3. Calculates instant CPU and throughput metrics
     * 4. Forwards completed requests downstream
     */
    virtual std::vector<std::shared_ptr<Request>> tick(double simNow, double dtSec);

    /**
     * Accepts a request into the component's FIFO waiting queue.
     * Returns false if the queue buffer is full (request is dropped).
     */
    virtual bool receiveRequest(std::shared_ptr<Request> req, double simNow);

    /**
     * Computes the processing duration in milliseconds for this specific request.
     * Can be overridden by subclasses (e.g. Database queries vs writes, Redis hit/miss).
     */
    virtual double computeProcessingDurationMs(const std::shared_ptr<Request>& req, double simNow) const;

    /**
     * Hook called right when a request transitions from queue to active processing.
     */
    virtual void onStartProcessing(std::shared_ptr<Request>& /*req*/, double /*simNow*/) {}

    /**
     * Routes a completed request to downstream components or marks it complete.
     */
    virtual void forwardRequest(std::shared_ptr<Request> req, double simNow);

    // ── Metrics & State ──────────────────────────────────────────────────────
    ComponentMetrics getMetrics(double simNow) const;
    void resetStats();

    int    queueDepth()    const { return static_cast<int>(waitQueue_.size()); }
    int    busySlots()     const;
    double cpuUsagePct()   const { return cpuUsage_; }

protected:
    int         id_;
    std::string name_;
    std::string type_;

    std::vector<Component*> outgoing_;
    std::vector<Component*> incoming_;

    // FIFO request waiting queue
    std::queue<std::shared_ptr<Request>> waitQueue_;

    // Concurrency processing worker slots
    std::vector<ActiveSlot> activeSlots_;

    // Telemetry and statistics counters
    uint64_t rxCount_         = 0;
    uint64_t txCount_         = 0;
    uint64_t droppedCount_    = 0;
    double   sumProcessingMs_ = 0.0;
    double   sumQueueWaitMs_  = 0.0;

    // Rolling CPU & throughput calculations
    double   cpuUsage_       = 0.0;
    uint64_t txLastWindow_   = 0;
    double   lastWindowTime_ = 0.0;
    double   throughputRps_  = 0.0;
};

} // namespace archisys
