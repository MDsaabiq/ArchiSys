# ArchiSys System Architecture

## Overview

ArchiSys is a real-time distributed systems simulation and architecture modeling platform. It employs a high-performance, layered architecture that decouples heavy mathematical simulation compute from the application, orchestration, and visualization layers.

```
┌─────────────────────────────────────────────────────────────┐
│                 React Frontend (Vite + React Flow)          │
│            - Visual Architecture Canvas & Node Config       │
│            - Real-time Animated Metrics & Gauges            │
└──────────────────┬───────────────────────▲──────────────────┘
                   │ HTTP POST             │ WebSocket
                   │ (/start, /stop)       │ (/ws - live metrics)
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

---

## Unified Project Structure

```
archisys/
├── backend/                       # Unified Backend
│   ├── api/                       # Python FastAPI Application Layer
│   │   ├── ai/                    # Generative AI Orchestrator Blueprint
│   │   ├── main.py                # FastAPI app & endpoint routing
│   │   ├── models.py              # Pydantic request/response schemas
│   │   ├── simulator.py           # SimulationService Python wrapper
│   │   ├── websocket_manager.py   # WebSocket client connection broadcaster
│   │   └── __init__.py
│   ├── engine/                    # Native C++ Discrete Simulation Engine
│   │   ├── include/               # C++ header files
│   │   │   ├── component.hpp
│   │   │   ├── components.hpp
│   │   │   ├── json_parser.hpp
│   │   │   ├── metrics.hpp
│   │   │   ├── request.hpp
│   │   │   ├── simulation_engine.hpp
│   │   │   └── simulator.hpp
│   │   ├── src/                   # C++ implementation files
│   │   │   ├── bindings.cpp       # pybind11 native module bindings
│   │   │   ├── component.cpp
│   │   │   ├── simulation_engine.cpp
│   │   │   └── simulator.cpp
│   │   ├── third_party/           # nlohmann mini json header
│   │   │   └── nlohmann/json.hpp
│   │   └── CMakeLists.txt         # CMake build script for archisys_cpp
│   ├── requirements.txt           # Python backend dependencies
│   └── __init__.py
│
├── frontend/                      # React + React Flow + Vite Canvas UI
├── tests/                         # Deterministic Pytest Test Suite
├── docs/                          # Architecture & Simulation Documentation
├── pytest.ini                     # Pytest environment configuration
└── README.md
```

---

## Architectural Separation of Concerns

### 1. C++ Simulation Engine (`backend/engine/`)
- **Purpose**: Pure computational core.
- **Responsibilities**:
  - Request generation and Poisson arrival distributions.
  - Discrete simulation time advancement (`simNow += dt`).
  - Strict FIFO wait queue management and overflow drop mechanics.
  - Instance concurrency slots and worker thread modeling.
  - Multi-hop routing (LoadBalancer, Redis Cache, Database, MessageQueue).
  - Exact latency profiling (queue wait, compute duration, round-trip).
  - System-wide metric aggregations including exact P99 percentile calculations.
- **Constraints**: Contains **no networking code**, no sockets, and no API servers. It runs directly inside the host Python process.

### 2. Python FastAPI Orchestration Layer (`backend/api/`)
- **Purpose**: Application API, WebSocket delivery, and orchestration.
- **Responsibilities**:
  - Validates frontend JSON payloads via Pydantic (`ArchitecturePayload`).
  - Manages the lifecycle of the C++ `Simulator` instance.
  - Executes non-blocking background async simulation loops.
  - Broadcasts live telemetry frames at smooth 20 FPS intervals to connected React clients via WebSockets (`/ws`).
  - Future home for GenAI agents (`User Prompt ➔ LLM ➔ Architecture JSON ➔ Simulator`).

### 3. React Frontend (`frontend/`)
- **Purpose**: Interactive user interface.
- **Responsibilities**:
  - Visual system topology builder using React Flow.
  - Real-time component configuration sliders (`cpuCores`, `procTime`, `instances`, `maxQueue`, `requestRate`).
  - Live metric visualization (CPU utilization bars, latency gauges, throughput counters).

---

## Why Keep the C++ Simulation Engine?

1. **Computational Performance & Predictable Memory**: Discrete simulations processing tens of thousands of concurrent requests benefit significantly from compiled C++ execution, contiguous memory arrays, and zero-allocation queue structures.
2. **Separation of Lifecycles**: The simulation engine represents a stable, mathematically rigorous compute core. In contrast, the web API, authentication, WebSocket framing, and GenAI capabilities evolve rapidly and are best built with Python and FastAPI.
3. **In-Process Integration (Zero Overhead)**: By using `pybind11` to compile C++ into a Python native extension (`archisys_cpp.pyd` / `archisys_cpp.so`), calls between Python and C++ happen directly in memory without subprocess overhead, named pipes, sockets, or serialization bottlenecks.
