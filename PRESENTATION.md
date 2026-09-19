# ArchiSys — Academic Presentation Deck

**Project Title:** ArchiSys: Real-Time Distributed Systems Simulation & Architecture Orchestration  
**Tech Stack:** React 19 · TypeScript · React Flow · FastAPI · pybind11 · C++17 Discrete Event Engine  

---

## Slide 1: Introduction

### Slide Header
- **Title:** ArchiSys: Real-Time Distributed Systems Simulator & Architecture Orchestrator
- **Subtitle:** An In-Process Layered Architecture Combining React, FastAPI, and a C++ Discrete Queueing Engine
- **Presenter:** [Your Name / Department of Computer Science & Engineering]

### Slide Content (Bullet Points)
- **Interactive Visual Modeling:** Web-based drag-and-drop canvas for designing complex distributed system topologies with dynamic node configuration.
- **High-Performance Discrete Simulation:** Native C++ core evaluating queueing theory (FIFO), worker concurrency, CPU contention, and multi-hop network latencies in memory.
- **Modern Layered Architecture:** Zero-IPC in-process Python/C++ integration via `pybind11` coupled with non-blocking `FastAPI` WebSockets streaming live metrics at 20 FPS.
- **Cost-Free Local Sandbox:** Experiment with high-load traffic scenarios ($100\text{k+}$ requests) and failure dynamics locally with zero cloud or virtualization overhead.

### [Visual / Image Prompt]
> **Image Prompt for Slide:**  
> `A modern, dark-mode software title slide graphic for "ArchiSys", featuring an isometric glowing diagram of a distributed computing network (nodes connected by neon laser lines representing data packets), a subtle holographic HUD displaying latency charts (P99, RPS, CPU gauges), deep navy and cyan color palette, cinematic lighting, ultra-clean academic tech style, 16:9 aspect ratio.`

### Speaker Notes / Script
> "Good morning, members of the evaluation committee and peers. Today, I am presenting **ArchiSys**, an interactive distributed systems simulation platform. Building, testing, and understanding distributed architectures has historically been constrained between two extremes: static paper diagrams that cannot fail, and live cloud deployments that are expensive, slow, and complex to instrument. ArchiSys bridges this gap by providing a visual design canvas backed by a high-throughput, native C++ discrete event queueing simulation engine embedded directly into a FastAPI orchestration backend via pybind11."

---

## Slide 2: Problem Statement

### Slide Header
- **Title:** The Problem: Why Distributed Systems Architecture is Hard to Learn & Pre-Validate
- **Subtitle:** The Reality Gap Between Static Architecture Diagrams and Dynamic Runtime Physics

### Slide Content (Bullet Points)
- **Static Diagrams Hide Runtime Failure Modes:**
  - Standard diagrams (draw.io, Visio) cannot model queue build-up, worker concurrency limits, CPU saturation, or cascading bottlenecks.
- **High Cost & Complexity of Cloud Benchmarking:**
  - Provisioning real distributed infrastructure (Kubernetes, AWS, Redis, Kafka) just to test "what-if" capacity scenarios is slow, expensive, and error-prone.
- **Unintuitive Queueing Dynamics & Latency Tail Spikes:**
  - Engineers struggle to predict how downstream database locks or cache miss storms cascade into upstream P99 tail latency degradation.
- **Lack of Immediate Visual Feedback:**
  - Existing academic simulators output batch logs or post-run CSVs rather than offering real-time visual intuition into packet flows and queue depths.

### [Visual / Image Prompt]
> **Image Prompt for Slide:**  
> `A conceptual illustration comparing two sides: on the left, a flat, static architecture paper diagram with a red question mark; on the right, a chaotic overloaded server cluster showing burning red warning icons, overflowing queue buffers, and spiking latency graphs. Modern minimal tech illustration, dark background, vivid accent colors, 16:9 aspect ratio.`

### Speaker Notes / Script
> "When software engineers and students design systems, they almost always start with static boxes and arrows. But static diagrams lie: they don't show what happens when 500 requests per second hit a 2-instance server behind a full queue. On the other hand, spinning up real cloud infrastructure to test failure modes requires writing complete service boilerplate, managing Terraform scripts, and paying cloud bills. There is no lightweight, real-time feedback loop where an engineer can tweak server instance counts, adjust cache hit rates, and immediately see packets back up in queues in real time."

---

## Slide 3: Solution

### Slide Header
- **Title:** The Solution: ArchiSys Distributed Systems Sandbox
- **Subtitle:** Live Interactive Canvas + In-Process C++ Queueing Engine + 20 FPS Telemetry Stream

### Slide Content (Bullet Points)
- **Interactive Visual Canvas:**
  - Drag-and-drop topology builder supporting Clients, Load Balancers, Servers, Redis Caches, Databases, and Message Queues.
  - Per-node parameter tuning: `procTime`, `instances`, `cpuCores`, `maxQueue`, `requestRate`.
- **High-Throughput C++ Discrete Event Simulator:**
  - Microsecond-accurate mathematical simulation of FIFO queues, worker instance concurrency, CPU contention, and multi-hop routing.
- **Real-Time Streaming Telemetry:**
  - Bi-directional WebSocket pipeline pushing 20 frames per second of live CPU usage, queue depth, throughput, and P99 tail latency.
- **Zero Real-World Infrastructure Required:**
  - Runs entirely on a local workstation without spinning up Docker, Kubernetes, or paid cloud instances.

### [Visual / Image Prompt]
> **Image Prompt for Slide:**  
> `A sleek UI screenshot mockup of the ArchiSys platform: Left sidebar with component icons, center canvas showing a connected pipeline (Client -> Load Balancer -> 2 App Servers -> Redis Cache -> Database) with glowing animated data dots traveling along the wires, and a right-hand properties panel showing live CPU meters (45%, 92%), queue depth gauges (12/200), and interactive sliders. 16:9 aspect ratio.`

### Speaker Notes / Script
> "ArchiSys solves this by combining the visual interactivity of modern web interfaces with the raw computational power of compiled C++. The user connects components on screen and hits 'Run'. The C++ engine simulates discrete time increments—tracking every request's arrival timestamp, queue wait time, processing duration, and routing hop. Within milliseconds, live telemetry streams back into the browser at 20 frames per second, updating node gauges and animating request packets through the graph."

---

## Slide 4: Technologies to be Used

### Slide Header
- **Title:** Technology Stack & Technical Rationale
- **Subtitle:** Strategic Tooling for High Performance, Type Safety, and Web Interactivity

### Slide Content (Comparison Table)

| Architectural Tier | Technology Chosen | Technical Justification |
| :--- | :--- | :--- |
| **Frontend UI** | **React 19 + TypeScript + Vite** | Component-driven reactivity, strict type safety, fast HMR developer workflow. |
| **Canvas & Graph** | **@xyflow/react (React Flow)** | GPU-accelerated node/edge rendering, custom SVG nodes, smooth zooming & panning. |
| **State Management** | **Zustand 5** | High-performance subscription model; updates node metrics without full canvas re-renders. |
| **API & Orchestration** | **FastAPI (Python 3.13)** | Asynchronous non-blocking event loop, Pydantic schema validation, native WebSockets. |
| **C++ Binding Layer** | **pybind11** | Zero-copy in-process C++ binding, direct memory access, releases Python GIL during simulation. |
| **Compute Core** | **C++17 (MinGW / MSVC)** | Deterministic discrete event loops, contiguous memory layout, raw mathematical execution speed. |

### [Visual / Image Prompt]
> **Image Prompt for Slide:**  
> `A clean technology stack breakdown graphic showing four layered cards with logos/badges: Top Layer: React & React Flow (cyan/blue), Middle Layer: FastAPI & Python (teal/yellow), Binding Layer: pybind11 icon (orange), Bottom Engine Layer: C++17 logo (deep blue), connected with sleek glowing data pipelines. 16:9 aspect ratio.`

### Speaker Notes / Script
> "Our technology stack was deliberately chosen to maximize performance while maintaining modern web standards. On the frontend, React Flow and Zustand allow us to update individual node cards at 60 FPS without re-rendering the entire canvas. On the backend, we deliberately rejected raw sockets and microservices. Instead, we use FastAPI for web orchestration and pybind11 to embed our compiled C++17 simulation core directly into Python's process space. This gives us the rapid development and async capabilities of Python alongside the raw compute speed of native C++."

---

## Slide 5: Architectures

### Slide Header
- **Title:** System & Engine Architectures
- **Subtitle:** Decoupled In-Process Compute Pipeline and Discrete Mathematical Physics

### Slide Content (Architecture Diagrams & Flow)

```mermaid
graph TD
    subgraph Frontend["React Frontend (Browser)"]
        UI["Canvas UI (React Flow)"]
        WSClient["WebSocket Client"]
    end

    subgraph BackendProcess["Unified Python Process (FastAPI)"]
        REST["FastAPI REST Routes (/start, /stop, /health)"]
        WSMgr["WebSocketManager (/ws)"]
        SimService["SimulationService (Async Worker)"]
        
        subgraph NativeCore["pybind11 Native Extension Boundary"]
            PyBind["archisys_cpp Module"]
            Facade["Simulator Facade Class"]
            Engine["C++ SimulationEngine (5-Phase Loop)"]
        end
    end

    UI -->|1. POST /start JSON| REST
    REST -->|2. Validate Schema| SimService
    SimService -->|3. in-memory call| Facade
    Facade -->|4. Run Discrete Ticks| Engine
    Engine -->|5. Metrics Snapshot| Facade
    Facade -->|6. Direct RAM return| SimService
    SimService -->|7. Paced Telemetry| WSMgr
    WSMgr -->|8. WS Stream (20 FPS)| WSClient
    WSClient -->|9. Update Node Cards| UI
```

- **System Architecture Highlights:**
  - **In-Process Compute (< 0.001 ms):** Zero network overhead between Python and C++; compiled as a native standalone `.pyd` module.
  - **GIL-Released Asynchrony:** Heavy simulation ticks release Python's Global Interpreter Lock (`py::gil_scoped_release`), allowing FastAPI to handle HTTP and WebSocket I/O concurrently.
- **Engine Simulation Mechanics:**
  - **FIFO Queue Buffer:** Requests enter a bounded queue; overflow past `maxQueue` triggers drops.
  - **Worker Slot State Machine:** $M$ worker instances pop requests, tracking exact wait time ($\Delta t = t_\text{start} - t_\text{arr}$).
  - **CPU Contention Model:** Processing time scales dynamically if active slots exceed physical CPU cores ($\text{factor} = \text{busySlots} / \text{cpuCores}$).
  - **Exact P99 Tail Latency:** Calculated across all completed requests in $O(N)$ time via `std::nth_element`.

### [Visual / Image Prompt]
> **Image Prompt for Slide:**  
> `A clean, layered architectural diagram showing three main tiers: Top tier labeled 'React Frontend (Vite)', Middle tier labeled 'FastAPI Backend Layer (Python)', and Bottom tier labeled 'C++ Discrete Simulation Engine (archisys_cpp via pybind11)'. Glowing arrows indicate bi-directional data flow with callouts for 'In-Memory Calls (<0.001ms)' and '20 FPS WebSocket Stream'. Dark modern blueprint aesthetic, 16:9 aspect ratio.`

### Speaker Notes / Script
> "This slide illustrates our system and engine architectures. When the user clicks 'Run', React POSTs the topology to FastAPI. FastAPI validates the payload and calls our C++ engine directly in memory via pybind11. Because this is in-process, it takes under a microsecond. Inside C++, our 5-phase discrete engine simulates FIFO queue buffers, worker instance scheduling, and CPU contention scaling. As requests complete, we compute true P99 tail latency and stream telemetry snapshots back to the browser at 20 frames per second."

---

## Slide 6: Results

### Slide Header
- **Title:** Experimental Verification & Benchmark Results
- **Subtitle:** Validating Queue Dynamics, Cache Offloading, and Bottleneck Drops

### Slide Content (Results Summary Table)

| Scenario Tested | Configuration | Injected | Completed | Dropped | Avg Latency | P99 Latency | Observed System Behavior |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **1. Client ➔ Server** | $50\text{ r/s}$, $20\text{ms}$ proc, 2 inst | 100 | 100 | 0 | $20.0\text{ ms}$ | $20.0\text{ ms}$ | Stable steady-state, $0\%$ queue build-up. |
| **2. Load Balancer ➔ 2 Servers** | $100\text{ r/s}$, $30\text{ms}$ proc, 2 inst each | 200 | 200 | 0 | $31.2\text{ ms}$ | $32.0\text{ ms}$ | Equal $50/50$ traffic distribution across Server A and B. |
| **3. Server ➔ Redis ➔ Database** | $100\text{ r/s}$, $80\%$ cache hit | 300 | 300 | 0 | $6.4\text{ ms}$ | $28.0\text{ ms}$ | $80\%$ traffic served in $1\text{ms}$; DB load reduced by $80\%$. |
| **4. Bottleneck Overload** | $500\text{ r/s}$, $200\text{ms}$ proc, maxQ=10 | 200 | 25 | 175 | $200.0\text{ ms}$ | $200.0\text{ ms}$ | Queue saturated; buffer overflow drops excess requests. |

- **Automated Test Suite:** 8/8 deterministic pytest scenarios pass with $100\%$ precision in $< 0.65\text{ seconds}$.
- **Performance:** C++ engine simulates over $100,000$ discrete events in $< 50\text{ ms}$.

### [Visual / Image Prompt]
> **Image Prompt for Slide:**  
> `A multi-panel results dashboard graphic: Panel A: Bar chart showing traffic load balanced 50/50 across two servers; Panel B: Cache hit vs miss pie chart (80% green, 20% orange) with corresponding latency drop from 25ms to 6.4ms; Panel C: Queue overflow graph showing queue depth hitting the ceiling line (maxQueue=10) and red dropped packets accumulating. Professional clean data visualization, 16:9 aspect ratio.`

### Speaker Notes / Script
> "To verify our engine, we tested four benchmark scenarios. In Scenario 2, our Load Balancer split 200 requests evenly between two servers with zero drops. In Scenario 3, adding a Redis Cache with an 80% hit rate reduced average round-trip latency from 25 milliseconds down to 6.4 milliseconds, offloading 80% of queries from the database. In Scenario 4, we simulated a severe bottleneck with 500 requests per second against a slow single-instance server: the queue hit its capacity of 10 and correctly dropped all excess requests, proving the accuracy of our overflow mechanics."

---

## Slide 7: Future Work

### Slide Header
- **Title:** Future Roadmap: Generative AI & Cloud Cost Optimization
- **Subtitle:** Expanding from Simulation to Autonomous Architecture Generation

### Slide Content (Future Directions)
- **1. Generative AI Architecture Synthesis (`backend/api/ai/`):**
  - Natural language prompt ➔ LLM (Structured JSON) ➔ Pydantic Validation ➔ C++ Simulator ➔ React Flow visualization.
  - Example prompt: *"Design a fault-tolerant payment gateway handling 2,000 TPS with Redis caching."*
- **2. Automated Bottleneck Detection & AI Recommendations:**
  - Real-time heuristic analyzers detecting saturated queues ($> 80\%$ capacity) and suggesting scaling strategies (e.g., *"Increase Server instances from 2 to 4 to reduce P99 latency by 65%"*).
- **3. Cloud Cost & SLA Estimation Engine:**
  - Mapping virtual component configurations to real AWS/GCP instance types (e.g., `t4g.xlarge`, `db.r6g.large`) to project monthly hosting costs.
- **4. Export to Infrastructure as Code (IaC):**
  - One-click export from ArchiSys canvas to **Terraform** configurations and **Kubernetes Helm Charts**.

### [Visual / Image Prompt]
> **Image Prompt for Slide:**  
> `A futuristic concept graphic of the GenAI feature: A chat prompt box typing 'Generate high-availability e-commerce system with 5000 TPS', radiating into an AI brain icon that automatically synthesizes and animates a full React Flow architecture with green metrics indicators. Deep space navy and neon purple/cyan color palette, 16:9 aspect ratio.`

### Speaker Notes / Script
> "Looking forward, the clean decoupling of FastAPI and our C++ engine positions ArchiSys for exciting future developments. Because FastAPI is in Python, we have established a dedicated AI module where Large Language Models can generate complete, validated architecture graphs directly from natural language prompts. Furthermore, we plan to add automatic bottleneck detection to recommend instance scaling and calculate estimated AWS and GCP monthly cloud costs directly from the canvas."
