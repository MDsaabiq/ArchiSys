#pragma once
#include "simulation_engine.hpp"
#include "components/components.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <stdexcept>

namespace archisys {

// ─────────────────────────────────────────────────────────────────────────────
// Reads the JSON exported by the frontend (architecture.json) and builds
// a fully wired SimulationEngine ready to call start().
//
// Expected JSON schema (matches frontend export):
// {
//   "nodes": [
//     { "id": 1, "type": "server", "name": "App Server",
//       "config": { "cpuCores":4, "procTime":50, "maxQueue":100, "instances":2 } }
//   ],
//   "edges": [
//     { "id": 1, "fromId": 1, "toId": 2 }
//   ]
// }
// ─────────────────────────────────────────────────────────────────────────────
class JsonParser {
public:
    // Fills an already-allocated engine (avoids move of non-moveable atomic<bool>)
    static void fillFromJson(SimulationEngine& engine, const nlohmann::json& j) {

        // ── Nodes ──────────────────────────────────────────────────────────
        for (const auto& n : j.at("nodes")) {
            int         id   = static_cast<int>(n.at("id"));
            std::string type = static_cast<std::string>(n.at("type"));
            std::string name = static_cast<std::string>(n.at("name"));

            std::shared_ptr<Component> comp = makeComponent(id, type, name);

            // Apply config if present
            if (n.contains("config")) {
                const auto& cfg = n.at("config");
                if (cfg.contains("procTime"))  comp->processingMs = (double)cfg.at("procTime");
                if (cfg.contains("maxQueue"))  comp->maxQueue     = static_cast<int>(cfg.at("maxQueue"));
                if (cfg.contains("instances")) comp->instances    = static_cast<int>(cfg.at("instances"));
            }

            engine.addComponent(comp);
        }

        // ── Edges ──────────────────────────────────────────────────────────
        for (const auto& e : j.at("edges")) {
            int fromId = static_cast<int>(e.at("fromId"));
            int toId   = static_cast<int>(e.at("toId"));
            engine.addEdge(fromId, toId);
        }

        // ── Entry point: first node of type "client" or the first node ─────
        int entryId = static_cast<int>(j.at("nodes").at(0).at("id"));
        for (const auto& n : j.at("nodes")) {
            std::string t = static_cast<std::string>(n.at("type"));
            if (t == "client") { entryId = static_cast<int>(n.at("id")); break; }
        }
        engine.setEntryPoint(entryId);
    }

private:
    static std::shared_ptr<Component> makeComponent(int id, const std::string& type, const std::string& name) {
        if (type == "client")       return std::make_shared<Client>(id, name);
        if (type == "server")       return std::make_shared<Server>(id, name);
        if (type == "database")     return std::make_shared<Database>(id, name);
        if (type == "loadbalancer") return std::make_shared<LoadBalancer>(id, name);
        if (type == "redis")        return std::make_shared<RedisCache>(id, name);
        if (type == "queue")        return std::make_shared<MessageQueue>(id, name);
        // Generic fallback for unknown types
        return std::make_shared<Server>(id, name);
    }
};

} // namespace archisys
