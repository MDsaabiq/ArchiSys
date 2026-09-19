#pragma once
#include "metrics.hpp"
#include "simulation_engine.hpp"
#include <string>
#include <memory>
#include <mutex>

namespace archisys {

/**
 * Simulator is a high-level facade providing a clean, thread-safe interface
 * exposed to Python via pybind11.
 */
class Simulator {
public:
    Simulator();
    ~Simulator();

    // Loads graph topology and node parameters from a JSON string
    void loadArchitecture(const std::string& architectureJson);

    // Starts the simulation execution
    void start();

    // Executes a single discrete time step
    void step(double dtSec = 0.01);

    // Stops the simulation
    void stop();

    // Checks if simulation is currently running
    bool isRunning() const;

    // Returns a live telemetry snapshot
    SystemMetrics getMetrics() const;

    // Resets the simulator
    void reset();

private:
    std::unique_ptr<SimulationEngine> engine_;
    mutable std::mutex simMutex_;
};

} // namespace archisys
