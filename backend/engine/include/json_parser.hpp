#pragma once
#include "simulation_engine.hpp"
#include "components.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <stdexcept>
#include <iostream>

namespace archisys {

/**
 * JsonParser deserializes architecture graph payloads from JSON format
 * and initializes a fully wired SimulationEngine.
 */
class JsonParser {
public:
    static void fillFromJson(SimulationEngine& engine, const nlohmann::json& j) {
        if (!j.contains("nodes")) {
            throw std::runtime_error("Invalid architecture JSON: 'nodes' array required");
        }

        engine.reset();

        const auto& nodes = j.at("nodes");

        // ── 1. Parse Nodes ───────────────────────────────────────────────────
        for (size_t i = 0; i < nodes.size(); ++i) {
            const auto& n = nodes.at(i);
            int         id   = static_cast<int>(n.at("id"));
            std::string type = static_cast<std::string>(n.at("type"));
            std::string name = n.contains("name") ? static_cast<std::string>(n.at("name")) : (type + "-" + std::to_string(id));

            std::shared_ptr<Component> comp = makeComponent(id, type, name);

            // Apply node-specific configuration
            if (n.contains("config")) {
                const auto& cfg = n.at("config");
                if (cfg.contains("procTime"))   comp->processingMs = static_cast<double>(cfg.at("procTime"));
                if (cfg.contains("maxQueue"))   comp->maxQueue     = static_cast<int>(cfg.at("maxQueue"));
                if (cfg.contains("instances"))  comp->instances    = static_cast<int>(cfg.at("instances"));
                if (cfg.contains("cpuCores"))   comp->cpuCores     = static_cast<int>(cfg.at("cpuCores"));

                // If Client node has traffic generator configuration
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

        // ── 2. Parse Edges (Connections) ─────────────────────────────────────
        if (j.contains("edges")) {
            const auto& edges = j.at("edges");
            for (size_t i = 0; i < edges.size(); ++i) {
                const auto& e = edges.at(i);
                int fromId = static_cast<int>(e.at("fromId"));
                int toId   = static_cast<int>(e.at("toId"));
                engine.addEdge(fromId, toId);
            }
        }

        // ── 3. Parse Simulation Config ───────────────────────────────────────
        if (j.contains("simulation")) {
            const auto& sc = j.at("simulation");
            if (sc.contains("durationSec"))       engine.config().durationSec       = static_cast<double>(sc.at("durationSec"));
            if (sc.contains("tickSec"))            engine.config().tickSec            = static_cast<double>(sc.at("tickSec"));
            if (sc.contains("requestRatePerSec")) engine.config().requestRatePerSec = static_cast<double>(sc.at("requestRatePerSec"));
            if (sc.contains("totalRequests"))     engine.config().totalRequests      = static_cast<uint64_t>(static_cast<double>(sc.at("totalRequests")));
            if (sc.contains("seed"))              engine.config().seed               = static_cast<uint64_t>(static_cast<double>(sc.at("seed")));
        }

        // ── 4. Set Entry Point ───────────────────────────────────────────────
        int entryId = -1;
        for (size_t i = 0; i < nodes.size(); ++i) {
            const auto& n = nodes.at(i);
            std::string t = static_cast<std::string>(n.at("type"));
            if (t == "client") {
                entryId = static_cast<int>(n.at("id"));
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

private:
    static std::shared_ptr<Component> makeComponent(int id, const std::string& type, const std::string& name) {
        if (type == "client")       return std::make_shared<Client>(id, name);
        if (type == "server")       return std::make_shared<Server>(id, name);
        if (type == "database")     return std::make_shared<Database>(id, name);
        if (type == "loadbalancer") return std::make_shared<LoadBalancer>(id, name);
        if (type == "redis")        return std::make_shared<RedisCache>(id, name);
        if (type == "queue")        return std::make_shared<MessageQueue>(id, name);
        // Default generic server fallback
        return std::make_shared<Server>(id, name);
    }
};

} // namespace archisys
