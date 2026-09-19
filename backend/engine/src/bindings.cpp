#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "engine.hpp"

namespace py = pybind11;
using namespace archisys;

PYBIND11_MODULE(archisys_cpp, m) {
    m.doc() = "ArchiSys C++ Simulation Engine pybind11 Native Extension";

    // ── ComponentMetrics ─────────────────────────────────────────────────────
    py::class_<ComponentMetrics>(m, "ComponentMetrics")
        .def(py::init<>())
        .def_readonly("id",                 &ComponentMetrics::id)
        .def_readonly("name",               &ComponentMetrics::name)
        .def_readonly("type",               &ComponentMetrics::type)
        .def_readonly("cpu_usage_pct",      &ComponentMetrics::cpuUsagePct)
        .def_readonly("queue_depth",        &ComponentMetrics::queueDepth)
        .def_readonly("max_queue",          &ComponentMetrics::maxQueue)
        .def_readonly("requests_received",  &ComponentMetrics::requestsReceived)
        .def_readonly("requests_completed", &ComponentMetrics::requestsCompleted)
        .def_readonly("requests_dropped",   &ComponentMetrics::requestsDropped)
        .def_readonly("avg_processing_ms",  &ComponentMetrics::avgProcessingMs)
        .def_readonly("avg_queue_wait_ms",  &ComponentMetrics::avgQueueWaitMs)
        .def_readonly("throughput_per_sec", &ComponentMetrics::throughputPerSec)
        .def("to_dict", [](const ComponentMetrics& cm) {
            py::dict d;
            d["id"]                 = cm.id;
            d["name"]               = cm.name;
            d["type"]               = cm.type;
            d["cpuUsagePct"]        = cm.cpuUsagePct;
            d["queueDepth"]         = cm.queueDepth;
            d["maxQueue"]           = cm.maxQueue;
            d["requestsReceived"]   = cm.requestsReceived;
            d["requestsCompleted"]  = cm.requestsCompleted;
            d["requestsDropped"]    = cm.requestsDropped;
            d["avgProcessingMs"]    = cm.avgProcessingMs;
            d["avgQueueWaitMs"]     = cm.avgQueueWaitMs;
            d["throughputPerSec"]   = cm.throughputPerSec;
            return d;
        });

    // ── SystemMetrics ────────────────────────────────────────────────────────
    py::class_<SystemMetrics>(m, "SystemMetrics")
        .def(py::init<>())
        .def_readonly("sim_time_sec",       &SystemMetrics::simTimeSec)
        .def_readonly("total_requests",     &SystemMetrics::totalRequests)
        .def_readonly("completed",          &SystemMetrics::completed)
        .def_readonly("failed",             &SystemMetrics::failed)
        .def_readonly("dropped",            &SystemMetrics::dropped)
        .def_readonly("in_flight",          &SystemMetrics::inFlight)
        .def_readonly("avg_latency_ms",     &SystemMetrics::avgLatencyMs)
        .def_readonly("p99_latency_ms",     &SystemMetrics::p99LatencyMs)
        .def_readonly("throughput_per_sec", &SystemMetrics::throughputPerSec)
        .def_property_readonly("components", [](const SystemMetrics& sm) {
            std::vector<ComponentMetrics> list;
            list.reserve(sm.perComponent.size());
            for (const auto& kv : sm.perComponent) {
                list.push_back(kv.second);
            }
            return list;
        })
        .def("to_dict", [](const SystemMetrics& sm) {
            py::dict d;
            d["type"]             = "metrics";
            d["simTimeSec"]       = sm.simTimeSec;
            d["totalRequests"]    = sm.totalRequests;
            d["completed"]        = sm.completed;
            d["failed"]           = sm.failed;
            d["dropped"]          = sm.dropped;
            d["inFlight"]         = sm.inFlight;
            d["avgLatencyMs"]     = sm.avgLatencyMs;
            d["p99LatencyMs"]     = sm.p99LatencyMs;
            d["throughputPerSec"] = sm.throughputPerSec;

            py::list comps;
            for (const auto& kv : sm.perComponent) {
                const auto& cm = kv.second;
                py::dict c;
                c["id"]                 = cm.id;
                c["name"]               = cm.name;
                c["type"]               = cm.type;
                c["cpuUsagePct"]        = cm.cpuUsagePct;
                c["queueDepth"]         = cm.queueDepth;
                c["maxQueue"]           = cm.maxQueue;
                c["requestsReceived"]   = cm.requestsReceived;
                c["requestsCompleted"]  = cm.requestsCompleted;
                c["requestsDropped"]    = cm.requestsDropped;
                c["avgProcessingMs"]    = cm.avgProcessingMs;
                c["avgQueueWaitMs"]     = cm.avgQueueWaitMs;
                c["throughputPerSec"]   = cm.throughputPerSec;
                comps.append(c);
            }
            d["components"] = comps;
            return d;
        });

    // ── Simulator Facade ─────────────────────────────────────────────────────
    py::class_<Simulator>(m, "Simulator")
        .def(py::init<>())
        .def("load_architecture", &Simulator::loadArchitecture, py::arg("architecture_json"))
        .def("start",             &Simulator::start,             py::call_guard<py::gil_scoped_release>())
        .def("step",              &Simulator::step,              py::arg("dt_sec") = 0.01, py::call_guard<py::gil_scoped_release>())
        .def("stop",              &Simulator::stop)
        .def("is_running",        &Simulator::isRunning)
        .def("get_metrics",       &Simulator::getMetrics)
        .def("reset",             &Simulator::reset);
}
