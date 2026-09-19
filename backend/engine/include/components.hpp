#pragma once
#include "component.hpp"
#include <string>
#include <vector>
#include <cstdlib>
#include <climits>
#include <algorithm>

namespace archisys {

// ─────────────────────────────────────────────────────────────────────────────
// Client Node — Injects synthetic traffic into the architecture
// ─────────────────────────────────────────────────────────────────────────────
class Client : public Component {
public:
    Client(int id, std::string name)
        : Component(id, std::move(name), "client") {
        procTimeMs = 0.0;     // Instant pass-through
        maxQueue   = 999999;
        instances  = 1;
        cpuCores   = 1;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Server Node — Multi-instance application worker pool
// ─────────────────────────────────────────────────────────────────────────────
class Server : public Component {
public:
    Server(int id, std::string name)
        : Component(id, std::move(name), "server") {
        procTimeMs = 20.0;
        maxQueue   = 100;
        instances  = 2;
        cpuCores   = 2;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Database Node — Data storage and persistence tier
// Simulates read/write transaction latencies and connection pool limits
// ─────────────────────────────────────────────────────────────────────────────
class Database : public Component {
public:
    double writeMultiplier = 2.0; // Writes take 2x read latency

    Database(int id, std::string name)
        : Component(id, std::move(name), "database") {
        procTimeMs = 20.0; // Base query latency (ms)
        maxQueue   = 50;   // Connection pool limit
        instances  = 1;
        cpuCores   = 1;
    }

    double calculateProcessTime(const std::shared_ptr<Request>& req) override {
        double base = Component::calculateProcessTime(req);
        // ~20% of operations are write transactions
        bool isWrite = (req && (req->id % 5 == 0));
        return isWrite ? (base * writeMultiplier) : base;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Load Balancer Node — Distributes incoming traffic across downstream servers
// ─────────────────────────────────────────────────────────────────────────────
enum class LBAlgorithm { RoundRobin, LeastConnections, Random };

class LoadBalancer : public Component {
public:
    LBAlgorithm algorithm = LBAlgorithm::LeastConnections;

    LoadBalancer(int id, std::string name)
        : Component(id, std::move(name), "loadbalancer") {
        procTimeMs = 1.0;
        maxQueue   = 500;
        instances  = 4;
        cpuCores   = 4;
    }

    void forwardRequest(std::shared_ptr<Request> req, double simNowSec) override {
        if (outgoing_.empty()) {
            req->status = RequestStatus::Completed;
            req->completedAtSec = simNowSec;
            return;
        }

        Component* target = nullptr;

        if (algorithm == LBAlgorithm::RoundRobin) {
            target = outgoing_[rrIndex_ % outgoing_.size()];
            rrIndex_++;
        } else if (algorithm == LBAlgorithm::Random) {
            target = outgoing_[rand() % outgoing_.size()];
        } else {
            // Default: Least Connections (select target with minimal queue + active workers)
            int minLoad = INT_MAX;
            for (Component* c : outgoing_) {
                int load = c->getQueueDepth() + c->getBusyWorkers();
                if (load < minLoad) {
                    minLoad = load;
                    target = c;
                }
            }
            if (!target) target = outgoing_[0];
        }

        if (target) {
            target->receiveRequest(req, simNowSec);
        } else {
            req->status = RequestStatus::Completed;
            req->completedAtSec = simNowSec;
        }
    }

private:
    size_t rrIndex_ = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Redis Cache Node — In-memory caching tier (80/20 rule)
// Cache hits complete in 1ms; cache misses forward to Database
// ─────────────────────────────────────────────────────────────────────────────
class RedisCache : public Component {
public:
    double hitRatio = 0.80; // 80% cache hit rate

    RedisCache(int id, std::string name)
        : Component(id, std::move(name), "redis") {
        procTimeMs = 1.0;   // 1ms cache lookup
        maxQueue   = 1000;
        instances  = 4;
        cpuCores   = 2;
    }

    double calculateProcessTime(const std::shared_ptr<Request>& req) override {
        bool isHit = isCacheHit(req);
        // Hit takes 1ms, Miss lookup takes 3ms
        return isHit ? 1.0 : 3.0;
    }

    void forwardRequest(std::shared_ptr<Request> req, double simNowSec) override {
        bool isHit = isCacheHit(req);

        if (isHit || outgoing_.empty()) {
            // Cache Hit: Served directly from cache, completed!
            req->status = RequestStatus::Completed;
            req->completedAtSec = simNowSec;
        } else {
            // Cache Miss: Forward downstream to Database
            outgoing_[0]->receiveRequest(req, simNowSec);
        }
    }

private:
    bool isCacheHit(const std::shared_ptr<Request>& req) const {
        if (!req) return true;
        // Deterministic hit ratio test based on request ID
        return ((req->id * 73) % 100) < static_cast<unsigned>(hitRatio * 100.0);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Message Queue Node — Asynchronous buffer decoupling upstream from workers
// ─────────────────────────────────────────────────────────────────────────────
class MessageQueue : public Component {
public:
    MessageQueue(int id, std::string name)
        : Component(id, std::move(name), "queue") {
        procTimeMs = 5.0;
        maxQueue   = 10000; // High buffer capacity
        instances  = 2;
        cpuCores   = 2;
    }
};

} // namespace archisys
