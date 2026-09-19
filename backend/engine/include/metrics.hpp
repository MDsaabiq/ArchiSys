#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace archisys {

/**
 * Metric snapshot for an individual component.
 */
struct ComponentMetrics {
    int id = 0;
    std::string name = "";
    std::string type = "";

    int queueDepth = 0;
    int maxQueue = 0;
    double cpuUsagePct = 0.0;

    uint64_t requestsReceived = 0;
    uint64_t requestsCompleted = 0;
    uint64_t requestsDropped = 0;

    double avgProcessingMs = 0.0;
    double avgQueueWaitMs = 0.0;
    double throughputPerSec = 0.0;
};

/**
 * Global system-wide telemetry metrics.
 */
struct SystemMetrics {
    double simTimeSec = 0.0;

    uint64_t totalRequests = 0;
    uint64_t completed = 0;
    uint64_t failed = 0;
    uint64_t dropped = 0;
    uint64_t inFlight = 0;

    double avgLatencyMs = 0.0;
    double p99LatencyMs = 0.0;
    double throughputPerSec = 0.0;

    std::unordered_map<int, ComponentMetrics> perComponent;
};

} // namespace archisys
