# ArchiSys — Project Documentation

**Version:** 0.2.0  
**Started:** 2025  
**Status:** Frontend complete ✅ | C++ Backend complete ✅ | Build verified ✅ | End-to-end simulation working ✅

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Architecture](#2-architecture)
3. [What Was Built](#3-what-was-built)
4. [Frontend](#4-frontend)
5. [Backend](#5-backend)
6. [JSON Contract](#6-json-contract)
7. [Design Patterns](#7-design-patterns)
8. [Simulation Mechanics](#8-simulation-mechanics)
9. [Build & Run Guide](#9-build--run-guide)
10. [Bugs Fixed & How](#10-bugs-fixed--how)
11. [Current Status & Known Issues](#11-current-status--known-issues)
12. [Next Steps](#12-next-steps)

---

## 1. Project Overview

ArchiSys is an **interactive distributed system design simulator**. Users drag and connect virtual system components on a visual canvas, configure them, then run a simulation to watch requests flow through the architecture in real time with live metrics on every node.

**Educational and analytical — not a deployment tool.**  
No real servers are created. No real network traffic is generated. Everything is a virtual event-driven simulation.

### Core Idea

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

---

## 2. Architecture

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

---

## 3. What Was Built

### Phase 1 — HTML Prototype
Single standalone `mockups/archisys-canvas/index.html`:
- Vanilla JS drag-and-drop canvas, fake JS simulation (random fluctuations)
- Visual packet animation, properties panel, export JSON button
- **Purpose:** visual proof-of-concept before building the real engine

### Phase 2 — C++ Simulation Engine
Real OOP event-driven simulation:
- `Request` objects with full route history and latency accumulation
- Component hierarchy: Client, Server, Database, LoadBalancer, RedisCache, MessageQueue
- Priority-queue event loop, per-component queue management, parallel processing slots
- **Two-phase loop:** injection phase + drain phase (all in-flight finish before shutdown)

### Phase 3 — C++ WebSocket API Server
Hand-rolled HTTP + WebSocket server using raw WinSock2:
- `POST /start` — parses JSON, starts simulation thread
- `POST /stop` — signals engine to stop
- `GET /health` — health check
- `WebSocket /ws` — streams metrics to all clients every 100ms
- RFC 6455 compliant (SHA-1 handshake, frame encode/decode from scratch)
- `SO_RCVTIMEO` receive timeout (100ms) on WS sockets — avoids FIONBIO race with `send()`

### Phase 4 — React + React Flow Frontend
Production-grade React application:
- React Flow canvas (drag, connect, zoom, pan), Zustand global state
- 6 custom node types with live CPU%, queue depth, latency per card
- Animated dashed edges with packet dots (SVG `animateMotion`)
- Properties panel with component-specific config sliders + live metric bars
- Bottom metrics bar with drop-rate warning banner
- **Client node** as the entry point — controls request rate and total requests
- Simulation settings panel (when no Client node) in the toolbar
- TypeScript throughout, zero `tsc --noEmit` errors

---

## 4. Frontend

### Technology Stack

| Tool | Purpose |
|---|---|
| Vite + React + TypeScript | Project scaffold |
| React Flow | Canvas, nodes, edges, handles, zoom, pan |
| Zustand | Global state (nodes, edges, metrics, sim settings) |
| Inline CSS-in-JS | Styling — no Tailwind, no CSS modules |

### File Structure

```
frontend/src/
├── App.tsx                    Layout shell + Canvas (React Flow wrapper)
├── main.tsx                   React root
├── App.css                    Global styles, React Flow overrides (light theme)
├── index.css                  Root element — 100vw × 100vh, no centering
├── store/
│   └── useStore.ts            Zustand store — all app state + SimSettings
├── hooks/
│   ├── useWebSocket.ts        WS connection; openWebSocketFirst() exported for ordering
│   └── useSimulation.ts       start/stop — opens WS first, then POSTs /start
├── api/
│   └── client.ts              fetch wrappers for POST /start, POST /stop
├── types/
│   ├── metrics.ts             TypeScript types for WebSocket messages
│   └── architecture.ts        TypeScript types for JSON payload + ComponentType
├── nodes/
│   ├── index.tsx              nodeTypes map for React Flow
│   └── BaseNode.tsx           Shared node card (SVG icon, metrics rows, delete)
├── edges/
│   └── AnimatedEdge.tsx       Dashed indigo edge with animating packet dot
└── components/
    ├── Toolbar.tsx            Run/Stop/Clear/Export + Sim Settings dropdown
    ├── Sidebar.tsx            Draggable palette (6 types, SVG icons)
    ├── PropertiesPanel.tsx    Config sliders + live metric bars for selected node
    └── BottomBar.tsx          System-wide metrics strip + drop-rate warning banner
```

### Component Types

| Sidebar Label | Type String | Colour | Default Config |
|---|---|---|---|
| Client | `client` | Sky blue | requestRate: 100 r/s, totalRequests: 0 (unlimited) |
| App Server | `server` | Blue | 4 CPU cores, 50ms proc, 200 queue, 2 instances |
| Database | `database` | Purple | 1 core, 20ms proc, 50 queue, 1 instance |
| Load Balancer | `loadbalancer` | Green | 4 cores, 2ms proc, 500 queue, 4 instances |
| Redis Cache | `redis` | Amber | 2 cores, 1ms proc, 1000 queue, 4 instances |
| Msg Queue | `queue` | Red | 1 core, 5ms proc, 10000 queue, 1 instance |

### State Flow

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

**Key fix:** `App.tsx` uses `prevNodesRef` to detect any change in Zustand `nodes` (not just length) and calls `setRfNodes` — this ensures every 100ms metrics update reaches the React Flow canvas.

### Simulation Settings (Toolbar)

When **no Client node** is on the canvas, a **⚙ Sim Settings** button appears in the toolbar. It opens a panel exposing:

| Field | Default | Meaning |
|---|---|---|
| Request Rate | 100 | req/s injected into the entry point |
| Total Requests | 1000 | Stop after N requests (0 = run until Stop) |
| Random Seed | 42 | Reproducible Poisson distribution |

When a **Client node** is present, its Properties panel controls these values instead, and the toolbar shows an info banner pointing the user there.

---

## 5. Backend

### Technology Stack

| Technology | Choice |
|---|---|
| Language | C++17 |
| HTTP/WebSocket | Raw WinSock2 — zero external libraries |
| JSON | Custom minimal stub (compatible with GCC 6.x → 16.x) |
| Build system | CMake 3.16+ |
| Compiler | MinGW-w64 GCC 16+ (POSIX threads, UCRT) from winlibs.com |

### File Structure

```
backend/
├── CMakeLists.txt                  Two targets: archisys_sim + archisys_server
├── include/
│   ├── request.hpp                 Request object, RouteHop, RequestStatus enum
│   ├── component.hpp               Base Component — queue, slots, tick, forward
│   ├── simulation_engine.hpp       Engine — two-phase loop, SimulationConfig
│   ├── metrics.hpp                 ComponentMetrics + SystemMetrics structs
│   ├── broadcast_buffer.hpp        Thread-safe single-slot sim→WS buffer
│   ├── http_server.hpp             HttpRequest / HttpResponse types
│   ├── ws_server.hpp               WsClient — handshake, frame codec, SO_RCVTIMEO
│   ├── json_parser.hpp             fillFromJson() — builds engine from JSON
│   └── components/
│       └── components.hpp          Client, Server, Database, LoadBalancer,
│                                   RedisCache, MessageQueue
├── src/
│   ├── component.cpp               Base tick/queue/slot logic
│   ├── simulation_engine.cpp       Two-phase event loop, request generation
│   ├── http_server.cpp             HTTP parse + write + CORS headers
│   ├── ws_server.cpp               SHA-1, Base64, frame encode/decode, SO_RCVTIMEO
│   ├── server_main.cpp             WinSock2 accept loop + broadcaster thread
│   └── main.cpp                    CLI tool (runs sim without HTTP server)
└── third_party/
    └── nlohmann/
        └── json.hpp                Custom minimal JSON stub (no internet required)
```

### Component Hierarchy

```
Component (base)
├── Client          — entry point, zero processing delay, injects requests
├── Server          — multi-instance, CPU usage, queue, configurable latency
├── Database        — single-threaded, read (20ms) / write (40ms, every 5th req)
├── LoadBalancer    — RoundRobin / Random / LeastConnections routing
├── RedisCache      — 80% probabilistic hit (1ms) / miss (5ms + forward)
└── MessageQueue    — async FIFO broker, 5ms enqueue latency
```

### Request Object

Every simulated request carries:

| Field | Type | Description |
|---|---|---|
| `id` | uint64_t | Unique monotonic ID |
| `createdAt` | double | Simulation seconds when spawned |
| `completedAt` | double | Simulation seconds when finished |
| `status` | enum | Created → InQueue → Processing → Forwarded → Completed/Dropped/Failed |
| `totalLatencyMs` | double | Accumulated processing delays across all components |
| `totalQueueWaitMs` | double | Total time spent waiting in component queues |
| `route` | vector\<RouteHop\> | One hop per component visited (arrivalTime, departureTime, processingTime) |

### Simulation Engine — Two-Phase Loop

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

### Capacity Formula

To avoid drops, ensure:

```
requestRate  ≤  sum over all servers of (instances / (procTimeMs / 1000))
```

Example: 2 × App Server, 4 instances each, 50ms proc:
```
capacity = 2 × (4 / 0.050) = 160 req/s
```
Setting `requestRate > 160` will cause queue overflow and drops.

### WebSocket Implementation

Written entirely from scratch:
- SHA-1 in ~60 lines of bitwise arithmetic (RFC 3174)
- Base64 encoder in ~15 lines
- RFC 6455 frame encoder (server→client, unmasked text frames)
- RFC 6455 frame decoder (client→server, masked per spec)
- `SO_RCVTIMEO = 100ms` set at handshake — recv loop wakes up every 100ms without toggling `FIONBIO` (which races with concurrent `send()` calls on Windows)
- Handles Ping/Pong and Close frames

### BroadcastBuffer

Thread-safe single-slot buffer between simulation thread and broadcaster thread:
- `put(json)` — producer overwrites with latest (old frame discarded if not consumed)
- `take(out)` — consumer drains exactly one frame per 100ms broadcast cycle
- Broadcaster thread iterates `gClients`, calls `ws->send(msg)`, removes closed sockets

---

## 6. JSON Contract

### Frontend → Backend: `POST /start`

```json
{
  "nodes": [
    {
      "id": 1,
      "type": "client",
      "name": "Client",
      "x": 100, "y": 200,
      "config": {
        "cpuCores": 1,
        "procTime": 0,
        "maxQueue": 99999,
        "instances": 1,
        "requestRate": 100,
        "totalRequests": 1000
      }
    },
    {
      "id": 2,
      "type": "loadbalancer",
      "name": "LB-1",
      "x": 300, "y": 200,
      "config": { "cpuCores": 4, "procTime": 2, "maxQueue": 500, "instances": 4 }
    },
    {
      "id": 3,
      "type": "server",
      "name": "App Server",
      "x": 500, "y": 200,
      "config": { "cpuCores": 4, "procTime": 50, "maxQueue": 200, "instances": 4 }
    }
  ],
  "edges": [
    { "id": 1, "fromId": 1, "toId": 2 },
    { "id": 2, "fromId": 2, "toId": 3 }
  ],
  "simulation": {
    "durationSec": 600,
    "tickSec": 0.01,
    "requestRatePerSec": 100,
    "totalRequests": 1000,
    "seed": 42
  }
}
```

**Field meanings:**

| Field | Meaning |
|---|---|
| `type` | Must match C++ type string exactly — `client`, `server`, `database`, `loadbalancer`, `redis`, `queue` |
| `procTime` | Base processing time per request in milliseconds |
| `maxQueue` | Max requests that can wait before dropping |
| `instances` | Parallel processing lanes (higher = more throughput) |
| `requestRate` | (Client only) req/s injected — overrides `simulation.requestRatePerSec` |
| `totalRequests` | (Client only) stop after N requests; 0 = run until Stop |
| `durationSec` | Hard upper bound on simulation time (safety limit) |
| `seed` | Poisson distribution seed for reproducible results |

### Backend → Frontend: WebSocket message (every 100ms)

```json
{
  "type": "metrics",
  "simTimeSec": 12.4,
  "totalRequests": 1240,
  "completed": 1180,
  "failed": 0,
  "dropped": 60,
  "inFlight": 34,
  "avgLatencyMs": 52.0,
  "p99LatencyMs": 0.0,
  "throughputPerSec": 95.2,
  "components": [
    {
      "id": 1,
      "name": "Client",
      "type": "client",
      "cpuUsagePct": 100,
      "queueDepth": 0,
      "maxQueue": 99999,
      "requestsReceived": 1240,
      "requestsCompleted": 1240,
      "requestsDropped": 0,
      "avgProcessingMs": 0,
      "avgQueueWaitMs": 0,
      "throughputPerSec": 100
    },
    {
      "id": 3,
      "name": "App Server",
      "type": "server",
      "cpuUsagePct": 100,
      "queueDepth": 200,
      "maxQueue": 200,
      "requestsReceived": 1240,
      "requestsCompleted": 1180,
      "requestsDropped": 60,
      "avgProcessingMs": 50,
      "avgQueueWaitMs": 120,
      "throughputPerSec": 95.2
    }
  ]
}
```

---

## 7. Design Patterns

### Observer Pattern (primary)

`useStore` (Zustand) is the **subject**. React components **subscribe** to slices:

```typescript
const m = useStore(s => s.latestMetrics)          // BottomBar
const applyMetrics = useStore(s => s.applyMetrics) // useWebSocket
```

Every 100ms `applyMetrics(msg)` fires, updating `nodes[].data.metrics` in a single Zustand dispatch. Only components subscribed to changed slices re-render (React.memo + Zustand selector equality).

### Strategy Pattern

`COMP_META` in `useStore.ts` maps each `ComponentType` to its default config and visual identity. On the C++ side, `makeComponent()` in `json_parser.hpp` selects the concrete class. The `Component` base class defines the strategy interface (`processRequest`, `forwardRequest`); each subclass overrides the parts that differ:

- `LoadBalancer::forwardRequest` — implements RoundRobin / LeastConnections routing strategy
- `RedisCache::processRequest` + `forwardRequest` — probabilistic hit/miss strategy
- `Database::processRequest` — read vs. write latency strategy

---

## 8. Simulation Mechanics

### Poisson Request Generation

Each tick, the expected number of new requests is:
```
expected = requestRatePerSec × tickSec
count = floor(expected)
if rand() < fractional_part(expected): count += 1
```
This gives Poisson-distributed inter-arrival times at the configured average rate.

### Component Tick

Each tick per component:
1. **Complete** active slots whose `finishAt ≤ simNow`
2. **Promote** requests from `waitQueue` into free slots (up to `instances`)
3. Call `processRequest()` on each newly promoted request (component-specific logic)
4. **Update** CPU usage = `busySlots / instances × 100`
5. **Forward** completed requests via `forwardRequest()` to downstream component(s)

### Drops

A request is **Dropped** when `receiveRequest()` is called on a component whose `waitQueue.size() >= maxQueue`. The drop is counted immediately and the request is never processed.

**Avoiding drops:** `requestRate ≤ capacity`. Capacity = `Σ (instances_i / (procTimeMs_i / 1000))` across all server components in the bottleneck path.

### Why the Engine Runs Faster than Real-Time

The simulation clock advances by `tickSec = 0.01` (10ms simulated) per tight CPU loop iteration. There is no `sleep()` inside the loop. 1000 requests at 100 r/s = 10 simulated seconds ≈ 10ms wall-clock time. The broadcaster thread runs on a real 100ms timer and sends whatever is in the BroadcastBuffer each cycle — so the frontend may receive all frames in a burst.

---

## 9. Build & Run Guide

### Prerequisites

| Tool | Required | Source |
|---|---|---|
| Node.js | 18+ | https://nodejs.org |
| CMake | 3.16+ | https://cmake.org |
| MinGW-w64 GCC | 14+ POSIX threads UCRT | https://winlibs.com |

### Install MinGW-w64

1. Go to **https://winlibs.com**
2. Download **Win64 — GCC 16.1.0 + MinGW-w64 14.0.0 (UCRT) — without LLVM**
3. Extract to `C:\mingw64`
4. Verify `C:\mingw64\bin\g++.exe` and `C:\mingw64\bin\ld.exe` both exist
5. Add to PATH (PowerShell as Administrator):

```powershell
[Environment]::SetEnvironmentVariable(
  "PATH",
  "C:\mingw64\bin;" + [Environment]::GetEnvironmentVariable("PATH","Machine"),
  "Machine"
)
```

6. **Close and reopen PowerShell**, then verify:

```powershell
g++ --version    # GCC 16.x
ld --version     # must respond
```

### Build C++ Backend

```powershell
cd backend
Remove-Item -Recurse -Force build   # wipe stale cache
cmake -B build -G "MinGW Makefiles" `
  -DCMAKE_CXX_COMPILER="C:/mingw64/bin/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="C:/mingw64/bin/mingw32-make.exe" `
  -S .
cmake --build build --target archisys_server
```

Expected: `backend\build\archisys_server.exe` (≈ 293 KB).

### Run

**Terminal 1 — C++ server:**
```powershell
.\backend\build\archisys_server.exe
# [ArchiSys] Listening on http://localhost:8765
```

**Terminal 2 — React frontend:**
```powershell
cd frontend
npm install      # first time only
npm run dev
# → http://localhost:5173
```

### Usage

1. Open **http://localhost:5173**
2. Drag a **Client** from the sidebar onto the canvas
3. Drag a **Load Balancer** and one or more **App Servers**
4. Connect: Client → Load Balancer → App Server(s)
5. Select the Client node → set **Request Rate** and **Total Requests** in the right panel
6. Click **▶ Run Simulation**
7. Watch live metrics update on each node card and the bottom bar
8. Simulation auto-stops after Total Requests are processed and drained
9. Click **■ Stop** at any time for manual stop

### Capacity Planning (avoid drops)

```
Max safe request rate = instances × (1000 / procTimeMs)  per server
```

| Config | Capacity |
|---|---|
| 1× Server, 2 instances, 50ms | 40 req/s |
| 2× Server, 4 instances, 50ms | 160 req/s |
| 1× Server, 4 instances, 20ms | 200 req/s |

Set `requestRate` ≤ total capacity or drops will appear (shown as red banner).

---

## 10. Bugs Fixed & How

### B1 — Simulation ran forever when Total Requests was set

**Cause:** The engine loop condition `totalRequests_ < config_.totalRequests` stopped injecting new requests but exited the loop immediately, leaving ~400 in-flight requests unfinished.

**Fix:** Added a **drain phase** after injection ends. The engine continues ticking until all component queues are empty (or 10 extra simulated seconds as safety), then exits. All in-flight requests complete before the WebSocket closes.

---

### B2 — Node cards showed `—` during simulation (never updated)

**Cause:** `App.tsx` synced `rfNodes` (React Flow's local state) only when `nodes.length` changed: `useEffect(() => setRfNodes(nodes), [nodes.length])`. Metrics updates change `node.data.metrics` but not the node count, so React Flow's canvas never received live data.

**Fix:** Replaced with a `prevNodesRef` comparison — on every render, if `nodes !== prevNodesRef.current`, push to `setRfNodes`. This fires on every Zustand update regardless of array length.

---

### B3 — Node cards stayed red after simulation stopped

**Cause:** `resetMetrics()` cleared `latestMetrics` and the metrics map but not `metrics` off individual node objects. Cards kept showing the last live values indefinitely.

**Fix:** `resetMetrics()` now also maps over `nodes` and sets `metrics: undefined` on every node, resetting all cards to `—`.

---

### B4 — Metrics received but WebSocket timing race (nothing shown)

**Cause:** The C++ engine runs microseconds of wall-clock time per simulated second. If the browser opened the WebSocket *after* `POST /start` (the previous order), the engine could complete before the browser socket was established — all metrics frames were sent to nobody.

**Fix:** `useSimulation.ts` now calls `openWebSocketFirst()` — which returns a Promise resolving on `ws.onopen` — **before** calling `POST /start`. The WebSocket is guaranteed open before the first engine tick.

---

### B5 — WebSocket send/recv race on Windows (FIONBIO toggle)

**Cause:** `ws_server.cpp` was toggling the socket between non-blocking and blocking mode using `ioctlsocket(FIONBIO)` on every `recv()` call. The broadcaster thread was concurrently calling `send()` on the same socket. On Windows, toggling `FIONBIO` while another thread holds the socket causes undefined behavior — the socket silently errored, closing before any metrics were delivered.

**Fix:** Set `SO_RCVTIMEO = 100ms` once in `doHandshake()`. The socket stays blocking and times out after 100ms per recv attempt — same non-blocking behavior without any mode toggling.

---

### B6 — `Sidebar.tsx` had no `default` export (Vite module error)

**Cause:** `TYPE_SHAPES` was typed as `Record<ComponentType, React.ReactNode>` requiring all keys. When `'client'` was added to `ComponentType`, the record was incomplete — TypeScript's strict check caused Vite's esbuild transform to fail producing no exports at all.

**Fix:** Added the `client` SVG icon as the missing key in `TYPE_SHAPES`.

---

### B7 — Old build errors (from initial MinGW setup)

| Error | Cause | Fix |
|---|---|---|
| `CMakeLists.txt not found` | Running cmake from repo root | `cd backend` first |
| `CMAKE_CXX_COMPILER not set` | No compiler in PATH | Install MinGW-w64 |
| Generator mismatch | Stale CMakeCache.txt | `Remove-Item -Recurse -Force build` |
| Structured bindings (GCC 6.3) | `auto& [k,v]` unsupported | Replaced with `kv.second` |
| `INT_MAX` not declared | Missing `<climits>` | Added include |
| `std::thread` not found | `WIN32_LEAN_AND_MEAN` order | Moved stdlib includes before WinSock2 |
| `unordered_map<string,json>` incomplete | GCC 6.x template issue | Switched to `std::map` in JSON stub |
| `operator[]` ambiguous | `const char*` vs `string` | Added explicit `operator[](const char*)` |
| `SimulationEngine` move deleted | `atomic<bool>` member | Changed to `fillFromJson(engine&)` |
| `uint64_t` assignment ambiguous | JSON stub overloads | Explicit `(int64_t)` cast |
| `cannot find 'ld'` | Broken MinGW at `C:\mingw32` | Fresh Win64 package from winlibs.com |
| UTF-16 source files | PowerShell `>` redirection | `[System.IO.File]::WriteAllText(..., UTF8)` |
| Chocolatey lock file | Stale lock | Direct download from winlibs.com |

---

## 11. Current Status & Known Issues

### ✅ Complete and working

- HTML prototype — open `mockups/archisys-canvas/index.html` directly in Chrome
- C++ backend — builds cleanly with GCC 16.1.0 on MinGW-w64
- C++ server — starts, accepts connections, streams WebSocket metrics
- Simulation engine — two-phase loop, all requests drain before shutdown
- React frontend — `npm run dev` works, TypeScript clean (`tsc --noEmit`)
- Production bundle — `npm run build` produces a clean bundle
- End-to-end simulation — Client → LB → Servers with live node metrics
- Drop detection — red warning banner when request rate > capacity
- Auto-stop — simulation ends naturally when Total Requests limit is reached

### ⚠️ Known Limitations

- `p99LatencyMs` is always `0.0` — the engine does not maintain a sorted latency sample
- Simulation runs much faster than real-time — 1000 reqs complete in ~10ms wall-clock time
- No authentication on the API server — localhost only
- No persistent storage — export JSON to save architecture between sessions
- Client node shows CPU 100% always — it has `instances=1` and `processingMs=0` so it's always "busy" in the slot model; cosmetically misleading

---

## 12. Next Steps

### Short term

- Fix Client node CPU display — Client should not report CPU usage (it's a traffic generator, not a processor)
- Add p99 latency calculation — maintain a sorted reservoir sample in the engine
- Per-component latency history graph in Properties panel (sparkline over time)
- Simulation speed multiplier — slow down the engine loop with `sleep_for` to make animations more visible

### Medium term

- Save/load architectures in browser localStorage
- More component types: **API Gateway**, **CDN**, **Kafka**, **RabbitMQ**, **Firewall**
- WebSocket reconnect with exponential backoff
- Post-simulation summary report — final metrics table, drop analysis, bottleneck identification

### Long term

- Replace custom JSON stub with real nlohmann/json (vendor into repo)
- Replace raw WinSock2 with a cross-platform async library (Asio / libuv)
- Linux/macOS build targets in CMakeLists.txt
- Multiple concurrent simulation runs (separate engine instances per session)
