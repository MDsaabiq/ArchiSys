#pragma once
#include "../component.hpp"
#include <string>
#include <climits>

namespace archisys {

// ─────────────────────────────────────────────────────────────────────────────
// Client — entry point / traffic generator node (pass-through, no queuing)
// Request rate and total request count are configured in SimulationConfig.
// ─────────────────────────────────────────────────────────────────────────────
class Client : public Component {
public:
    Client(int id, std::string name)
        : Component(id, std::move(name), "client") {
        processingMs = 0.0;   // no processing delay
        maxQueue     = 999999;
        instances    = 1;
    }
};


// ─────────────────────────────────────────────────────────────────────────────
// Server — multi-instance application server
// Tracks CPU usage per instance, processes requests in parallel
// ─────────────────────────────────────────────────────────────────────────────
class Server : public Component {
public:
    Server(int id, std::string name)
        : Component(id, std::move(name), "server") {
        processingMs = 50.0;
        maxQueue     = 200;
        instances    = 2;
    }

    void processRequest(std::shared_ptr<Request> req, double simNow) override {
        // No extra logic needed — base tick() handles slot scheduling.
        // Override here to add future features (error injection, etc.)
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Database — single-threaded query engine with configurable latency
// ─────────────────────────────────────────────────────────────────────────────
class Database : public Component {
public:
    double queryLatencyMs = 20.0;   // base query latency
    double writeLatencyMs = 40.0;
    int    connectionLimit= 50;

    Database(int id, std::string name)
        : Component(id, std::move(name), "database") {
        processingMs = queryLatencyMs;
        maxQueue     = connectionLimit;
        instances    = 1;  // databases serialize writes
    }

    void processRequest(std::shared_ptr<Request> req, double simNow) override {
        // Simulate heavier write latency occasionally
        processingMs = (req->id % 5 == 0) ? writeLatencyMs : queryLatencyMs;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// LoadBalancer — distributes requests across multiple downstream servers
// Supports: RoundRobin, Random, LeastConnections
// ─────────────────────────────────────────────────────────────────────────────
enum class LBAlgorithm { RoundRobin, Random, LeastConnections };

class LoadBalancer : public Component {
public:
    LBAlgorithm algorithm = LBAlgorithm::RoundRobin;
    int         maxConns  = 10000;

    LoadBalancer(int id, std::string name)
        : Component(id, std::move(name), "loadbalancer") {
        processingMs = 2.0;   // nearly instant forwarding
        maxQueue     = 500;
        instances    = 4;
    }

    // Override forwardRequest to implement balancing logic
    void forwardRequest(std::shared_ptr<Request> req, double simNow) override {
        if (outgoing_.empty()) {
            req->status = RequestStatus::Completed;
            req->completedAt = simNow;
            return;
        }
        Component* target = nullptr;
        switch (algorithm) {
            case LBAlgorithm::RoundRobin:
                target = outgoing_[rrIndex_++ % outgoing_.size()];
                break;
            case LBAlgorithm::Random:
                target = outgoing_[rand() % outgoing_.size()];
                break;
            case LBAlgorithm::LeastConnections: {
                int minQ = INT_MAX;
                for (auto* c : outgoing_) {
                    int q = c->queueDepth();
                    if (q < minQ) { minQ = q; target = c; }
                }
                break;
            }
        }
        if (target) target->receiveRequest(req, simNow);
    }

private:
    size_t rrIndex_ = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// RedisCache — probabilistic cache hit/miss, forwards misses to next component
// ─────────────────────────────────────────────────────────────────────────────
class RedisCache : public Component {
public:
    double hitRatio   = 0.80;  // 80% of requests served from cache
    double cacheHitMs = 1.0;   // near-instant on hit
    double cacheMissMs= 5.0;   // slight overhead on miss before forwarding

    // Stats
    uint64_t hits_   = 0;
    uint64_t misses_ = 0;

    RedisCache(int id, std::string name)
        : Component(id, std::move(name), "redis") {
        processingMs = cacheHitMs;
        maxQueue     = 1000;
        instances    = 4;
    }

    void processRequest(std::shared_ptr<Request> req, double simNow) override {
        bool hit = ((double)rand() / RAND_MAX) < hitRatio;
        req->totalLatencyMs += hit ? cacheHitMs : cacheMissMs;
        if (hit) {
            ++hits_;
        } else {
            ++misses_;
        }
        // Tag the request so forwardRequest knows what to do
        req->route.back().componentName = hit
            ? (name_ + "[HIT]")
            : (name_ + "[MISS]");
    }

    void forwardRequest(std::shared_ptr<Request> req, double simNow) override {
        const auto& tag = req->route.back().componentName;
        bool hit = (tag.find("[HIT]") != std::string::npos);
        if (hit) {
            // Cache hit: complete immediately, no downstream needed
            req->status      = RequestStatus::Completed;
            req->completedAt = simNow;
        } else {
            // Cache miss: forward to next (usually a database)
            if (!outgoing_.empty()) {
                outgoing_[0]->receiveRequest(req, simNow);
            } else {
                req->status      = RequestStatus::Completed;
                req->completedAt = simNow;
            }
        }
    }

    double hitRate()  const { uint64_t t = hits_+misses_; return t ? double(hits_)/t   : 0.0; }
    double missRate() const { uint64_t t = hits_+misses_; return t ? double(misses_)/t : 0.0; }
};

// ─────────────────────────────────────────────────────────────────────────────
// MessageQueue — async broker (Kafka / RabbitMQ model)
// Requests are buffered; consumers pull at a configurable rate
// ─────────────────────────────────────────────────────────────────────────────
class MessageQueue : public Component {
public:
    double consumeRatePerSec = 200.0; // messages consumed per sim-second

    MessageQueue(int id, std::string name)
        : Component(id, std::move(name), "queue") {
        processingMs = 5.0;   // enqueue latency
        maxQueue     = 10000;
        instances    = 1;
    }
};

} // namespace archisys
