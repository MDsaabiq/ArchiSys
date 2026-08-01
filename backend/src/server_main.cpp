// server_main.cpp — ArchiSys HTTP + WebSocket API Server
// Standard library MUST come before winsock2.h to avoid WIN32_LEAN_AND_MEAN stripping threading types.
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <vector>
#include <memory>
#include <algorithm>
#include <sstream>
#include <string>
#include <cstdint>

// WinSock2 after stdlib
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif

#include "simulation_engine.hpp"
#include "components/components.hpp"
#include "json_parser.hpp"
#include "broadcast_buffer.hpp"
#include "http_server.hpp"
#include "ws_server.hpp"
#include "metrics.hpp"
#include <nlohmann/json.hpp>

using namespace archisys;

// ── Global state ─────────────────────────────────────────────────────────────
static std::unique_ptr<SimulationEngine>  gEngine;
static std::thread                         gSimThread;
static std::atomic<bool>                   gServerRunning{true};
static BroadcastBuffer                     gBroadcast;

static std::vector<std::shared_ptr<WsClient>> gClients;
static std::mutex                              gClientsMtx;

static constexpr int PORT = 8765;

// ── metricsToJson ─────────────────────────────────────────────────────────────
static std::string metricsToJson(const SystemMetrics& m) {
    nlohmann::json j;
    j["type"]             = std::string("metrics");
    j["simTimeSec"]       = m.simTimeSec;
    j["totalRequests"]    = (int64_t)m.totalRequests;
    j["completed"]        = (int64_t)m.completed;
    j["failed"]           = (int64_t)m.failed;
    j["dropped"]          = (int64_t)m.dropped;
    j["inFlight"]         = (int64_t)m.inFlight;
    j["avgLatencyMs"]     = m.avgLatencyMs;
    j["p99LatencyMs"]     = m.p99LatencyMs;
    j["throughputPerSec"] = m.throughputPerSec;

    nlohmann::json comps = nlohmann::json::array();
    for (auto& kv : m.perComponent) {
        const auto& cm = kv.second;
        nlohmann::json c;
        c["id"]                = cm.componentId;
        c["name"]              = cm.componentName;
        c["type"]              = cm.componentType;
        c["cpuUsagePct"]       = cm.cpuUsagePct;
        c["queueDepth"]        = cm.queueDepth;
        c["maxQueue"]          = cm.maxQueue;
        c["requestsReceived"]  = (int64_t)cm.requestsReceived;
        c["requestsCompleted"] = (int64_t)cm.requestsCompleted;
        c["requestsDropped"]   = (int64_t)cm.requestsDropped;
        c["avgProcessingMs"]   = cm.avgProcessingMs;
        c["avgQueueWaitMs"]    = cm.avgQueueWaitMs;
        c["throughputPerSec"]  = cm.throughputPerSec;
        comps.push_back(c);
    }
    j["components"] = comps;
    return j.dump();
}

// ── stopSimulation ────────────────────────────────────────────────────────────
static void stopSimulation() {
    if (gEngine) {
        gEngine->stop();
        if (gSimThread.joinable()) gSimThread.join();
        gEngine.reset();
        std::cout << "[ArchiSys] Simulation stopped.\n";
    }
}

// ── handleStart ───────────────────────────────────────────────────────────────
static HttpResponse handleStart(const HttpRequest& req) {
    if (req.body.empty())
        return HttpResponse::error(400, "Empty body");

    stopSimulation(); // stop any running sim first

    try {
        nlohmann::json j = nlohmann::json::parse(req.body);
        auto engine = std::unique_ptr<SimulationEngine>(new SimulationEngine());
        JsonParser::fillFromJson(*engine, j);

        // Apply simulation config if present
        if (j.contains("simulation")) {
            const auto& sc = j.at("simulation");
            if (sc.contains("durationSec"))       engine->config().durationSec       = (double)sc.at("durationSec");
            if (sc.contains("tickSec"))            engine->config().tickSec            = (double)sc.at("tickSec");
            if (sc.contains("requestRatePerSec")) engine->config().requestRatePerSec = (double)sc.at("requestRatePerSec");
            if (sc.contains("totalRequests"))     engine->config().totalRequests      = (uint64_t)(double)sc.at("totalRequests");
            if (sc.contains("seed"))              engine->config().seed               = (uint64_t)(double)sc.at("seed");
        }

        // If a Client node exists, its requestRate overrides the simulation config
        for (const auto& n : j.at("nodes")) {
            std::string t = static_cast<std::string>(n.at("type"));
            if (t == "client" && n.contains("config")) {
                const auto& cfg = n.at("config");
                if (cfg.contains("requestRate"))
                    engine->config().requestRatePerSec = (double)cfg.at("requestRate");
                if (cfg.contains("totalRequests") && engine->config().totalRequests == 0)
                    engine->config().totalRequests = (uint64_t)(double)cfg.at("totalRequests");
            }
        }

        // Wire onTick → BroadcastBuffer
        engine->onTick = [](const SystemMetrics& m) {
            gBroadcast.put(metricsToJson(m));
        };

        gEngine = std::move(engine);

        // Launch sim on background thread
        gSimThread = std::thread([]() {
            try { gEngine->start(); }
            catch (const std::exception& ex) {
                std::cerr << "[ArchiSys] Sim error: " << ex.what() << "\n";
            }
            std::cout << "[ArchiSys] Simulation finished.\n";
        });

        std::cout << "[ArchiSys] Simulation started.\n";
        return HttpResponse::ok("{\"started\":true}");
    }
    catch (const std::exception& ex) {
        return HttpResponse::error(400, ex.what());
    }
}

// ── handleStop ────────────────────────────────────────────────────────────────
static HttpResponse handleStop() {
    stopSimulation();
    return HttpResponse::ok("{\"stopped\":true}");
}

// ── handleWs ─────────────────────────────────────────────────────────────────
// Runs on a dedicated thread per WebSocket client.
static void handleWs(SOCKET s, const HttpRequest& req) {
    auto ws = std::make_shared<WsClient>(s);
    if (!ws->doHandshake(req.rawRequest)) {
        std::cerr << "[ArchiSys] WS handshake failed\n";
        return;
    }

    std::cout << "[ArchiSys] WebSocket client connected.\n";
    { std::lock_guard<std::mutex> lk(gClientsMtx); gClients.push_back(ws); }

    // Receive loop — handle ping/close from client.
    // recv() blocks for up to 100ms (SO_RCVTIMEO) then returns, so no sleep needed.
    while (!ws->isClosed() && gServerRunning) {
        std::string frame;
        ws->recv(frame);
    }

    // Remove from client list
    { std::lock_guard<std::mutex> lk(gClientsMtx);
      gClients.erase(std::remove(gClients.begin(), gClients.end(), ws), gClients.end()); }
    std::cout << "[ArchiSys] WebSocket client disconnected.\n";
}

// ── handleOptions ─────────────────────────────────────────────────────────────
static HttpResponse handleOptions() {
    HttpResponse r;
    r.status      = 200;
    r.body        = "";
    r.contentType = "text/plain";
    return r;
}

// ── handleClient ─────────────────────────────────────────────────────────────
static void handleClient(SOCKET s) {
    HttpRequest req;
    if (!readHttpRequest(s, req)) { closesocket(s); return; }

    // CORS preflight
    if (req.method == "OPTIONS") {
        writeHttpResponse(s, handleOptions());
        closesocket(s);
        return;
    }

    if (req.path == "/health") {
        writeHttpResponse(s, HttpResponse::ok("{\"ok\":true}"));
        closesocket(s);
        return;
    }
    if (req.path == "/start" && req.method == "POST") {
        writeHttpResponse(s, handleStart(req));
        closesocket(s);
        return;
    }
    if (req.path == "/stop" && req.method == "POST") {
        writeHttpResponse(s, handleStop());
        closesocket(s);
        return;
    }
    if (req.path == "/ws" && req.isWebSocketUpgrade()) {
        // handleWs takes ownership of the socket; don't closesocket here
        handleWs(s, req);
        closesocket(s);
        return;
    }
    writeHttpResponse(s, HttpResponse::error(404, "Not found"));
    closesocket(s);
}

// ── main ──────────────────────────────────────────────────────────────────────
int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n"; return 1;
    }
#endif

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        std::cerr << "socket() failed\n"; return 1;
    }

    // SO_REUSEADDR
    int reuse = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(PORT);

    if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed on port " << PORT << "\n"; return 1;
    }
    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen() failed\n"; return 1;
    }

    std::cout << "[ArchiSys] Listening on http://localhost:" << PORT << "\n";
    std::cout << "[ArchiSys] POST /start  — start simulation\n";
    std::cout << "[ArchiSys] POST /stop   — stop simulation\n";
    std::cout << "[ArchiSys] GET  /health — health check\n";
    std::cout << "[ArchiSys] GET  /ws     — WebSocket metrics stream\n\n";

    // ── Broadcaster thread: drain BroadcastBuffer → send to all WS clients ──
    std::thread broadcaster([]() {
        while (gServerRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::string msg;
            if (!gBroadcast.take(msg)) continue;
            std::lock_guard<std::mutex> lk(gClientsMtx);
            for (auto it = gClients.begin(); it != gClients.end(); ) {
                if ((*it)->isClosed()) {
                    it = gClients.erase(it);
                } else {
                    (*it)->send(msg);
                    ++it;
                }
            }
        }
    });
    broadcaster.detach();

    // ── Accept loop ──────────────────────────────────────────────────────────
    while (gServerRunning) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(listenSock, &fds);
        timeval tv{0, 50000}; // 50ms timeout
        int sel = select(static_cast<int>(listenSock + 1), &fds, nullptr, nullptr, &tv);
        if (sel <= 0) continue;

        SOCKET clientSock = accept(listenSock, nullptr, nullptr);
        if (clientSock == INVALID_SOCKET) continue;

        std::thread(handleClient, clientSock).detach();
    }

    stopSimulation();
    closesocket(listenSock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
