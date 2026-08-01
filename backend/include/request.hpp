#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

namespace archisys {

// Every simulated user request is a first-class object.
// No real sockets, no HTTP – this is purely a simulation entity.

enum class RequestStatus {
    Created,
    InQueue,
    Processing,
    Forwarded,
    Completed,
    Failed,
    Dropped,
};

inline const char* statusName(RequestStatus s) {
    switch (s) {
        case RequestStatus::Created:    return "Created";
        case RequestStatus::InQueue:    return "InQueue";
        case RequestStatus::Processing: return "Processing";
        case RequestStatus::Forwarded:  return "Forwarded";
        case RequestStatus::Completed:  return "Completed";
        case RequestStatus::Failed:     return "Failed";
        case RequestStatus::Dropped:    return "Dropped";
    }
    return "Unknown";
}

struct RouteHop {
    int         componentId;
    std::string componentName;
    double      arrivalTime;    // sim-seconds
    double      departureTime;  // sim-seconds (0 if still there)
    double      processingTime; // ms spent in that component
};

struct Request {
    // ── Identity ──────────────────────────────────────────────
    uint64_t    id;
    uint64_t    clientId;       // which client component generated this

    // ── Timing (simulation seconds) ────────────────────────────
    double      createdAt;      // sim time when spawned
    double      completedAt;    // sim time when finished (0 = not done)

    // ── State ─────────────────────────────────────────────────
    RequestStatus status;
    int           currentComponentId;  // -1 = not yet placed

    // ── Accumulated metrics ────────────────────────────────────
    double      totalLatencyMs;   // sum of all processing delays seen so far
    double      totalQueueWaitMs; // time spent waiting in queues

    // ── Route history (one entry per component visited) ────────
    std::vector<RouteHop> route;

    // ── Helpers ───────────────────────────────────────────────
    bool isDone()    const { return status == RequestStatus::Completed || status == RequestStatus::Failed || status == RequestStatus::Dropped; }
    bool isSuccess() const { return status == RequestStatus::Completed; }

    double totalRoundTripMs(double simNow) const {
        if (completedAt > 0) return (completedAt - createdAt) * 1000.0;
        return (simNow - createdAt) * 1000.0;
    }
};

} // namespace archisys
