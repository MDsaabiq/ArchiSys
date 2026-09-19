#pragma once
#include "request.hpp"
#include "metrics.hpp"
#include <string>
#include <vector>
#include <queue>
#include <memory>
#include <mutex>

namespace archisys {

/**
 * Worker instance inside a component that processes a single request at a time.
 */
struct Worker {
    std::shared_ptr<Request> activeRequest = nullptr;
    double remainingTimeMs = 0.0;
    double totalTimeMs = 0.0;
};

/**
 * Base Component class for all nodes in the architecture graph.
 * Uses standard OOP inheritance and polymorphism.
 */
class Component {
public:
    Component(int id, std::string name, std::string type);
    virtual ~Component() = default;

    // Topology getters & setters
    int getId() const { return id_; }
    const std::string& getName() const { return name_; }
    const std::string& getType() const { return type_; }

    void addOutgoing(Component* next);
    void addIncoming(Component* prev);
    const std::vector<Component*>& getOutgoing() const { return outgoing_; }
    const std::vector<Component*>& getIncoming() const { return incoming_; }

    // User-configurable parameters
    int maxQueue = 100;
    double procTimeMs = 20.0;
    int instances = 1;
    int cpuCores = 1;

    // Simulation lifecycle methods (Polymorphic)
    virtual bool receiveRequest(std::shared_ptr<Request> req, double simNowSec);
    virtual std::vector<std::shared_ptr<Request>> update(double dtSec, double simNowSec);
    virtual double calculateProcessTime(const std::shared_ptr<Request>& req);
    virtual void forwardRequest(std::shared_ptr<Request> req, double simNowSec);

    // Telemetry & metrics
    ComponentMetrics getMetrics() const;
    void reset();

    int getQueueDepth() const;
    int getBusyWorkers() const;
    double getCpuUsagePct() const;

protected:
    int id_;
    std::string name_;
    std::string type_;

    std::vector<Component*> outgoing_;
    std::vector<Component*> incoming_;

    // Standard FIFO queue of waiting requests with their arrival timestamps (in seconds)
    std::queue<std::pair<std::shared_ptr<Request>, double>> waitQueue_;

    // Concurrent worker pool
    std::vector<Worker> workers_;

    // Performance counters
    uint64_t rxCount_ = 0;
    uint64_t txCount_ = 0;
    uint64_t droppedCount_ = 0;
    double sumProcessingMs_ = 0.0;
    double sumQueueWaitMs_ = 0.0;
    double cpuUsagePct_ = 0.0;
    double throughput_ = 0.0;

    uint64_t lastTxCount_ = 0;
    double lastMetricTimeSec_ = 0.0;

    mutable std::mutex compMutex_;
};

} // namespace archisys
