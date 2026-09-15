#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace archisys {

/**
 * Live metric snapshot for an individual system component.
 */
struct ComponentMetrics {
    int         componentId       = 0;
    std::string componentName     = "";
    std::string componentType     = "";

    // Queue depth and capacity
    int queueDepth = 0; // Current requests sitting in wait queue
    int maxQueue   = 0; // Max queue buffer capacity before drops occur

    // Resource utilization (0.0 to 100.0 %)
    double cpuUsagePct = 0.0;

    // Cumulative counters
    uint64_t requestsReceived  = 0;
    uint64_t requestsCompleted = 0;
    uint64_t requestsDropped   = 0;

    // Rolling latency averages (in milliseconds)
    double avgProcessingMs = 0.0; // Mean time spent actively computing in worker slots
    double avgQueueWaitMs  = 0.0; // Mean time spent waiting in the incoming FIFO queue

    // Throughput (requests completed per second)
    double throughputPerSec = 0.0;
};

/**
 * System-wide aggregated simulation metrics.
 */
struct SystemMetrics {
    double simTimeSec = 0.0; // Current simulation elapsed time in seconds

    uint64_t totalRequests = 0; // Total requests generated across all clients
    uint64_t completed     = 0; // Requests successfully completed
    uint64_t failed        = 0; // Requests failed
    uint64_t dropped       = 0; // Requests dropped due to queue buffer overflow
    uint64_t inFlight      = 0; // Requests currently queued or actively processing

    // End-to-end latency metrics (in milliseconds)
    double avgLatencyMs = 0.0; // Mean round-trip latency of completed requests
    double p99LatencyMs = 0.0; // 99th percentile round-trip latency

    // System-wide throughput
    double throughputPerSec = 0.0; // Requests completed per simulation second

    // Breakdown per component
    std::unordered_map<int, ComponentMetrics> perComponent;
};

} // namespace archisys
