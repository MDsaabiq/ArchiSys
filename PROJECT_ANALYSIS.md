# ArchiSys — Project Analysis Report

## 1. Project Overview

**ArchiSys** is an interactive distributed systems simulator. Users visually design distributed system architectures on a drag-and-drop canvas (React Flow), connect components, and run real-time simulations of request traffic, queueing dynamics, CPU saturation, caching, and latency profiling.

The project is a **three-tier full-stack application**:

| Layer | Technology | Role |
|-------|-----------|------|
| Frontend | React 19 + React Flow + Zustand (TypeScript) | Visual canvas, live metrics display |
| Backend | FastAPI (Python 3.13) | HTTP/WebSocket orchestration, async sim loop |
| Engine | C++17 (pybind11 native extension) | Discrete-tick simulation compute core |

**Codebase size**: ~3,500 lines across C++ (1,422), Python (392), TypeScript (1,419), and tests (273).

**Git history**: 3 commits — early-stage project, actively under development.

---

## 2. Architecture Analysis

### 2.1 Communication Flow

```
React (Frontend)  ──HTTP POST /start, /stop──>  FastAPI (Python)  ──pybind11 in-process──>  C++ Engine
React (Frontend)  <──WebSocket @ 20 FPS──     FastAPI (Python)  <──metrics snapshot──     C++ Engine
```

The frontend POSTs an architecture JSON payload to `/start`, then listens on a WebSocket for streaming metrics at 20 updates/second. The Python layer drives the C++ engine tick-by-tick in an asyncio background task, broadcasting metrics snapshots each cycle.

### 2.2 Backend (Python / FastAPI)

**Key files:**
- `backend/api/main.py` (114 lines) — FastAPI app with 4 endpoints: `/health`, `/start`, `/stop`, `/metrics` + WebSocket `/ws`
- `backend/api/models.py` (87 lines) — Pydantic v2 schemas for request/response validation
- `backend/api/simulator.py` (129 lines) — `SimulationService` orchestrates async simulation loop, calls C++ `step()`, broadcasts metrics
- `backend/api/websocket_manager.py` (54 lines) — WebSocket connection manager with stale-client cleanup

**Strengths:**
- Clean separation: routing → service → engine
- Proper async lifecycle: `lifespan` context, `asyncio.create_task` for non-blocking sim loop, cancellation handling
- WebSocket-first design: opens WS before POST /start to avoid missing early metrics
- Pydantic v2 models with `model_dump_json()` for efficient serialization

**Observations:**
- CORS is wide open (`allow_origins=["*"]`) — fine for local dev, needs restriction for production
- The `/health` endpoint doesn't actually probe the C++ engine health (just returns static "ok")
- No request rate limiting or authentication

### 2.3 C++ Simulation Engine

**Key files:**
- `backend/engine/src/simulation_engine.cpp` (282 lines) — Central coordinator: request generation, component ticking, metrics aggregation
- `backend/engine/src/component.cpp` (234 lines) — Base component: FIFO queue, worker slots, CPU contention, forwarding
- `backend/engine/src/bindings.cpp` (109 lines) — pybind11 module exposing `Simulator`, `SystemMetrics`, `ComponentMetrics`
- `backend/engine/src/simulator.cpp` (63 lines) — Thread-safe facade with mutex
- `backend/engine/include/components.hpp` (199 lines) — 6 component subclasses

**Architecture:**
- `Simulator` (facade) → `SimulationEngine` (coordinator) → `Component` instances
- JSON parsed by `JsonParser` which builds the topology graph
- Thread-safe: `Simulator` holds a `std::mutex`, `SimulationEngine` holds its own `engineMutex_`

**Six simulated component types:**

| Type | Class | Default Behavior |
|------|-------|------------------|
| Client | `Client` | Traffic entry point, 0ms processing, injects requests via Poisson distribution |
| Server | `Server` | Multi-instance app server, configurable CPU/procTime/queue |
| Database | `Database` | Read/write split (writes 2x), connection pool limit |
| LoadBalancer | `LoadBalancer` | 3 algorithms: RoundRobin, LeastConnections, Random |
| Redis | `RedisCache` | 80% hit ratio, misses forward downstream to DB |
| Queue | `MessageQueue` | Async buffer, high capacity, configurable drain rate |

**Simulation tick lifecycle (4 phases):**
1. `generateRequests(dt)` — Poisson arrival, inject into entry point
2. `updateComponents(dt)` — Each component: complete expired slots → schedule from queue → update CPU/throughput → forward downstream
3. `getSystemMetrics()` — Aggregate: total/completed/dropped/in-flight, P99 latency via `nth_element`
4. `advanceSimulationTime(dt)` — `simNow_ += dt`

**Strengths:**
- Well-structured OOP hierarchy with virtual method overrides for component-specific behavior
- Deterministic with seed (`srand(config_.seed)`) — reproducible runs
- GIL released at pybind11 boundary (`py::call_guard<py::gil_scoped_release>()`) — non-blocking Python
- P99 calculated accurately via `std::nth_element` on sorted latency vector
- CPU contention model: when `busy > cpuCores`, processing scales up (capped at 2.5x)
- Full request lifecycle tracking: per-hop `RouteHop` with queue wait + processing time

**Observations:**
- `allRequests_` vector retains all request objects for the entire simulation — memory grows linearly with request count. For long-running simulations with high request rates, this could be significant
- `completedLatencies_` vector also grows unbounded — used for P99 calculation. For very large simulations (100k+ requests), this triggers a full sort each metrics query
- `rand()` is used for randomness, which has poor statistical quality. A Mersenne Twister (`std::mt19937`) would be more appropriate
- `checkDraining()` is declared in the header but not called in `step()` — appears to be dead code or planned for future use
- LoadBalancer's `Random` algorithm uses `rand() % outgoing_.size()` which introduces modulo bias
- No cycle detection in the topology graph — circular architectures could cause infinite request loops

### 2.4 Frontend (React / TypeScript)

**Key files:**
- `frontend/src/App.tsx` (108 lines) — React Flow canvas with drag-and-drop, Zustand↔ReactFlow sync
- `frontend/src/store/useStore.ts` (137 lines) — Zustand store: nodes, edges, metrics, settings
- `frontend/src/nodes/BaseNode.tsx` (289 lines) — Custom SVG node renderer with CPU rings, queue bars, metrics panels
- `frontend/src/hooks/useSimulation.ts` (73 lines) — Builds payload from canvas state, manages WS-before-POST ordering
- `frontend/src/hooks/useWebSocket.ts` (70 lines) — Singleton WS with reconnection logic

**Strengths:**
- Clean Zustand store with well-typed interfaces — single source of truth
- Custom SVG node rendering (not default React Flow boxes) — each component type gets its own architectural shape (server rack, database cylinder, load balancer diamond, redis hexagon)
- Smart metrics display: CPU ring (heat-colored), queue fill bar, live latency
- Drag-and-drop from sidebar to canvas
- Properties panel with sliders for all configurable parameters
- Bottom bar with system-wide metrics + drop warning banner
- Export to JSON feature

**Observations:**
- All styling is inline — no CSS modules or styled-components. Works but harder to maintain at scale
- React Flow's `rfNodes`/`rfEdges` are cast to `any` — type safety is relaxed at the React Flow boundary
- WebSocket URL is hardcoded to `ws://localhost:8000/ws`
- No error boundary or loading state for initial render
- The `useWebSocket` hook has a potential race: `globalWs` singleton could be closed by `onclose` while `useSimulation` still holds a reference

### 2.5 Tests

**3 test files, 273 lines total:**

| File | Tests | Coverage |
|------|-------|----------|
| `test_api.py` (63 lines) | 3 tests | Health check, start/stop, validation |
| `test_simulation_scenarios.py` (198 lines) | 4 tests | Client→Server, LB→2 Servers, Client→LB→Server→Redis→DB, Queue overflow |
| `test_websocket.py` (12 lines) | 1 test | Ping/pong |

**Strengths:**
- Deterministic test design with fixed seeds
- Scenario-based tests cover the key architecture patterns (linear, fan-out, multi-tier cache, overflow)
- API tests use FastAPI's `TestClient` (sync, no server needed)

**Gaps:**
- No test for edge cases: empty edges, circular topologies, zero-duration, concurrent start/stop
- No frontend tests (no Jest/Vitest setup)
- No C++ unit tests (tests go through pybind11 only)
- WebSocket test only checks ping/pong, not metrics streaming

---

## 3. Code Quality Assessment

### 3.1 Strengths

| Area | Rating | Notes |
|------|--------|-------|
| Architecture clarity | Excellent | Clean 3-tier separation, each layer has a single responsibility |
| Type safety | Good | Python uses Pydantic v2, TS uses proper interfaces, C++ uses strong types |
| Documentation | Good | 4 doc files (architecture, simulation, cpp-python-integration, API), inline comments |
| Test coverage | Moderate | Backend API + simulation scenarios well covered, frontend and C++ unit tests missing |
| Naming conventions | Excellent | Consistent naming across all three languages |
| Error handling | Good | HTTPException in API, runtime_error in C++, try/catch in sim loop |

### 3.2 Areas for Improvement

| Priority | Issue | Location | Impact |
|----------|-------|----------|--------|
| High | Memory: `allRequests_` + `completedLatencies_` grow unbounded | `simulation_engine.cpp` | OOM risk for long-running/high-rate simulations |
| High | `rand()` instead of `std::mt19937` | `simulation_engine.cpp`, `components.hpp` | Poor statistical quality, platform-dependent |
| Medium | No cycle detection in topology | `json_parser.hpp` | Infinite request loops possible |
| Medium | CORS `allow_origins=["*"]` | `main.py` | Security risk in production |
| Medium | Hardcoded URLs (localhost:8000, ws://localhost:8000) | `client.ts`, `useWebSocket.ts` | Non-configurable backend address |
| Medium | No frontend tests | `frontend/` | Regression risk on UI changes |
| Low | `checkDraining()` declared but unused | `simulation_engine.hpp` | Dead code |
| Low | `as any` casts in React Flow integration | `App.tsx` | Type safety gap |
| Low | No `.gitignore` in project root (only frontend) | root | Build artifacts may be committed |

---

## 4. Technology Stack Summary

| Category | Technology | Version |
|----------|-----------|---------|
| Frontend framework | React | 19.2.8 |
| Canvas library | React Flow | 11.11.4 |
| State management | Zustand | 5.0.14 |
| Build tool | Vite | 8.2.0 |
| Linter | oxlint | 1.75.0 |
| Backend framework | FastAPI | >=0.110.0 |
| ASGI server | uvicorn | >=0.28.0 |
| Validation | Pydantic | >=2.6.0 |
| Native binding | pybind11 | >=2.12.0 |
| C++ standard | C++17 | - |
| Build system | CMake | >=3.16 |
| JSON (C++) | nlohmann/json | vendored |
| Test framework | pytest | >=8.0.0 |
| HTTP client (tests) | httpx | >=0.27.0 |

---

## 5. Build & Run

### Prerequisites
- Python 3.13+, CMake 3.16+, a C++17 compiler (MSVC/MinGW), Node.js 22+

### Steps
1. Python venv + dependencies: `pip install -r backend/requirements.txt`
2. Build C++ extension: `cmake -B backend/engine/build -S backend/engine` + `cmake --build`
3. Copy `.pyd` to site-packages
4. Run tests: `pytest -v tests/`
5. Start backend: `uvicorn backend.api.main:app --port 8000 --reload`
6. Start frontend: `cd frontend && npm run dev` → http://localhost:5173

---

## 6. Future/Planned Features

- **GenAI module** (`backend/api/ai/`) — placeholder for natural language → architecture generation via LLM
- Architecture import (JSON export exists, import not yet implemented)
- Additional component types (CDN, API Gateway, Service Mesh)
- Simulation replay and comparison

---

## 7. Verdict

ArchiSys is a **well-architected, early-stage project** with a sophisticated three-tier design. The C++ simulation engine is the standout component — it models realistic distributed system behavior (queueing, CPU contention, caching, load balancing) with per-request hop tracking and P99 latency. The frontend provides an intuitive visual design experience with live metrics.

The main risks are memory management in long-running simulations and the absence of cycle detection. With ~3,500 lines of code and 3 git commits, this is clearly a project in active early development with strong foundations.
