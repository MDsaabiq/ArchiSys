#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

namespace archisys {

// Per-component live metrics snapshot
struct ComponentMetrics {
    int         componentId;
    std::string componentName;
    std::string componentType;

    // Queue
    int    queueDepth    = 0;
    int    maxQueue      = 0;

    // CPU / utilisation
    double cpuUsagePct   = 0.0;  // 0–100

    // Request counters
    uint64_t requestsReceived  = 0;
    uint64_t requestsCompleted = 0;
    uint64_t requestsDropped   = 0;

    // Latency (rolling averages, ms)
    double avgProcessingMs  = 0.0;
    double avgQueueWaitMs   = 0.0;

    // Throughput
    double throughputPerSec = 0.0;  // requests completed per sim-second
};

// System-wide aggregated metrics
struct SystemMetrics {
    double   simTimeSec      = 0.0;

    uint64_t totalRequests   = 0;
    uint64_t completed       = 0;
    uint64_t failed          = 0;
    uint64_t dropped         = 0;
    uint64_t inFlight        = 0;

    double   avgLatencyMs    = 0.0;
    double   p99LatencyMs    = 0.0;
    double   throughputPerSec= 0.0;

    std::unordered_map<int, ComponentMetrics> perComponent;
};

} // namespace archisys
