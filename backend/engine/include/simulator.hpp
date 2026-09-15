#pragma once
#include "metrics.hpp"
#include "simulation_engine.hpp"
#include <string>
#include <memory>
#include <mutex>

namespace archisys {

/**
 * Simulator is a facade providing a clean, thread-safe, and minimal interface
 * exposed to Python via pybind11.
 * Python interacts exclusively with this class rather than internal C++ entities.
 */
class Simulator {
public:
    Simulator();
    ~Simulator();

    // Loads and builds the architecture graph from a JSON string
    void loadArchitecture(const std::string& architectureJson);

    // Starts the simulation execution loop (blocking until duration or stopped)
    void start();

    // Runs a single discrete simulation step (e.g. 0.01s)
    void step(double dtSec);

    // Stops the simulation execution loop
    void stop();

    // Returns true if the simulation engine is currently running
    bool isRunning() const;

    // Returns a complete live telemetry snapshot
    SystemMetrics getMetrics() const;

    // Resets the simulator state
    void reset();

private:
    std::unique_ptr<SimulationEngine> engine_;
    mutable std::mutex simulatorMutex_;
};

} // namespace archisys
