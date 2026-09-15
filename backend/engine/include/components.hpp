#pragma once
#include "component.hpp"
#include <string>
#include <vector>
#include <climits>
#include <cstdlib>
#include <algorithm>

namespace archisys {

// ─────────────────────────────────────────────────────────────────────────────
// Client — Traffic Entry Point Node
// Injects requests into the architecture. Acts as an instant pass-through node.
// ─────────────────────────────────────────────────────────────────────────────
class Client : public Component {
public:
    Client(int id, std::string name)
        : Component(id, std::move(name), "client") {
        processingMs = 0.0;
        maxQueue     = 999999;
        instances    = 1;
        cpuCores     = 1;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Server — Multi-Instance Application Server
// Simulates compute workloads with configurable instances, CPU cores, and latency.
// ─────────────────────────────────────────────────────────────────────────────
class Server : public Component {
public:
    Server(int id, std::string name)
        : Component(id, std::move(name), "server") {
        processingMs = 50.0;
        maxQueue     = 200;
        instances    = 2;
        cpuCores     = 4;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Database — Data Persistence Tier
// Simulates read/write database query workloads with connection limits.
// Writes incur slightly higher latency than reads.
// ─────────────────────────────────────────────────────────────────────────────
class Database : public Component {
public:
    double writeMultiplier = 2.0; // Writes take 2x read latency

    Database(int id, std::string name)
        : Component(id, std::move(name), "database") {
        processingMs = 20.0; // Base query latency (ms)
        maxQueue     = 50;   // Connection pool limit
        instances    = 1;    // Databases typically serialize writes or have limited pool
        cpuCores     = 1;
    }

    double computeProcessingDurationMs(const std::shared_ptr<Request>& req, double simNow) const override {
        double base = Component::computeProcessingDurationMs(req, simNow);
        // Approximately 20% of requests are write transactions
        bool isWrite = (req && req->id % 5 == 0);
        return isWrite ? (base * writeMultiplier) : base;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// LoadBalancer — Traffic Distribution Node
// Balances incoming requests across multiple downstream servers using
// RoundRobin, LeastConnections, or Random strategies.
// ─────────────────────────────────────────────────────────────────────────────
enum class LBAlgorithm { RoundRobin, LeastConnections, Random };

class LoadBalancer : public Component {
public:
    LBAlgorithm algorithm = LBAlgorithm::LeastConnections;

    LoadBalancer(int id, std::string name)
        : Component(id, std::move(name), "loadbalancer") {
        processingMs = 2.0;  // Routing overhead latency
        maxQueue     = 500;
        instances    = 4;
        cpuCores     = 4;
    }

    void forwardRequest(std::shared_ptr<Request> req, double simNow) override {
        if (outgoing_.empty()) {
            req->status      = RequestStatus::Completed;
            req->completedAt = simNow;
            return;
        }

        Component* target = nullptr;
        switch (algorithm) {
            case LBAlgorithm::RoundRobin:
                target = outgoing_[rrIndex_++ % outgoing_.size()];
                break;
            case LBAlgorithm::Random:
                target = outgoing_[static_cast<size_t>(rand()) % outgoing_.size()];
                break;
            case LBAlgorithm::LeastConnections: {
                int minQueue = INT_MAX;
                for (auto* c : outgoing_) {
                    int totalLoad = c->queueDepth() + c->busySlots();
                    if (totalLoad < minQueue) {
                        minQueue = totalLoad;
                        target = c;
                    }
                }
                if (!target) target = outgoing_[0];
                break;
            }
        }

        if (target) {
            target->receiveRequest(req, simNow);
        } else {
            req->status      = RequestStatus::Completed;
            req->completedAt = simNow;
        }
    }

private:
    size_t rrIndex_ = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// RedisCache — In-Memory Caching Tier
// Serves hits in near-zero latency (~1ms).
// Cache misses incur slight lookup overhead and forward downstream to the Database.
// ─────────────────────────────────────────────────────────────────────────────
class RedisCache : public Component {
public:
    double hitRatio       = 0.80; // 80% cache hit probability
    double missMultiplier = 3.0;  // Cache miss lookup overhead

    uint64_t hits_   = 0;
    uint64_t misses_ = 0;

    RedisCache(int id, std::string name)
        : Component(id, std::move(name), "redis") {
        processingMs = 1.0;   // Cache hit lookup time (ms)
        maxQueue     = 1000;
        instances    = 4;
        cpuCores     = 2;
    }

    double computeProcessingDurationMs(const std::shared_ptr<Request>& req, double simNow) const override {
        double base = Component::computeProcessingDurationMs(req, simNow);
        // Deterministic pseudo-random hit check per request ID
        bool isHit = ((req->id * 2654435761u) % 100) < static_cast<unsigned>(hitRatio * 100.0);
        return isHit ? base : (base * missMultiplier);
    }

    void onStartProcessing(std::shared_ptr<Request>& req, double /*simNow*/) override {
        bool isHit = ((req->id * 2654435761u) % 100) < static_cast<unsigned>(hitRatio * 100.0);
        if (isHit) {
            ++hits_;
            if (!req->route.empty()) req->route.back().componentName = name_ + " [HIT]";
        } else {
            ++misses_;
            if (!req->route.empty()) req->route.back().componentName = name_ + " [MISS]";
        }
    }

    void forwardRequest(std::shared_ptr<Request> req, double simNow) override {
        bool isHit = (!req->route.empty() && req->route.back().componentName.find("[HIT]") != std::string::npos);

        if (isHit || outgoing_.empty()) {
            // Cache hit: Served directly from cache, no DB query needed
            req->status      = RequestStatus::Completed;
            req->completedAt = simNow;
        } else {
            // Cache miss: Forward to downstream Database
            outgoing_[0]->receiveRequest(req, simNow);
        }
    }

    double hitRate()  const { uint64_t t = hits_ + misses_; return t ? (static_cast<double>(hits_) / t) : 0.0; }
    double missRate() const { uint64_t t = hits_ + misses_; return t ? (static_cast<double>(misses_) / t) : 0.0; }
};

// ─────────────────────────────────────────────────────────────────────────────
// MessageQueue — Asynchronous Message Broker Tier (Kafka / RabbitMQ)
// Buffers asynchronous events and decouples fast producers from slow consumers.
// ─────────────────────────────────────────────────────────────────────────────
class MessageQueue : public Component {
public:
    double consumeRatePerSec = 200.0; // Drain rate

    MessageQueue(int id, std::string name)
        : Component(id, std::move(name), "queue") {
        processingMs = 5.0;   // Ingestion & acknowledgement latency
        maxQueue     = 10000; // High buffer capacity
        instances    = 2;
        cpuCores     = 2;
    }
};

} // namespace archisys
