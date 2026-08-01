#pragma once
#include "request.hpp"
#include "metrics.hpp"
#include <string>
#include <vector>
#include <queue>
#include <functional>
#include <memory>

namespace archisys {

// ─────────────────────────────────────────────────────────────────────────────
// Base class for every simulated system component.
// Derived classes override processRequest() and getType().
// ─────────────────────────────────────────────────────────────────────────────
class Component {
public:
    explicit Component(int id, std::string name, std::string type);
    virtual ~Component() = default;

    // ── Identity ──────────────────────────────────────────────────────────
    int                id()   const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& type() const { return type_; }

    // ── Topology ──────────────────────────────────────────────────────────
    void addOutgoing(Component* c)  { outgoing_.push_back(c); }
    void addIncoming(Component* c)  { incoming_.push_back(c); }
    const std::vector<Component*>& outgoing() const { return outgoing_; }
    const std::vector<Component*>& incoming() const { return incoming_; }

    // ── Configuration (set before simulation starts) ───────────────────────
    int    maxQueue    = 100;    // max requests that can sit in queue
    double processingMs= 50.0;  // base processing time per request (ms)
    int    instances   = 1;     // parallel processing lanes

    // ── Simulation interface ───────────────────────────────────────────────

    // Called each tick. Returns list of requests that finished processing
    // and are ready to be forwarded to the next component.
    virtual std::vector<std::shared_ptr<Request>> tick(double simNow, double dtSec);

    // Receive a new request into this component's queue.
    // Returns false if the queue is full (request is dropped).
    virtual bool receiveRequest(std::shared_ptr<Request> req, double simNow);

    // Override to apply component-specific processing logic.
    // simNow is in simulation seconds.
    virtual void processRequest(std::shared_ptr<Request> req, double simNow) {}

    // Forward completed request to the appropriate next component(s).
    virtual void forwardRequest(std::shared_ptr<Request> req, double simNow);

    // ── Metrics ───────────────────────────────────────────────────────────
    ComponentMetrics getMetrics(double simNow) const;
    void resetStats();

    // Current live state
    int    queueDepth()    const;
    double cpuUsagePct()   const { return cpuUsage_; }

protected:
    // ── Internal state ────────────────────────────────────────────────────
    int         id_;
    std::string name_;
    std::string type_;

    std::vector<Component*> outgoing_;
    std::vector<Component*> incoming_;

    // Waiting queue (FIFO)
    std::queue<std::shared_ptr<Request>> waitQueue_;

    // Active (being processed) slots: {request, finishTime}
    struct ActiveSlot {
        std::shared_ptr<Request> req;
        double finishAt; // sim-seconds
    };
    std::vector<ActiveSlot> activeSlots_;

    // Stats
    uint64_t rxCount_        = 0;
    uint64_t txCount_        = 0;
    uint64_t droppedCount_   = 0;
    double   sumProcessingMs_= 0.0;
    double   sumQueueWaitMs_ = 0.0;

    // Rolling CPU usage (updated each tick)
    double cpuUsage_ = 0.0;

    // For throughput calculation
    uint64_t txLastWindow_  = 0;
    double   lastWindowTime_= 0.0;
    double   throughputRps_ = 0.0;
};

} // namespace archisys
