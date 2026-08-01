# ArchiSys — Interactive System Design Simulator

## What is this?

ArchiSys lets you visually design distributed systems by dragging and connecting components, then simulates how requests travel through the architecture in real time.

```
React + React Flow frontend
        │
        │  POST /start (architecture JSON)
        │  WebSocket /ws (live metrics stream)
        │
C++ Simulation Engine (8765)
        │
        └── Real request objects, event queue, component graph
```

---

## Project structure

```
archisys/
├── frontend/          ← React + React Flow + Zustand (TypeScript)
├── backend/           ← C++ simulation engine + WebSocket API server
│   ├── include/       ← Headers (component, request, engine, ws)
│   ├── src/           ← Implementation
│   └── CMakeLists.txt
└── mockups/           ← Standalone HTML prototype (reference only)
```

---

## How to run

### Step 1 — Build the C++ server

You need a C++ compiler: **MSVC** (Visual Studio Build Tools) or **MinGW** (g++).

```powershell
# Install Visual Studio Build Tools (if not already installed)
winget install Microsoft.VisualStudio.2022.BuildTools

# Or MinGW
winget install MinGW.MinGW
```

Build:

```powershell
cd backend

# MSVC (from Developer PowerShell)
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --target archisys_server --config Release
.\build\Release\archisys_server.exe

# MinGW
cmake -B build -G "MinGW Makefiles"
cmake --build build --target archisys_server
.\build\archisys_server.exe
```

The server starts on **http://localhost:8765**.

### Step 2 — Start the React frontend

```powershell
cd frontend
npm install      # already done if you cloned the repo
npm run dev      # opens http://localhost:5173
```

### Step 3 — Use the app

1. Open http://localhost:5173
2. Drag components from the left sidebar onto the canvas
3. Connect components by dragging from one port to another
4. Click a component to edit its properties (right panel)
5. Click **▶ Run Simulation**
   - Architecture JSON is sent to the C++ server
   - Simulation starts
   - Live metrics stream back via WebSocket
   - Node cards update in real time (CPU %, queue depth, latency)
   - Packets animate along edges
6. Click **■ Stop** to end the simulation
7. Click **↓ Export JSON** to save your architecture

---

## API contract

### POST /start

Send architecture JSON:

```json
{
  "nodes": [
    { "id": 1, "type": "loadbalancer", "name": "LB-1", "x": 100, "y": 100,
      "config": { "cpuCores": 4, "procTime": 2, "maxQueue": 500, "instances": 4 } },
    { "id": 2, "type": "server", "name": "App Server", "x": 320, "y": 100,
      "config": { "cpuCores": 4, "procTime": 50, "maxQueue": 200, "instances": 2 } }
  ],
  "edges": [
    { "id": 1, "fromId": 1, "toId": 2 }
  ],
  "simulation": {
    "durationSec": 3600,
    "tickSec": 0.01,
    "requestRatePerSec": 200,
    "seed": 42
  }
}
```

Node types: `server`, `database`, `loadbalancer`, `redis`, `queue`

### WebSocket /ws — live metrics stream (every 100ms)

```json
{
  "type": "metrics",
  "simTimeSec": 4.23,
  "totalRequests": 847,
  "completed": 801,
  "failed": 0,
  "dropped": 46,
  "inFlight": 23,
  "avgLatencyMs": 68.4,
  "throughputPerSec": 189.4,
  "components": [
    {
      "id": 1, "name": "LB-1", "type": "loadbalancer",
      "cpuUsagePct": 12.3, "queueDepth": 0, "maxQueue": 500,
      "requestsReceived": 847, "requestsCompleted": 847, "requestsDropped": 0,
      "avgProcessingMs": 2.1, "avgQueueWaitMs": 0.0, "throughputPerSec": 200.1
    }
  ]
}
```

---

## CLI tool (no frontend)

```powershell
# Built-in demo: LB → Server A/B → Redis → Database
.\build\archisys_sim.exe

# Load your exported architecture.json
.\build\archisys_sim.exe ..\architecture.json
```

Prints live metrics every 0.5s and a final report.
