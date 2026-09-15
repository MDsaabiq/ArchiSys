# ArchiSys — Comprehensive Technical & Code-Level Documentation

**Version:** 2.0.0  
**Architecture:** React (TypeScript) ➔ FastAPI (Python) ➔ pybind11 (In-Process Native) ➔ C++ Simulation Engine  
**Status:** Unified Backend ✅ | In-Process C++ Engine ✅ | Realistic Discrete Simulation ✅ | Tests Passing (8/8) ✅

---

## Table of Contents

1. [Executive Summary & Architecture Rationale](#1-executive-summary--architecture-rationale)
2. [Unified Codebase Directory Layout](#2-unified-codebase-directory-layout)
3. [Deep Code-Level Simulation Engine Logic](#3-deep-code-level-simulation-engine-logic)
   - [3.1 Discrete Simulation Clock & Time Model](#31-discrete-simulation-clock--time-model)
   - [3.2 The 5-Phase Simulation Loop](#32-the-5-phase-simulation-loop)
   - [3.3 Request Entity & RouteHop Telemetry](#33-request-entity--routehop-telemetry)
   - [3.4 FIFO Queueing & Buffer Overflow Dropping](#34-fifo-queueing--buffer-overflow-dropping)
   - [3.5 Worker Instance Concurrency & Slot State Machine](#35-worker-instance-concurrency--slot-state-machine)
   - [3.6 CPU Utilization & Core Contention Mathematics](#36-cpu-utilization--core-contention-mathematics)
   - [3.7 Component Implementations & Routing Strategies](#37-component-implementations--routing-strategies)
   - [3.8 Mathematical Metrics & P99 Percentile Calculation](#38-mathematical-metrics--p99-percentile-calculation)
4. [C++ and Python Integration via pybind11](#4-c-and-python-integration-via-pybind11)
   - [4.1 Why In-Process Native Extension?](#41-why-in-process-native-extension)
   - [4.2 The Simulator Facade](#42-the-simulator-facade)
   - [4.3 Non-Blocking Execution & GIL Release](#43-non-blocking-execution--gil-release)
   - [4.4 Static Runtime Linking on Windows](#44-static-runtime-linking-on-windows)
5. [FastAPI Application & WebSocket Orchestration](#5-fastapi-application--websocket-orchestration)
   - [5.1 Background Async Simulation Loop & Pacing](#51-background-async-simulation-loop--pacing)
   - [5.2 WebSocket Connection Management & 20 FPS Broadcasting](#52-websocket-connection-management--20-fps-broadcasting)
   - [5.3 REST Endpoints & Pydantic Validation](#53-rest-endpoints--pydantic-validation)
6. [Frontend Architecture (React + React Flow + Zustand)](#6-frontend-architecture-react--react-flow--zustand)
7. [Bugs Found, Analyzed & Corrected](#7-bugs-found-analyzed--corrected)
8. [Deterministic Test Suite](#8-deterministic-test-suite)
9. [Developer Guide: Build, Test & Run](#9-developer-guide-build-test--run)

---

## 1. Executive Summary & Architecture Rationale

ArchiSys is an interactive distributed systems simulation platform. Users build system architectures on a visual canvas (e.g., Client ➔ Load Balancer ➔ Servers ➔ Redis Cache ➔ Database), configure component parameters (`procTime`, `instances`, `cpuCores`, `maxQueue`), and observe real-time queueing, CPU saturation, caching, bottleneck formation, and latency percentiles.

```
┌─────────────────────────────────────────────────────────────┐
│                 React Frontend (Vite + React Flow)          │
│            - Interactive Visual Node Canvas & Sliders       │
│            - Live Edge Packet Animations & Gauges           │
└──────────────────┬───────────────────────▲──────────────────┘
                   │ HTTP POST             │ WebSocket
                   │ (/start, /stop)       │ (/ws - 20 FPS)
                   ▼                       │
┌──────────────────────────────────────────┴──────────────────┐
│             FastAPI Backend (backend/api/)                  │
│            - REST Endpoints & Pydantic Validation           │
│            - WebSocket Connection Management                │
│            - Background Simulation Orchestration            │
│            - Future GenAI Orchestration Layer               │
│                                                             │
│   ┌─────────────────────────────────────────────────────┐   │
│   │ pybind11 In-Process Native C++ Binding Layer        │   │
│   └──────────────────────┬──────────────────────────────┘   │
│                          │ Direct C++ in-memory calls       │
│                          ▼ (Zero IPC / Zero Network)        │
│   ┌─────────────────────────────────────────────────────┐   │
│   │ C++ Simulation Engine (backend/engine/)             │   │
│   │   - Discrete Simulation Clock & Stepper             │   │
│   │   - FIFO Queues & Instance Concurrency Slots        │   │
│   │   - Load Balancing, Caching & Database Models       │   │
│   │   - P99 Latency, CPU & Throughput Computation       │   │
│   └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### Why This Architecture?

1. **C++ Simulation Core**: Discrete event simulations generating tens of thousands of requests demand deterministic, zero-allocation data structures, contiguous memory layout, and raw computational efficiency.
2. **Python FastAPI Orchestrator**: Web routing, schema validation (Pydantic), asynchronous WebSockets, and future Generative AI capabilities (e.g., natural language architecture generation) evolve rapidly and thrive in Python's modern ecosystem.
3. **In-Process Binding (`pybind11`)**: C++ is compiled into a native Python extension (`archisys_cpp.pyd` on Windows / `archisys_cpp.so` on Linux). Python and C++ run in the **exact same memory space**. Function calls execute in sub-microseconds without socket overhead, serialization, or subprocess management.
4. **No C++ Networking**: Raw WinSock2 socket servers were completely removed. All network I/O is managed cleanly by FastAPI.

---

## 2. Unified Codebase Directory Layout

```
archisys/
├── backend/                       # Single Unified Backend
│   ├── api/                       # Python FastAPI Application Layer
│   │   ├── ai/                    # GenAI Orchestrator Blueprint
│   │   │   ├── __init__.py
│   │   │   └── README.md
│   │   ├── main.py                # FastAPI REST endpoints & WebSocket route
│   │   ├── models.py              # Pydantic schemas (ArchitecturePayload, Metrics)
│   │   ├── simulator.py           # SimulationService Python wrapper & async loop
│   │   ├── websocket_manager.py   # WebSocket broadcast manager
│   │   └── __init__.py
│   │
│   ├── engine/                    # Native C++ Simulation Compute Core
│   │   ├── include/               # C++ Header Files
│   │   │   ├── component.hpp      # Base Component class
│   │   │   ├── components.hpp     # Server, Database, LoadBalancer, Redis, Queue
│   │   │   ├── json_parser.hpp    # JSON deserialization
│   │   │   ├── metrics.hpp        # Component & System metrics structs
│   │   │   ├── request.hpp        # Request & RouteHop telemetry structures
│   │   │   ├── simulation_engine.hpp # 5-phase discrete engine
│   │   │   └── simulator.hpp      # Facade exposed to Python
│   │   ├── src/                   # C++ Implementation Files
│   │   │   ├── bindings.cpp       # pybind11 native module bindings
│   │   │   ├── component.cpp      # FIFO queue & worker slot logic
│   │   │   ├── simulation_engine.cpp # Main simulation loop & P99 math
│   │   │   └── simulator.cpp      # Facade implementation
│   │   ├── third_party/           # Minimal JSON header
│   │   │   └── nlohmann/json.hpp
│   │   └── CMakeLists.txt         # Standalone build for archisys_cpp
│   │
│   ├── requirements.txt           # Python dependencies
│   └── __init__.py
│
├── frontend/                      # React + React Flow + Zustand UI
├── tests/                         # Deterministic Pytest Test Suite
├── docs/                          # In-depth architectural & API guides
├── pytest.ini                     # Pytest environment configuration
└── README.md                      # Quickstart documentation
```

---

## 3. Deep Code-Level Simulation Engine Logic

### 3.1 Discrete Simulation Clock & Time Model

The engine operates on a **discrete-time tick model**. It does not use wall-clock time for internal calculations. Instead, simulation time `simNow` starts at `0.0` seconds and advances by a fixed increment `dtSec` ($\Delta t = 0.01\text{ s}$ or $10\text{ ms}$ per tick).

```
Tick 0 (simNow = 0.00s) ➔ Tick 1 (simNow = 0.01s) ➔ Tick 2 (simNow = 0.02s) ➔ ...
```

This guarantees:
- **100% Deterministic Reproducibility**: The exact same random seed generates the identical request arrival sequence, queue states, and latencies across any machine.
- **Decoupled Speed**: Simulation can run at accelerated speeds (e.g. simulating 60 seconds in 200ms) or paced in real time.

---

### 3.2 The 5-Phase Simulation Loop

Located in [`backend/engine/src/simulation_engine.cpp`](file:///c:/Users/saabi/OneDrive/Documents/code/projects/archisys/backend/engine/src/simulation_engine.cpp), the simulation advances through five chronological phases on every tick:

```cpp
void SimulationEngine::step(double dtSec) {
    if (components_.empty()) return;
    double dt = dtSec > 0.0 ? dtSec : config_.tickSec;

    generateRequests(dt);      // Phase 1: Traffic Injection
    updateComponents(dt);      // Phase 2: Component Processing & Routing
    advanceSimulationTime(dt); // Phase 3: Time Clock Step
    if (onTick) {
        onTick(getSystemMetrics()); // Phase 4: Telemetry Hook
    }
}
```

```
┌────────────────────────────────────────────────────────────────────────┐
│ Phase 1: generateRequests(dt)                                          │
│  - Compute expected arrivals: E = rate * dt                            │
│  - Inject requests into entry point Client component                   │
└───────────────────────────────────┬────────────────────────────────────┘
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Phase 2: updateComponents(dt)                                          │
│  - For each component:                                                 │
│      1. Complete active slots where finishAt <= simNow                 │
│      2. Record slot departure & compute hop processing duration        │
│      3. Dequeue waiting requests from waitQueue into free slots        │
│      4. Calculate queue wait latency (simNow - arrivalTime)            │
│      5. Forward finished requests downstream (or mark Completed)       │
└───────────────────────────────────┬────────────────────────────────────┘
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Phase 3: advanceSimulationTime(dt)                                     │
│  - Increment clock: simNow += dt                                       │
└───────────────────────────────────┬────────────────────────────────────┘
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Phase 4: calculateMetrics() & onTick()                                 │
│  - Calculate SystemMetrics (inFlight, avgLatencyMs, p99LatencyMs, RPS) │
│  - Snapshot ComponentMetrics (queueDepth, cpuUsagePct, throughput)     │
└────────────────────────────────────────────────────────────────────────┘
```

---

### 3.3 Request Entity & RouteHop Telemetry

Every request moving through ArchiSys is a first-class object defined in [`backend/engine/include/request.hpp`](file:///c:/Users/saabi/OneDrive/Documents/code/projects/archisys/backend/engine/include/request.hpp):

```cpp
struct RouteHop {
    int         componentId;
    std::string componentName;
    double      arrivalTime;      // Sim-seconds when entered queue
    double      processingStart;  // Sim-seconds when allocated to worker slot
    double      departureTime;    // Sim-seconds when finished processing
    double      queueWaitTimeMs;  // (processingStart - arrivalTime) * 1000
    double      processingTimeMs; // (departureTime - processingStart) * 1000
};

struct Request {
    uint64_t              id;
    uint64_t              clientId;
    double                createdAt;      // Sim time when spawned
    double                completedAt;    // Sim time when finished (0 = in flight)
    RequestStatus         status;         // Created, InQueue, Processing, Forwarded, Completed, Dropped
    int                   currentComponentId;
    double                totalLatencyMs;   // Sum of processing delays
    double                totalQueueWaitMs; // Sum of queue delays
    std::vector<RouteHop> route;          // Full hop-by-hop history
};
```

#### End-to-End Latency Calculation
When a request finishes its final hop, its total round-trip latency is:
$$\text{RoundTripMs} = (\text{completedAt} - \text{createdAt}) \times 1000.0\text{ ms}$$
This includes the exact time spent waiting in every queue plus the computation time across every component in its path.

---

### 3.4 FIFO Queueing & Buffer Overflow Dropping

In [`backend/engine/src/component.cpp`](file:///c:/Users/saabi/OneDrive/Documents/code/projects/archisys/backend/engine/src/component.cpp), when a request arrives at a component:

```cpp
bool Component::receiveRequest(std::shared_ptr<Request> req, double simNow) {
    if (static_cast<int>(waitQueue_.size()) >= maxQueue) {
        // Queue full — drop request immediately (buffer overflow)
        req->status = RequestStatus::Dropped;
        req->completedAt = simNow;
        ++droppedCount_;
        return false;
    }

    req->status = RequestStatus::InQueue;
    req->currentComponentId = id_;

    RouteHop hop;
    hop.componentId   = id_;
    hop.componentName = name_;
    hop.arrivalTime   = simNow;
    req->route.push_back(hop);

    waitQueue_.push(req);
    ++rxCount_;
    return true;
}
```

If the incoming request rate exceeds the component's processing throughput and the queue reaches `maxQueue`, excess requests are dropped and accounted for in `requestsDropped`.

---

### 3.5 Worker Instance Concurrency & Slot State Machine

A component has `instances` representing the number of parallel processing threads / container replicas.

Each worker instance is modeled as an `ActiveSlot`:
```cpp
struct ActiveSlot {
    std::shared_ptr<Request> req;
    double startedAt; // Sim time when slot started working
    double finishAt;  // Scheduled completion sim time
};
```

#### Execution Lifecycle inside `Component::tick()`:
1. **Slot Completion**:
   Any slot with `simNow >= slot.finishAt` is completed. Processing duration is recorded:
   $$\text{procMs} = (\text{simNow} - \text{slot.startedAt}) \times 1000.0$$
   The request is marked `Forwarded` and pushed to the `completed` batch for routing. The slot becomes `nullptr` (free).

2. **Slot Scheduling (Promotion from Queue)**:
   For every free slot (`slot.req == nullptr`), if `!waitQueue_.empty()`:
   - Pop `req` from `waitQueue_`.
   - Record queue wait time: $\text{waitMs} = (\text{simNow} - \text{hop.arrivalTime}) \times 1000$.
   - Calculate processing duration: `procDurationMs = computeProcessingDurationMs(req, simNow)`.
   - Schedule completion timestamp: `slot.finishAt = simNow + (procDurationMs / 1000.0)`.

---

### 3.6 CPU Utilization & Core Contention Mathematics

1. **Instantaneous CPU Utilization (%)**:
   $$\text{CPU Usage} = \min\left(100.0, \frac{\text{busySlots}}{\text{instances}} \times 100.0\right)$$

2. **CPU Core Contention Scaling**:
   If a component has $M$ concurrent active worker slots and $K$ allocated CPU cores (`cpuCores`), when $M > K$, CPU contention occurs:
   $$\text{ContentionFactor} = \frac{\text{busySlots}}{\text{cpuCores}}$$
   $$\text{EffectiveProcessingDuration} = \text{baseDuration} \times \min\left(2.5, \text{ContentionFactor}\right)$$

This realistically models compute degradation when too many threads fight for fewer physical CPU cores.

---

### 3.7 Component Implementations & Routing Strategies

Defined in [`backend/engine/include/components.hpp`](file:///c:/Users/saabi/OneDrive/Documents/code/projects/archisys/backend/engine/include/components.hpp):

#### 1. Client Node
- Traffic entry point. Injects requests according to a Poisson arrival process:
  $$\lambda = \text{requestRatePerSec}$$
  $$\text{Expected} = \lambda \times \Delta t$$
  $$\text{Count} = \lfloor \text{Expected} \rfloor + \left(\text{rand}() < (\text{Expected} - \lfloor \text{Expected} \rfloor) ? 1 : 0\right)$$
- $0\text{ ms}$ processing latency pass-through.

#### 2. Server Node
- Multi-instance application worker pool.
- Concurrently processes requests for `procTime` ms across `instances` slots.

#### 3. Database Node
- Simulates SQL/NoSQL storage tier with connection pooling (`maxQueue = 50`).
- Read queries take `procTime` ms.
- Write queries ($\approx 20\%$ of requests: `req->id % 5 == 0`) take $\text{procTime} \times 2.0$ ms.
- **Bug Fix**: Processing time is calculated per-request and does not mutate global component state.

#### 4. LoadBalancer Node
- Distributes traffic across connected downstream servers (`outgoing_`).
- Supports three balancing algorithms:
  - **`LeastConnections`**: Dispatches to the downstream server with the smallest total load ($\text{queueDepth} + \text{busySlots}$).
  - **`RoundRobin`**: Cycles through downstream servers sequentially (`rrIndex++ % outgoing.size()`).
  - **`Random`**: Randomly picks an outgoing link.

#### 5. RedisCache Node
- In-memory key-value cache layer.
- **Cache Hit ($80\%$ probability)**:
  - Lookup takes `procTime` ($1\text{ ms}$).
  - Request is marked `Completed` immediately and returned to the client without calling the database.
- **Cache Miss ($20\%$ probability)**:
  - Lookup takes $3 \times \text{procTime}$ ($3\text{ ms}$) before forwarding downstream to the Database.
- **Bug Fix**: Latency is tracked accurately without double counting.

#### 6. MessageQueue Node
- Asynchronous message broker (Kafka/RabbitMQ model).
- Buffers requests with high capacity (`maxQueue = 10000`) and drains them downstream according to consumer rate.

---

### 3.8 Mathematical Metrics & P99 Percentile Calculation

In [`backend/engine/src/simulation_engine.cpp`](file:///c:/Users/saabi/OneDrive/Documents/code/projects/archisys/backend/engine/src/simulation_engine.cpp):

1. **Mean Latency**:
   $$\bar{L} = \frac{1}{N} \sum_{i=1}^{N} \text{RoundTripMs}_i$$

2. **99th Percentile (P99) Latency**:
   All completed request latencies are recorded in `completedLatencies_`. To compute $P99$ efficiently without a full $O(N \log N)$ sort:
   ```cpp
   if (!completedLatencies_.empty()) {
       std::vector<double> copy = completedLatencies_;
       size_t idx = static_cast<size_t>(std::ceil(0.99 * copy.size())) - 1;
       if (idx >= copy.size()) idx = copy.size() - 1;
       std::nth_element(copy.begin(), copy.begin() + idx, copy.end());
       m.p99LatencyMs = copy[idx];
   }
   ```
   This uses $O(N)$ selection to accurately compute the 99th percentile.

3. **Rolling System & Component Throughput (req/s)**:
   $$\text{Throughput} = \frac{\Delta \text{CompletedRequests}}{\Delta t}$$

---

## 4. C++ and Python Integration via pybind11

### 4.1 Why In-Process Native Extension?
Traditional multi-language setups use HTTP, gRPC, stdin/stdout, or subprocess pipes. In ArchiSys, `pybind11` compiles C++ directly into `archisys_cpp.pyd`.

| Metric / Attribute | In-Process pybind11 | Local Socket / IPC | Subprocess / stdin-stdout |
| :--- | :--- | :--- | :--- |
| **Call Latency** | **$< 0.001\text{ ms}$ (direct RAM call)** | $0.5 - 2.0\text{ ms}$ | $5 - 20\text{ ms}$ |
| **Memory Copies** | **Zero-copy / direct struct access** | Full TCP serialization | Pipe serialization |
| **Process Overhead** | **1 single Python process** | 2 separate OS processes | 2+ processes + IPC pipes |
| **Reliability** | **Crash-safe inside process** | Port conflicts, firewall issues | Broken pipe handling |

### 4.2 The Simulator Facade
Python interacts exclusively with the `Simulator` facade in [`backend/engine/include/simulator.hpp`](file:///c:/Users/saabi/OneDrive/Documents/code/projects/archisys/backend/engine/include/simulator.hpp):

```cpp
class Simulator {
public:
    void loadArchitecture(const std::string& architectureJson);
    void start();
    void step(double dtSec);
    void stop();
    bool isRunning() const;
    SystemMetrics getMetrics() const;
    void reset();
};
```

### 4.3 Non-Blocking Execution & GIL Release
In [`backend/engine/src/bindings.cpp`](file:///c:/Users/saabi/OneDrive/Documents/code/projects/archisys/backend/engine/src/bindings.cpp):

```cpp
py::class_<Simulator>(m, "Simulator")
    .def("start", &Simulator::start, py::call_guard<py::gil_scoped_release>())
    .def("step",  &Simulator::step,  py::call_guard<py::gil_scoped_release>())
    ...
```

The `py::call_guard<py::gil_scoped_release>()` drops Python's Global Interpreter Lock during C++ execution. This ensures FastAPI's asyncio event loop handles incoming HTTP requests and WebSocket packets with zero lag.

### 4.4 Static Runtime Linking on Windows
In [`backend/engine/CMakeLists.txt`](file:///c:/Users/saabi/OneDrive/Documents/code/projects/archisys/backend/engine/CMakeLists.txt):
```cmake
target_link_libraries(archisys_cpp PRIVATE -mthreads -static -static-libgcc -static-libstdc++)
```
This statically embeds `libwinpthread` and `libstdc++`, producing a standalone `.pyd` that loads cleanly in standard CPython without requiring MinGW DLLs on the Windows `PATH`.

---

## 5. FastAPI Application & WebSocket Orchestration

### 5.1 Background Async Simulation Loop & Pacing
Located in [`backend/api/simulator.py`](file:///c:/Users/saabi/OneDrive/Documents/code/projects/archisys/backend/api/simulator.py), `SimulationService` manages execution:

```python
async def _run_simulation_loop(self, duration_sec: float, tick_sec: float, total_requests: int):
    elapsed_sim_time = 0.0
    broadcast_interval = 0.05  # 50ms (20 FPS)
    time_since_broadcast = 0.0

    while self._is_active and elapsed_sim_time <= duration_sec:
        # Step C++ engine by 5 ticks (50ms) per batch
        step_dt = tick_sec * 5
        self._simulator.step(step_dt)
        elapsed_sim_time += step_dt
        time_since_broadcast += step_dt

        # Check totalRequests completion
        metrics_obj = self._simulator.get_metrics()
        if total_requests > 0:
            done = metrics_obj.completed + metrics_obj.dropped + metrics_obj.failed
            if done >= total_requests and metrics_obj.in_flight == 0:
                await ws_manager.broadcast(metrics_obj.to_dict())
                break

        # Broadcast telemetry frame
        if time_since_broadcast >= broadcast_interval:
            await ws_manager.broadcast(metrics_obj.to_dict())
            time_since_broadcast = 0.0

        # Yield control to FastAPI event loop
        await asyncio.sleep(0.01)
```

### 5.2 WebSocket Connection Management & 20 FPS Broadcasting
[`backend/api/websocket_manager.py`](file:///c:/Users/saabi/OneDrive/Documents/code/projects/archisys/backend/api/websocket_manager.py) manages connected frontend clients and broadcasts JSON telemetry frames every 50ms (20 updates/second).

### 5.3 REST Endpoints & Pydantic Validation
[`backend/api/main.py`](file:///c:/Users/saabi/OneDrive/Documents/code/projects/archisys/backend/api/main.py):
- `GET /health` ➔ Returns `{ "status": "ok", "ok": true }`
- `POST /start` ➔ Validates `ArchitecturePayload` and launches simulation
- `POST /stop` ➔ Stops simulation
- `GET /metrics` ➔ Returns latest metrics snapshot
- `WebSocket /ws` ➔ Live telemetry stream

---

## 6. Frontend Architecture (React + React Flow + Zustand)

- **Canvas**: Built with `@xyflow/react` (`frontend/src/`).
- **State Management**: Zustand store (`useStore.ts`) holding node topology, configuration parameters, and live telemetry updates.
- **Connection**:
  - `client.ts`: Targets `http://localhost:8000/start`, `/stop`, `/health`.
  - `useWebSocket.ts`: Connects to `ws://localhost:8000/ws`.

---

## 7. Bugs Found, Analyzed & Corrected

| Bug / Problem | Root Cause Analysis | Corrected Behavior |
| :--- | :--- | :--- |
| **Missing P99 Metric** | `p99LatencyMs` was declared in `SystemMetrics` struct but never computed (always `0.0`). | Implemented $O(N)$ percentile selection via `std::nth_element` on completed request latencies. |
| **Redis Latency Double-Count** | `RedisCache::processRequest` added latency to `totalLatencyMs`, and `Component::tick()` added `processingMs` again. | Recorded latency once upon slot completion based on true departure and start timestamps. |
| **Database State Corruption** | `Database::processRequest` mutated `this->processingMs` member, overriding user config for subsequent requests. | Made `computeProcessingDurationMs` pure and per-request without modifying class fields. |
| **MessageQueue Inactivity** | `consumeRatePerSec` was declared but never referenced in dequeuing logic. | Added buffer capacity checks and drain pacing. |
| **Map Order Dependence** | Map iteration order caused downstream nodes to tick before or after upstream nodes inconsistently. | Implemented discrete time batching with completed request buffering. |

---

## 8. Deterministic Test Suite

Located in [`tests/`](file:///c:/Users/saabi/OneDrive/Documents/code/projects/archisys/tests), verified with `pytest`:

1. `test_health_check_endpoint`: Verifies `GET /health`.
2. `test_start_and_stop_endpoints`: Verifies simulation start/stop lifecycle.
3. `test_start_endpoint_empty_nodes_validation`: Verifies Pydantic 400 validation on empty payload.
4. `test_client_to_server_scenario`: Verifies single-hop queueing, worker instances, and latency accumulation.
5. `test_client_loadbalancer_two_servers_scenario`: Verifies LeastConnections traffic distribution across multiple servers.
6. `test_client_lb_server_redis_database_scenario`: Verifies multi-tier pipeline (80% cache hits returned early, 20% misses forwarded to DB).
7. `test_queue_overflow_and_dropping`: Verifies `maxQueue` overflow and dropped request accounting.
8. `test_websocket_stream`: Verifies WebSocket connection and bi-directional ping/pong.

```powershell
.\.venv\Scripts\pytest -v tests/
# ======================== 8 passed in 0.64s ========================
```

---

## 9. Developer Guide: Build, Test & Run

### 1. Setup Virtual Environment
```powershell
python -m venv .venv
.\.venv\Scripts\pip install -r backend/requirements.txt
```

### 2. Build C++ pybind11 Extension
```powershell
cmake -B backend/engine/build -S backend/engine -G "MinGW Makefiles" `
  -DPython_EXECUTABLE="$PWD/.venv/Scripts/python.exe" `
  -Dpybind11_DIR="$PWD/.venv/Lib/site-packages/pybind11/share/cmake/pybind11"

cmake --build backend/engine/build --config Release
Copy-Item "backend/engine/build/archisys_cpp.*.pyd" -Destination ".venv/Lib/site-packages/archisys_cpp.pyd" -Force
```

### 3. Run Tests
```powershell
.\.venv\Scripts\pytest -v tests/
```

### 4. Start the Application

```powershell
# Terminal 1: FastAPI Backend (Port 8000)
.\.venv\Scripts\uvicorn backend.api.main:app --host 0.0.0.0 --port 8000 --reload

# Terminal 2: React Frontend (Port 5173)
cd frontend
npm run dev
```

Open **`http://localhost:5173`** in your browser.
