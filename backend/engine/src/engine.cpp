#include "engine.hpp"
#include <iostream>

namespace archisys {

// ─────────────────────────────────────────────────────────────────────────────
// Component Base Class Implementation
// ─────────────────────────────────────────────────────────────────────────────

Component::Component(int id, std::string name, std::string type)
    : id_(id), name_(std::move(name)), type_(std::move(type)) {}

void Component::addOutgoing(Component* next) {
    if (next) outgoing_.push_back(next);
}

void Component::addIncoming(Component* prev) {
    if (prev) incoming_.push_back(prev);
}

bool Component::receiveRequest(std::shared_ptr<Request> req, double simNowSec) {
    std::lock_guard<std::mutex> lock(compMutex_);

    // Queue overflow check
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

double Component::calculateProcessTime(const std::shared_ptr<Request>& /*req*/) {
    double timeMs = procTimeMs;
    if (timeMs <= 0.0) return 0.0;

    // CPU contention scaling if active workers exceed physical CPU cores
    int busy = getBusyWorkers();
    if (cpuCores > 0 && busy > cpuCores) {
        double contentionFactor = (double)busy / cpuCores;
        if (contentionFactor > 2.5) contentionFactor = 2.5; // Cap at 2.5x
        timeMs *= contentionFactor;
    }
    return timeMs;
}

std::vector<std::shared_ptr<Request>> Component::update(double dtSec, double simNowSec) {
    std::lock_guard<std::mutex> lock(compMutex_);
    std::vector<std::shared_ptr<Request>> finishedRequests;
    double dtMs = dtSec * 1000.0;

    // 1. Maintain worker pool matching instance count
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

    // 2. Advance active worker execution
    for (size_t i = 0; i < workers_.size(); i++) {
        if (workers_[i].activeRequest != nullptr) {
            workers_[i].remainingTimeMs -= dtMs;
            if (workers_[i].remainingTimeMs <= 0.0) {
                auto req = workers_[i].activeRequest;
                double actualProcMs = workers_[i].totalTimeMs;

                sumProcessingMs_ += actualProcMs;
                req->totalProcessingMs += actualProcMs;

                if (!req->route.empty()) {
                    req->route.back().processingTimeMs = actualProcMs;
                }

                txCount_++;
                finishedRequests.push_back(req);

                workers_[i].activeRequest = nullptr;
                workers_[i].remainingTimeMs = 0.0;
                workers_[i].totalTimeMs = 0.0;
            }
        }
    }

    // 3. Dispatch waiting requests to idle workers
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

    // 4. Calculate CPU Utilization (% busy workers)
    int busy = 0;
    for (const auto& w : workers_) {
        if (w.activeRequest != nullptr) busy++;
    }
    int total = (int)workers_.size();
    cpuUsagePct_ = total > 0 ? ((double)busy / total) * 100.0 : 0.0;
    if (cpuUsagePct_ > 100.0) cpuUsagePct_ = 100.0;

    // 5. Calculate rolling throughput
    if (simNowSec - lastMetricTimeSec_ >= 1.0) {
        double elapsedSec = simNowSec - lastMetricTimeSec_;
        throughput_ = (double)(txCount_ - lastTxCount_) / elapsedSec;
        lastTxCount_ = txCount_;
        lastMetricTimeSec_ = simNowSec;
    }

    // 6. Forward finished requests downstream
    for (auto& req : finishedRequests) {
        forwardRequest(req, simNowSec);
    }

    return finishedRequests;
}

void Component::forwardRequest(std::shared_ptr<Request> req, double simNowSec) {
    if (outgoing_.empty()) {
        req->status = RequestStatus::Completed;
        req->completedAtSec = simNowSec;
        return;
    }
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

// ─────────────────────────────────────────────────────────────────────────────
// Specialized Component Implementations
// ─────────────────────────────────────────────────────────────────────────────

Client::Client(int id, std::string name)
    : Component(id, std::move(name), "client") {
    procTimeMs = 0.0;
    maxQueue   = 999999;
    instances  = 1;
    cpuCores   = 1;
}

Server::Server(int id, std::string name)
    : Component(id, std::move(name), "server") {
    procTimeMs = 20.0;
    maxQueue   = 100;
    instances  = 2;
    cpuCores   = 2;
}

Database::Database(int id, std::string name)
    : Component(id, std::move(name), "database") {
    procTimeMs = 20.0;
    maxQueue   = 50;
    instances  = 1;
    cpuCores   = 1;
}

double Database::calculateProcessTime(const std::shared_ptr<Request>& req) {
    double base = Component::calculateProcessTime(req);
    // ~20% of operations are write transactions taking 2x time
    bool isWrite = (req && (req->id % 5 == 0));
    return isWrite ? (base * writeMultiplier) : base;
}

LoadBalancer::LoadBalancer(int id, std::string name)
    : Component(id, std::move(name), "loadbalancer") {
    procTimeMs = 1.0;
    maxQueue   = 500;
    instances  = 4;
    cpuCores   = 4;
}

void LoadBalancer::forwardRequest(std::shared_ptr<Request> req, double simNowSec) {
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
        // Least Connections: Pick downstream node with minimal queue + active workers
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

RedisCache::RedisCache(int id, std::string name)
    : Component(id, std::move(name), "redis") {
    procTimeMs = 1.0;
    maxQueue   = 1000;
    instances  = 4;
    cpuCores   = 2;
}

bool RedisCache::isCacheHit(const std::shared_ptr<Request>& req) const {
    if (!req) return true;
    return ((req->id * 73) % 100) < static_cast<unsigned>(hitRatio * 100.0);
}

double RedisCache::calculateProcessTime(const std::shared_ptr<Request>& req) {
    bool isHit = isCacheHit(req);
    return isHit ? 1.0 : 3.0; // 1ms for hit, 3ms for miss lookup
}

void RedisCache::forwardRequest(std::shared_ptr<Request> req, double simNowSec) {
    bool isHit = isCacheHit(req);

    if (isHit || outgoing_.empty()) {
        // Cache Hit: Served directly from cache
        req->status = RequestStatus::Completed;
        req->completedAtSec = simNowSec;
    } else {
        // Cache Miss: Forward downstream to Database
        outgoing_[0]->receiveRequest(req, simNowSec);
    }
}

MessageQueue::MessageQueue(int id, std::string name)
    : Component(id, std::move(name), "queue") {
    procTimeMs = 5.0;
    maxQueue   = 10000;
    instances  = 2;
    cpuCores   = 2;
}

// ─────────────────────────────────────────────────────────────────────────────
// SimulationEngine Implementation
// ─────────────────────────────────────────────────────────────────────────────

SimulationEngine::SimulationEngine() = default;

SimulationEngine::~SimulationEngine() {
    stop();
}

void SimulationEngine::addComponent(std::shared_ptr<Component> comp) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    if (comp) {
        components_[comp->getId()] = comp;
    }
}

void SimulationEngine::addEdge(int fromId, int toId) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    Component* from = getComponent(fromId);
    Component* to = getComponent(toId);
    if (!from || !to) {
        throw std::runtime_error("Cannot add edge: Unknown component ID");
    }
    from->addOutgoing(to);
    to->addIncoming(from);
}

void SimulationEngine::setEntryPoint(int componentId) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    entryPointId_ = componentId;
}

void SimulationEngine::start() {
    if (components_.empty()) {
        throw std::runtime_error("Cannot start simulation: No components registered");
    }

    srand(static_cast<unsigned>(config_.seed));
    isRunning_.store(true);
    double dt = config_.tickSec > 0.0 ? config_.tickSec : 0.01;

    while (isRunning_.load() && simNowSec_ <= config_.durationSec) {
        if (config_.totalRequests > 0 && totalRequests_ >= config_.totalRequests) {
            bool hasInFlight = false;
            for (const auto& kv : components_) {
                if (kv.second->getQueueDepth() > 0 || kv.second->getBusyWorkers() > 0) {
                    hasInFlight = true;
                    break;
                }
            }
            if (!hasInFlight) break;
        }

        step(dt);
    }

    isRunning_.store(false);
}

void SimulationEngine::step(double dtSec) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    if (components_.empty()) return;

    double dt = dtSec > 0.0 ? dtSec : 0.01;

    generateRequests(dt);
    updateComponents(dt);
    simNowSec_ += dt;

    if (onTick) {
        onTick(getSystemMetrics());
    }
}

void SimulationEngine::stop() {
    isRunning_.store(false);
    if (simThread_.joinable()) {
        simThread_.join();
    }
}

void SimulationEngine::reset() {
    std::lock_guard<std::mutex> lock(engineMutex_);
    stop();
    simNowSec_ = 0.0;
    nextRequestId_ = 1;
    totalRequests_ = 0;
    completedRequests_ = 0;
    failedRequests_ = 0;
    droppedRequests_ = 0;
    sumLatencyMs_ = 0.0;
    allRequests_.clear();
    completedLatenciesMs_.clear();

    for (auto& kv : components_) {
        kv.second->reset();
    }
}

void SimulationEngine::generateRequests(double dtSec) {
    if (config_.totalRequests > 0 && totalRequests_ >= config_.totalRequests) return;

    Component* entry = getEntryPoint();
    if (!entry) return;

    double expectedArrivals = config_.requestRatePerSec * dtSec;
    int count = static_cast<int>(expectedArrivals);
    double remainder = expectedArrivals - count;

    if ((static_cast<double>(rand()) / RAND_MAX) < remainder) {
        count++;
    }

    for (int i = 0; i < count; i++) {
        if (config_.totalRequests > 0 && totalRequests_ >= config_.totalRequests) break;

        auto req = std::make_shared<Request>();
        req->id = nextRequestId_++;
        req->clientId = entryPointId_;
        req->createdAtSec = simNowSec_;
        req->status = RequestStatus::Created;

        allRequests_.push_back(req);
        totalRequests_++;

        bool received = entry->receiveRequest(req, simNowSec_);
        if (!received) {
            droppedRequests_++;
        }
    }
}

void SimulationEngine::updateComponents(double dtSec) {
    for (auto& kv : components_) {
        auto finished = kv.second->update(dtSec, simNowSec_);

        for (auto& req : finished) {
            if (req->isDone()) {
                if (req->isSuccess()) {
                    completedRequests_++;
                    double roundTripMs = req->totalRoundTripMs();
                    if (roundTripMs <= 0.0) {
                        roundTripMs = (req->completedAtSec - req->createdAtSec) * 1000.0;
                    }
                    if (roundTripMs < 0.0) roundTripMs = 0.0;

                    sumLatencyMs_ += roundTripMs;
                    completedLatenciesMs_.push_back(roundTripMs);
                } else if (req->status == RequestStatus::Dropped) {
                    droppedRequests_++;
                } else {
                    failedRequests_++;
                }
            }
        }
    }
}

SystemMetrics SimulationEngine::getSystemMetrics() const {
    SystemMetrics m;
    m.simTimeSec = simNowSec_;
    m.totalRequests = totalRequests_;
    m.completed = completedRequests_;
    m.failed = failedRequests_;
    m.dropped = droppedRequests_;

    uint64_t inFlight = 0;
    for (const auto& kv : components_) {
        inFlight += static_cast<uint64_t>(kv.second->getQueueDepth() + kv.second->getBusyWorkers());
        m.perComponent[kv.first] = kv.second->getMetrics();
    }
    m.inFlight = inFlight;

    m.avgLatencyMs = completedRequests_ > 0
        ? (sumLatencyMs_ / static_cast<double>(completedRequests_))
        : 0.0;

    // 99th Percentile Latency via std::nth_element
    if (!completedLatenciesMs_.empty()) {
        std::vector<double> copy = completedLatenciesMs_;
        size_t idx = static_cast<size_t>(std::ceil(0.99 * copy.size())) - 1;
        if (idx >= copy.size()) idx = copy.size() - 1;

        std::nth_element(copy.begin(), copy.begin() + idx, copy.end());
        m.p99LatencyMs = copy[idx];
    } else {
        m.p99LatencyMs = 0.0;
    }

    m.throughputPerSec = simNowSec_ > 0.0
        ? (static_cast<double>(completedRequests_) / simNowSec_)
        : 0.0;

    return m;
}

Component* SimulationEngine::getComponent(int id) const {
    auto it = components_.find(id);
    return it != components_.end() ? it->second.get() : nullptr;
}

Component* SimulationEngine::getEntryPoint() const {
    return getComponent(entryPointId_);
}

// ─────────────────────────────────────────────────────────────────────────────
// JsonParser Implementation
// ─────────────────────────────────────────────────────────────────────────────

void JsonParser::fillFromJson(SimulationEngine& engine, const nlohmann::json& j) {
    if (!j.contains("nodes")) {
        throw std::runtime_error("Invalid architecture JSON: 'nodes' array required");
    }

    engine.reset();
    const auto& nodes = j.at("nodes");

    for (size_t i = 0; i < nodes.size(); i++) {
        const auto& n = nodes.at(i);
        int id = static_cast<int>(n.at("id"));
        std::string type = static_cast<std::string>(n.at("type"));
        std::string name = n.contains("name")
            ? static_cast<std::string>(n.at("name"))
            : (type + "-" + std::to_string(id));

        std::shared_ptr<Component> comp;
        if (type == "client")       comp = std::make_shared<Client>(id, name);
        else if (type == "server")  comp = std::make_shared<Server>(id, name);
        else if (type == "database") comp = std::make_shared<Database>(id, name);
        else if (type == "loadbalancer") comp = std::make_shared<LoadBalancer>(id, name);
        else if (type == "redis")   comp = std::make_shared<RedisCache>(id, name);
        else if (type == "queue")   comp = std::make_shared<MessageQueue>(id, name);
        else comp = std::make_shared<Server>(id, name);

        if (n.contains("config")) {
            const auto& cfg = n.at("config");
            if (cfg.contains("procTime"))   comp->procTimeMs = static_cast<double>(cfg.at("procTime"));
            if (cfg.contains("maxQueue"))   comp->maxQueue   = static_cast<int>(cfg.at("maxQueue"));
            if (cfg.contains("instances"))  comp->instances  = static_cast<int>(cfg.at("instances"));
            if (cfg.contains("cpuCores"))   comp->cpuCores   = static_cast<int>(cfg.at("cpuCores"));

            if (type == "client") {
                if (cfg.contains("requestRate")) {
                    engine.config().requestRatePerSec = static_cast<double>(cfg.at("requestRate"));
                }
                if (cfg.contains("totalRequests")) {
                    engine.config().totalRequests = static_cast<uint64_t>(static_cast<double>(cfg.at("totalRequests")));
                }
            }
        }

        engine.addComponent(comp);
    }

    if (j.contains("edges")) {
        const auto& edges = j.at("edges");
        for (size_t i = 0; i < edges.size(); i++) {
            const auto& e = edges.at(i);
            int fromId = static_cast<int>(e.at("fromId"));
            int toId   = static_cast<int>(e.at("toId"));
            engine.addEdge(fromId, toId);
        }
    }

    if (j.contains("simulation")) {
        const auto& sc = j.at("simulation");
        if (sc.contains("durationSec"))       engine.config().durationSec       = static_cast<double>(sc.at("durationSec"));
        if (sc.contains("tickSec"))            engine.config().tickSec            = static_cast<double>(sc.at("tickSec"));
        if (sc.contains("requestRatePerSec")) engine.config().requestRatePerSec = static_cast<double>(sc.at("requestRatePerSec"));
        if (sc.contains("totalRequests"))     engine.config().totalRequests      = static_cast<uint64_t>(static_cast<double>(sc.at("totalRequests")));
        if (sc.contains("seed"))              engine.config().seed               = static_cast<uint64_t>(static_cast<double>(sc.at("seed")));
    }

    int entryId = -1;
    for (size_t i = 0; i < nodes.size(); i++) {
        if (static_cast<std::string>(nodes.at(i).at("type")) == "client") {
            entryId = static_cast<int>(nodes.at(i).at("id"));
            break;
        }
    }
    if (entryId == -1 && nodes.size() > 0) {
        entryId = static_cast<int>(nodes.at(0).at("id"));
    }
    if (entryId != -1) {
        engine.setEntryPoint(entryId);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Simulator Implementation
// ─────────────────────────────────────────────────────────────────────────────

Simulator::Simulator()
    : engine_(std::make_unique<SimulationEngine>()) {}

Simulator::~Simulator() {
    stop();
}

void Simulator::loadArchitecture(const std::string& architectureJson) {
    std::lock_guard<std::mutex> lock(simMutex_);
    if (!engine_) {
        engine_ = std::make_unique<SimulationEngine>();
    }
    nlohmann::json j = nlohmann::json::parse(architectureJson);
    JsonParser::fillFromJson(*engine_, j);
}

void Simulator::start() {
    if (engine_) {
        engine_->start();
    }
}

void Simulator::step(double dtSec) {
    std::lock_guard<std::mutex> lock(simMutex_);
    if (engine_) {
        engine_->step(dtSec);
    }
}

void Simulator::stop() {
    if (engine_) {
        engine_->stop();
    }
}

bool Simulator::isRunning() const {
    return engine_ ? engine_->isRunning() : false;
}

SystemMetrics Simulator::getMetrics() const {
    if (engine_) {
        return engine_->getSystemMetrics();
    }
    return SystemMetrics{};
}

void Simulator::reset() {
    std::lock_guard<std::mutex> lock(simMutex_);
    if (engine_) {
        engine_->reset();
    }
}

} // namespace archisys
