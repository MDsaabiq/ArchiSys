#include "simulator.hpp"
#include "json_parser.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace archisys {

Simulator::Simulator()
    : engine_(std::make_unique<SimulationEngine>()) {}

Simulator::~Simulator() {
    stop();
}

void Simulator::loadArchitecture(const std::string& architectureJson) {
    std::lock_guard<std::mutex> lk(simulatorMutex_);
    if (!engine_) {
        engine_ = std::make_unique<SimulationEngine>();
    }
    nlohmann::json j = nlohmann::json::parse(architectureJson);
    JsonParser::fillFromJson(*engine_, j);
}

void Simulator::start() {
    // start() will block inside engine_->start() while running.
    // GIL is released at the pybind11 boundary.
    if (engine_) {
        engine_->start();
    }
}

void Simulator::step(double dtSec) {
    std::lock_guard<std::mutex> lk(simulatorMutex_);
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
    std::lock_guard<std::mutex> lk(simulatorMutex_);
    if (engine_) {
        engine_->reset();
    }
}

} // namespace archisys
