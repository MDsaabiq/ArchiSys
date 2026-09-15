# ArchiSys — Interactive System Design Simulator

## 1. Introduction

**ArchiSys** is an interactive distributed system design simulator that lets users visually architect distributed systems by dragging and connecting virtual components on a canvas, then simulates how requests flow through that architecture in real time with live metrics on every node.

**What it is NOT:** ArchiSys does not spin up real servers, containers, or generate real network traffic. It is a purely virtual, event-driven simulation engine for educational and analytical purposes — a "what-if" sandbox for system architects.

### The Core Idea

```
User designs architecture visually (drag + connect components)
        ↓
Client node: sets request rate + total requests
        ↓
Frontend opens WebSocket → then POSTs architecture JSON to C++ backend
        ↓
C++ engine: two-phase simulation loop
  Phase 1 — inject requests at configured rate until limit reached
  Phase 2 — drain all in-flight requests to completion
        ↓
Live metrics broadcast to frontend via WebSocket every 100ms
        ↓
Frontend: node cards update (CPU%, queue depth, latency), bottom bar, properties panel
        ↓
Simulation ends → WebSocket closes → UI resets to idle
```

### Technology Stack

| Layer | Technology | Rationale |
|---|---|---|
| **Frontend** | React 19 + TypeScript + Vite | Modern, fast HMR, type safety |
| **Canvas** | React Flow | Drag, connect, zoom, pan, custom nodes |
| **State** | Zustand 5 | Minimal, fast, subscription-based reactivity |
| **Backend** | C++17 | Performance-critical simulation engine |
| **Networking** | Raw WinSock2 + RFC 6455 WebSocket | Zero external dependencies |
| **JSON** | Custom minimal stub | Compatible with GCC 6.x through 16.x |
| **Build** | CMake 3.16+ | Cross-compiler support (MSVC, MinGW) |

### Project Structure

```
archisys/
├── frontend/          React + React Flow + Zustand (TypeScript)
│   ├── src/
│   │   ├── App.tsx            Layout shell + Canvas (React Flow wrapper)
│   │   ├── store/useStore.ts    Zustand global state
│   │   ├── hooks/               useWebSocket.ts, useSimulation.ts
│   │   ├── api/client.ts        fetch wrappers for /start, /stop
│   │   ├── nodes/               BaseNode.tsx (6 SVG shape components)
│   │   ├── edges/AnimatedEdge.tsx  Dashed edge + packet dot
│   │   ├── components/          Sidebar, Toolbar, PropertiesPanel, BottomBar
│   │   └── types/               architecture.ts, metrics.ts
│   └── package.json
├── backend/           C++ simulation engine + WebSocket API server
│   ├── include/       Headers (request, component, engine, metrics, ws, http, json_parser)
│   │   └── components/     Component subclasses
│   ├── src/           Implementations
│   │   ├── component.cpp        Base tick/queue/slot logic
│   │   ├── simulation_engine.cpp  Two-phase loop, request generation
│   │   ├── ws_server.cpp        SHA-1, Base64, RFC 6455 frame codec
│   │   ├── http_server.cpp      HTTP parse + CORS
│   │   ├── server_main.cpp      WinSock2 accept loop + broadcaster thread
│   │   └── main.cpp             CLI tool (runs sim without HTTP server)
│   ├── CMakeLists.txt   Two targets: archisys_sim + archisys_server
│   └── third_party/nlohmann/json.hpp  Custom minimal JSON stub
└── mockups/           Standalone HTML prototype (reference only)
```

---

## 2. Problem Statement

Modern system architects and engineering students face a fundamental challenge: **how do you predict the behavior of a distributed system before you deploy it?**

### The Specific Problems

1. **Bottleneck identification is hard.** When you design a system with a load balancer, app servers, a Redis cache, and a database, you can't easily tell which component will become the bottleneck under load. Will the app servers saturate? Will the Redis cache reduce database load enough? Traditional back-of-the-envelope calculations are error-prone and don't account for queueing dynamics.

2. **Queue overflow and request drops happen silently in production.** When request arrival rate exceeds processing capacity, requests queue up. When the queue fills, requests are dropped — often with cascading failures. Engineers rarely visualize this before hitting production.

3. **Educational gap in systems design.** Computer science students learn algorithms on paper but lack an interactive tool to visualize how latency, throughput, parallelism, and queueing interact in a distributed architecture. Textbook diagrams are static; real systems are dynamic.

4. **No lightweight, self-contained simulation tool exists.** Heavy tools like JMeter or Locust generate *real* traffic against *real* services. There was no tool that lets a student or architect draw a system and immediately simulate its behavior with proper queue modeling, processing latency, and real-time metrics — all in a single application with zero deployment prerequisites.

### What the User Needs

A tool where you can **drag 6 types of components** (Client, Server, Database, Load Balancer, Redis Cache, Message Queue), **wire them together**, **configure each one** (CPU cores, processing time, queue size, instances), set a **request rate**, and then **watch requests flow** through the system with live feedback on CPU%, queue depth, latency, throughput, and drop rates.

---

## 3. Solution

**ArchiSys provides a two-phase event-driven simulation** built entirely from scratch — a C++ engine for the computational core and a React frontend for the interactive canvas.

### The Solution Architecture (Live Demo Flow)

```
User drags component  → addNode()            → Zustand nodes[]
User connects ports   → React Flow onConnect → Zustand edges[]
User clicks Run       → openWebSocketFirst() → WS open
                      → POST /start          → engine starts
                      → setSimStatus('running')
Every 100ms           → WS message arrives   → applyMetrics()
                      → Zustand nodes[] updated (metrics on each node)
                      → Canvas rfNodes synced via prevNodesRef comparison
                      → Node cards re-render with live data
Sim finishes / Stop   → WS closes            → resetMetrics()
                      → node.data.metrics = undefined → cards show "—"
                      → simStatus = 'stopped'
```

### Key Design Decisions in the Solution

1. **Two-phase simulation loop.** The engine doesn't just run until the duration expires. It runs Phase 1 (inject requests at the configured rate) and then Phase 2 (drain all in-flight requests to completion). This ensures no requests are lost artificially — every injected request either completes or is genuinely dropped due to queue overflow.

2. **WebSocket-first ordering.** The frontend opens the WebSocket connection **before** calling `POST /start`. This is critical because the C++ engine runs faster than real-time (1000 requests at 100 r/s = 10 simulated seconds, but completes in ~10ms wall-clock). Without this ordering, the simulation could complete before the browser's socket is established.

3. **BroadcastBuffer as a thread-safe single-slot buffer.** The simulation thread produces metrics every tick (10ms). The broadcaster thread consumes and sends over WebSocket every 100ms. The single-slot buffer means the broadcaster always gets the latest metrics, discarding anything in between — this decouples simulation speed from network speed.

4. **Component hierarchy with Strategy pattern.** The base `Component` class handles all the queue management, slot scheduling, and CPU calculation generically. Subclasses like `LoadBalancer` (round-robin vs least-connections), `RedisCache` (probabilistic hit/miss), and `Database` (read vs write latency) override only what differs.

5. **No external dependencies on the backend.** The WebSocket RFC 6455 implementation is hand-rolled — SHA-1, Base64, frame encoding/decoding — with zero third-party libraries. The JSON parser is a custom minimal stub that works even with GCC 6.3.

---

## 4. Design Architecture

### System Architecture Diagram

```
┌─────────────────────────────────────────────────────┐
│              React Frontend (port 5173)              │
│  React Flow canvas · Zustand state · TypeScript      │
│                                                      │
│  Sidebar → Canvas → Properties Panel → Bottom Bar   │
└──────────────────┬──────────────────────────────────┘
                   │
                   │  1. WebSocket /ws  ← opens FIRST
                   │  2. POST /start    ← sends architecture JSON
                   │  3. POST /stop     ← manual stop
                   │  WebSocket streams metrics every 100ms
                   │
┌──────────────────▼──────────────────────────────────┐
│          C++ API Server (port 8765)                  │
│  Raw WinSock2 · RFC 6455 WebSocket · No lib deps     │
└──────────────────┬──────────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────────┐
│          C++ Simulation Engine                       │
│                                                      │
│  Phase 1: event queue → inject requests → tick       │
│  Phase 2: drain in-flight until all components idle  │
│  onTick callback → BroadcastBuffer → WS broadcaster  │
└─────────────────────────────────────────────────────┘
```

**Critical ordering:** The frontend opens the WebSocket connection **before** calling `POST /start`. The C++ engine runs microseconds of wall-clock time per simulated second — without this ordering the simulation could finish before the browser socket is open.

### Frontend Architecture

**Component hierarchy:**

```
App.tsx                          Layout shell: Toolbar, Sidebar, Canvas, PropertiesPanel, BottomBar
  ├── Toolbar.tsx                Run/Stop/Clear/Export + Sim Settings dropdown
  ├── Sidebar.tsx                Draggable palette (6 types, SVG icons, 3 sections)
  ├── Canvas (inline)            React Flow canvas with Background + Controls
  │   └── ArchNode (nodes/index.tsx)
  │       ├── Handle (target)    Left connection point
  │       ├── BaseNode.tsx       SVG shape + label + metrics panel
  │       │   ├── ShapeClient    Person silhouette (sky blue)
  │       │   ├── ShapeServer    Rack unit stack (blue)
  │       │   ├── ShapeDatabase  3D cylinder (purple)
  │       │   ├── ShapeLoadBalancer  Diamond with flow arrows (green)
  │       │   ├── ShapeRedis     Hexagon with lightning bolt (amber)
  │       │   └── ShapeQueue     Pipeline slots (red)
  │       └── Handle (source)    Right connection point
  ├── AnimatedEdge.tsx           Dashed indigo edge with packet dot (SVG animateMotion)
  ├── PropertiesPanel.tsx        Config sliders + live metric bars for selected node
  └── BottomBar.tsx              System-wide metrics strip + drop-rate warning banner
```

**State flow (React + Zustand):**

```
User drags component  → addNode()            → Zustand nodes[]
User connects ports   → React Flow onConnect → Zustand edges[]
Every 100ms WS msg    → applyMetrics()       → Zustand nodes[] updated (metrics on each node)
                      → prevNodesRef check   → setRfNodes(nodes) — syncs to React Flow canvas
```

**Key fix:** `App.tsx` uses `prevNodesRef` to detect any change in Zustand `nodes` (not just length) and calls `setRfNodes` — this ensures every 100ms metrics update reaches the React Flow canvas.

### Backend Architecture

**C++ Engine — Component hierarchy (OOP):**

```
Component (base class)
├── Client          — entry point, zero processing delay, injects requests
├── Server          — multi-instance, CPU usage, queue, configurable latency
├── Database        — single-threaded, read (20ms) / write (40ms, every 5th req)
├── LoadBalancer    — RoundRobin / Random / LeastConnections routing
├── RedisCache      — 80% probabilistic hit (1ms) / miss (5ms + forward)
└── MessageQueue    — async FIFO broker, 5ms enqueue latency
```

**Simulation engine — Two-phase loop:**

```
Phase 1 — Injection
  while running AND simNow <= durationSec AND totalRequests < limit:
    fire all events scheduled at or before simNow
    tick all components (complete slots, promote queue → slots)
    call onTick(SystemMetrics) → BroadcastBuffer
    simNow += tickSec (0.01s = 10ms)

Phase 2 — Drain (only when totalRequests limit was set)
  drainLimit = simNow + 10.0 simulated seconds
  while running AND simNow <= drainLimit:
    if all component queues are empty → break
    tick all components
    call onTick(SystemMetrics)
    simNow += tickSec

running_ = false → sim thread exits → server closes WebSocket
```

**Performance:** The engine runs far faster than real-time. 1000 requests at 100 r/s = 10 simulated seconds. Wall-clock time: ~10ms. This is why the WebSocket must be opened before `POST /start`.

**WebSocket server architecture:**

```
main (server_main.cpp)
├── WSAStartup()              WinSock2 init
├── socket() + bind() + listen()  TCP listener on port 8765
├── Broadcaster thread (detached) Runs every 100ms, drains BroadcastBuffer → sends to all WS clients
├── Accept loop               select() with 50ms timeout, spawns detached thread per connection
│   └── handleClient()
│       ├── POST /start       → handleStart() → parse JSON → create engine → start sim thread
│       ├── POST /stop        → handleStop() → signal engine to stop
│       ├── GET /health       → returns {"ok":true}
│       └── GET /ws           → WebSocket upgrade → handleWs() per-client thread
│           └── doHandshake()  SHA-1 + Base64 accept key, set SO_RCVTIMEO = 100ms
│           └── send()/recv()  RFC 6455 frame encode/decode
└── stopSimulation()          Join sim thread, reset engine
```

**Thread model:**

| Thread | Responsibility |
|---|---|
| **Accept loop (main)** | Accepts new TCP connections, spawns handler threads |
| **Per-client handler (detached)** | Complete HTTP request / WebSocket handshake, recv loop |
| **Simulation thread (detached)** | Runs the two-phase event loop, calls onTick callback |
| **Broadcaster (detached)** | Sleeps 100ms, drains BroadcastBuffer, sends to all WS clients |

### WebSocket Implementation Details (from scratch)

- **SHA-1** (RFC 3174): ~60 lines of bitwise arithmetic — message padding, 80-round compression, 5× 32-bit state words
- **Base64**: ~15 lines — 3-byte to 4-character encoding with `=` padding
- **Frame encoder (server→client)**: FIN + opcode 0x1 (text), length encoding (7-bit, 16-bit, or 64-bit extended), no mask
- **Frame decoder (client→server)**: Reads opcode, mask bit, payload length, XOR-unmask with 4-byte key per RFC 6455
- **SO_RCVTIMEO = 100ms**: Set once at handshake — the socket stays in blocking mode with a 100ms receive timeout, avoiding the `FIONBIO` toggle race condition

### JSON Data Contract

**Frontend → Backend (`POST /start`):**

```json
{
  "nodes": [
    { "id": 1, "type": "client", "name": "Client", "x": 100, "y": 200,
      "config": { "cpuCores": 1, "procTime": 0, "maxQueue": 99999, "instances": 1,
                  "requestRate": 100, "totalRequests": 1000 } },
    { "id": 2, "type": "loadbalancer", "name": "LB-1", "x": 300, "y": 200,
      "config": { "cpuCores": 4, "procTime": 2, "maxQueue": 500, "instances": 4 } },
    { "id": 3, "type": "server", "name": "App Server", "x": 500, "y": 200,
      "config": { "cpuCores": 4, "procTime": 50, "maxQueue": 200, "instances": 4 } }
  ],
  "edges": [
    { "id": 1, "fromId": 1, "toId": 2 },
    { "id": 2, "fromId": 2, "toId": 3 }
  ],
  "simulation": {
    "durationSec": 600, "tickSec": 0.01,
    "requestRatePerSec": 100, "totalRequests": 1000, "seed": 42
  }
}
```

Node types: `server`, `database`, `loadbalancer`, `redis`, `queue`, `client`

**Backend → Frontend (WebSocket, every 100ms):**

```json
{
  "type": "metrics",
  "simTimeSec": 12.4,
  "totalRequests": 1240, "completed": 1180, "failed": 0,
  "dropped": 60, "inFlight": 34,
  "avgLatencyMs": 52.0, "p99LatencyMs": 0.0, "throughputPerSec": 95.2,
  "components": [
    { "id": 1, "name": "Client", "type": "client",
      "cpuUsagePct": 100, "queueDepth": 0, "maxQueue": 99999,
      "requestsReceived": 1240, "requestsCompleted": 1240, "requestsDropped": 0,
      "avgProcessingMs": 0, "avgQueueWaitMs": 0, "throughputPerSec": 100 },
    { "id": 3, "name": "App Server", "type": "server",
      "cpuUsagePct": 100, "queueDepth": 200, "maxQueue": 200,
      "requestsReceived": 1240, "requestsCompleted": 1180, "requestsDropped": 60,
      "avgProcessingMs": 50, "avgQueueWaitMs": 120, "throughputPerSec": 95.2 }
  ]
}
```

### Simulation Mechanics

#### Poisson Request Generation

Each tick (10ms simulated), the expected number of new requests is:

```
expected = requestRatePerSec × tickSec
count = floor(expected)
if rand() < fractional_part(expected): count += 1
```

This gives Poisson-distributed inter-arrival times at the configured average rate — realistic traffic patterns.

#### Component Tick Cycle (per component, per tick)

1. **Complete** active slots whose `finishAt ≤ simNow` — finished requests are forwarded
2. **Promote** requests from `waitQueue` into free slots (up to `instances` parallel lanes)
3. **Process** each newly promoted request via `processRequest()` (component-specific)
4. **Update** CPU usage = `busySlots / instances × 100`
5. **Forward** completed requests via `forwardRequest()` to downstream component(s)

#### Drops

A request is **Dropped** when `receiveRequest()` is called on a component whose `waitQueue.size() >= maxQueue`. The drop is counted immediately and the request is never processed.

**Avoiding drops:** `requestRate ≤ capacity`. Capacity = `Σ (instances_i / (procTimeMs_i / 1000))` across all server components.

#### Why the Engine Runs Faster than Real-Time

The simulation clock advances by `tickSec = 0.01` (10ms simulated) per tight CPU loop iteration. There is no `sleep()` inside the loop. 1000 requests at 100 r/s = 10 simulated seconds ≈ 10ms wall-clock time. The broadcaster thread runs on a real 100ms timer and sends whatever is in the BroadcastBuffer each cycle.

### Design Patterns Used

**Observer Pattern (primary):** `useStore` (Zustand) is the subject. Components subscribe to slices:

```typescript
const m = useStore(s => s.latestMetrics)          // BottomBar
const applyMetrics = useStore(s => s.applyMetrics) // useWebSocket
```

Every 100ms `applyMetrics(msg)` fires, updating `nodes[].data.metrics` in a single Zustand dispatch. Only components subscribed to changed slices re-render (`React.memo` + Zustand selector equality).

**Strategy Pattern:** `COMP_META` in `useStore.ts` maps each `ComponentType` to its default config and visual identity. In the C++ backend, `makeComponent()` in `json_parser.hpp` selects the concrete class. The `Component` base class defines the strategy interface (`processRequest`, `forwardRequest`); each subclass overrides the parts that differ:

- `LoadBalancer::forwardRequest` — implements RoundRobin / LeastConnections routing strategy
- `RedisCache::processRequest` + `forwardRequest` — probabilistic hit/miss strategy
- `Database::processRequest` — read vs. write latency strategy

---

## 5. Modules Used

### Frontend Modules (React/TypeScript)

| Module | Lines | Responsibility |
|---|---|---|
| `App.tsx` | 108 | Layout shell. Critical `prevNodesRef` sync fix for React Flow state synchronization. |
| `store/useStore.ts` | 137 | Zustand store: all app state. `COMP_META` maps ComponentType → default config + color + icon. |
| `hooks/useWebSocket.ts` | 71 | WebSocket lifecycle: `openWebSocketFirst()` ensures WS open before POST /start. Message parsing and metrics dispatch. |
| `hooks/useSimulation.ts` | 73 | Start/stop orchestration: builds architecture payload, opens WS first, then POSTs /start. |
| `api/client.ts` | 24 | `fetch` wrappers: `startSimulation()`, `stopSimulation()`, `healthCheck()`. |
| `types/architecture.ts` | 40 | TypeScript types: ComponentType, NodeConfig, ArchNode, ArchEdge, SimulationConfig, ArchitecturePayload. |
| `types/metrics.ts` | 28 | TypeScript types: ComponentMetrics, SystemMetricsMsg (WebSocket message schema). |
| `nodes/BaseNode.tsx` | 289 | 6 SVG shape components, CPU ring, queue bar, metrics display. `React.memo` for performance. |
| `nodes/index.tsx` | 22 | `ArchNode` wrapper: adds React Flow Handles (target/source) to BaseNode. |
| `edges/AnimatedEdge.tsx` | 44 | Custom edge: dashed line with `animateMotion` packet dot. |
| `components/Sidebar.tsx` | 161 | Draggable palette: 3 sections, 6 component cards with SVG shapes. |
| `components/Toolbar.tsx` | 203 | Top bar: Run/Stop, Sim Settings, Clear, Export JSON, status pill. |
| `components/PropertiesPanel.tsx` | 132 | Config sliders + live metric bars with heat colors. |
| `components/BottomBar.tsx` | 74 | System metrics strip + drop-rate warning banner. |

### Backend Modules (C++17)

| Module | Lines | Responsibility |
|---|---|---|
| `src/simulation_engine.cpp` | 188 | Two-phase event loop: Phase 1 (inject), Phase 2 (drain). `getSystemMetrics()`, `generateRequests()` (Poisson). |
| `include/simulation_engine.hpp` | 106 | `SimulationEngine` class: `EventQueue`, `SimulationConfig`, callbacks, atomic `running_`. |
| `src/component.cpp` | 165 | Base `Component`: tick(complete→promote→process→CPU→forward), receiveRequest (drop on full), getMetrics. |
| `include/component.hpp` | 97 | Base class: queue, active slots, stats, throughput rolling window. |
| `include/components/components.hpp` | 185 | 6 subclasses: Client, Server, Database, LoadBalancer, RedisCache, MessageQueue. |
| `src/ws_server.cpp` | 266 | RFC 6455 WebSocket: SHA-1, Base64, frame encode/decode, SO_RCVTIMEO, Ping/Pong/Close. |
| `include/ws_server.hpp` | 49 | `WsClient` class declaration. |
| `src/http_server.cpp` | 125 | Raw HTTP/1.1: request parsing, response writing, CORS headers. |
| `include/http_server.hpp` | 69 | `HttpRequest`/`HttpResponse` structs, socket helpers, platform abstraction. |
| `src/server_main.cpp` | 304 | API server: accept loop, per-client threads, POST /start, POST /stop, GET /health, WebSocket /ws, Broadcaster thread. |
| `src/main.cpp` | 134 | CLI tool: loads JSON or runs built-in demo, prints live metrics, final report. |
| `include/request.hpp` | 73 | `Request` object: ID, timing, 7-state enum, RouteHop vector, accumulated latency. |
| `include/metrics.hpp` | 51 | `ComponentMetrics` + `SystemMetrics` structs. |
| `include/broadcast_buffer.hpp` | 34 | Thread-safe single-slot buffer: put() overwrites, take() drains. |
| `include/json_parser.hpp` | 79 | `fillFromJson()` builds engine from JSON. `makeComponent()` factory. |
| `third_party/nlohmann/json.hpp` | 209 | Custom minimal JSON stub using `std::map` for GCC 6.x compatibility. |
| `CMakeLists.txt` | 39 | Two targets: `archisys_sim` (CLI) + `archisys_server` (API). MSVC + MinGW support. |

---

## 6. Conclusion

ArchiSys is a **fully functional, production-grade distributed system design simulator** that demonstrates sophisticated engineering across two language ecosystems.

**Key achievements:**
1. **From-scratch WebSocket stack** — SHA-1, Base64, RFC 6455 frame codec implemented entirely in C++ with zero external libraries
2. **Two-phase simulation loop** — properly drains in-flight requests, preventing artificial request loss
3. **Solved timing race** — WebSocket opens before POST /start, ensuring metrics reach the browser
4. **Solved React Flow sync** — `prevNodesRef` pattern propagates live metrics to the canvas
5. **Clean build** — compiles with GCC 16.1.0, passes `tsc --noEmit`, produces clean production bundle

The system demonstrates deep understanding of both frontend (React, state synchronization, real-time UI) and backend (systems programming, networking protocols, concurrent simulation) engineering — bridging the gap between educational tools and production-quality software.

---

## 7. Future Works

### Short Term

| Priority | Task | Why |
|---|---|---|
| High | Fix Client node CPU display | Client shows CPU 100% always — it's a traffic generator with `processingMs=0` and `instances=1` |
| High | Add p99 latency calculation | `p99LatencyMs` is always `0.0` — engine doesn't maintain a sorted latency sample |
| Medium | Per-component latency history sparkline | In PropertiesPanel, show mini chart of latency/throughput/CPU over time |
| Medium | Simulation speed multiplier | Add `sleep_for` to slow down for better animation visibility |

### Medium Term

| Priority | Task | Why |
|---|---|---|
| High | Save/load architectures in localStorage | Currently only export JSON works |
| Medium | Additional component types: API Gateway, CDN, Kafka, RabbitMQ, Firewall | Expand simulation vocabulary |
| Medium | WebSocket reconnect with exponential backoff | Handle connection drops |
| Medium | Post-simulation summary report | Final metrics table, drop analysis, bottleneck identification |

### Long Term

| Priority | Task | Why |
|---|---|---|
| High | Replace custom JSON stub with real nlohmann/json | Improve robustness |
| High | Cross-platform networking (Asio/libuv) | Linux/macOS support |
| Medium | Multiple concurrent simulation runs | Separate engine instances per session |
| Low | Export simulation as video/GIF | Record runs for presentations |

---

## 8. Bugs Fixed During Development

| Bug | Root Cause | Fix |
|---|---|---|
| **B1: Sim runs forever** | Stop injection but loop exits immediately, leaving in-flight requests unfinished | Added drain phase (Phase 2) — ticks until queues empty |
| **B2: Node cards show `—`** | `setRfNodes` only on `nodes.length` change — metrics don't change array length | `prevNodesRef` comparison fires on any node data change |
| **B3: Cards stay red after stop** | `resetMetrics()` didn't clear `metrics` off individual node objects | Now maps over nodes, sets `metrics: undefined` |
| **B4: WS timing race** | Engine finishes before browser socket opens (faster than real-time) | `openWebSocketFirst()` before `POST /start` |
| **B5: WS send/recv race** | `ioctlsocket(FIONBIO)` toggle races with concurrent `send()` on Windows | `SO_RCVTIMEO = 100ms` set once at handshake |
| **B6: Sidebar default export** | `TYPE_SHAPES` Record incomplete when 'client' added | Added client SVG icon |

---

## 9. Build & Run

### Prerequisites

| Tool | Version | Install |
|---|---|---|
| Node.js | 18+ | https://nodejs.org |
| CMake | 3.16+ | https://cmake.org |
| MinGW-w64 GCC | 16+ | https://winlibs.com |

### Build & Run

```powershell
# Terminal 1 — C++ backend
cd backend
cmake -B build -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER="C:/mingw64/bin/g++.exe" -S .
cmake --build build --target archisys_server
.\build\archisys_server.exe    # server starts on localhost:8765

# Terminal 2 — React frontend
cd frontend
npm install
npm run dev                     # opens http://localhost:5173
```

### CLI Mode

```powershell
.\build\archisys_sim.exe        # built-in demo: LB → Server → Redis → DB
.\build\archisys_sim.exe ..\architecture.json  # load exported architecture
```

### Capacity Planning

```
Max safe request rate = instances × (1000 / procTimeMs) per server
```

| Config | Capacity |
|---|---|
| 1× Server, 2 instances, 50ms | 40 req/s |
| 2× Server, 4 instances, 50ms | 160 req/s |
| 1× Server, 4 instances, 20ms | 200 req/s |

---

## 10. Live Demo Scenario

1. **Drag** a **Client** → **Load Balancer** → two **App Servers** → **Redis Cache** → **Database**
2. **Connect** them: Client → LB → Servers → Redis → DB
3. **Select the Client** → set **Request Rate = 200 r/s**, **Total Requests = 1000**
4. **Click ▶ Run Simulation**
5. **Observe:** Client sends 200 req/s → LB distributes round-robin → Servers process (50ms each, 2 instances) → Redis serves 80% from cache → DB handles misses
6. **Watch:** CPU rings rise on servers, queue bars fill up, throughput stabilizes around 160 req/s (capacity-limited), drop banner appears if rate exceeds capacity
7. **Auto-stops** after 1000 requests drain through
8. **Export** as JSON, change configs, re-run to see how parameters affect behavior
