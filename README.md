# ArchiSys — Interactive Distributed Systems Simulator

ArchiSys lets you visually design distributed systems by dragging and connecting components on an interactive canvas, then simulates request traffic, queueing dynamics, CPU saturation, caching, and latency profiling in real time.

$$\text{React (Frontend)} \xrightarrow[\text{POST /start, /stop, /health}]{\text{HTTP + WS}} \text{FastAPI (Python Orchestration)} \xrightarrow[\text{in-process calls}]{\text{pybind11}} \text{C++ Simulation Engine} \xrightarrow{\text{metrics}} \text{FastAPI WebSocket} \xrightarrow{\text{20 FPS Stream}} \text{React}$$

---

## Project Structure

```
archisys/
├── backend/                       # Unified Backend
│   ├── api/                       # Python FastAPI Application Layer
│   │   ├── ai/                    # GenAI Orchestration Blueprint
│   │   ├── main.py                # FastAPI app & endpoint routing
│   │   ├── models.py              # Pydantic request/response schemas
│   │   ├── simulator.py           # SimulationService Python wrapper
│   │   ├── websocket_manager.py   # WebSocket client connection broadcaster
│   │   └── __init__.py
│   ├── engine/                    # Native C++ Simulation Compute Core
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
├── frontend/                      # React + React Flow + Zustand (TypeScript)
├── tests/                         # Deterministic Pytest Test Suite
├── docs/                          # Architecture & Simulation Documentation
│   ├── architecture.md
│   ├── simulation.md
│   ├── cpp-python-integration.md
│   └── api.md
├── pytest.ini                     # Pytest environment configuration
└── README.md
```

---

## Developer Quickstart

### Step 1 — Python Environment Setup
```powershell
python -m venv .venv
.\.venv\Scripts\pip install -r backend/requirements.txt
```

### Step 2 — Build C++ Native Extension (pybind11)
```powershell
cmake -B backend/engine/build -S backend/engine -G "MinGW Makefiles" `
  -DPython_EXECUTABLE="$PWD/.venv/Scripts/python.exe" `
  -Dpybind11_DIR="$PWD/.venv/Lib/site-packages/pybind11/share/cmake/pybind11"

cmake --build backend/engine/build --config Release

# Copy compiled extension to site-packages for global venv import
Copy-Item "backend/engine/build/archisys_cpp.*.pyd" -Destination ".venv/Lib/site-packages/archisys_cpp.pyd" -Force
```

### Step 3 — Run the Test Suite
```powershell
.\.venv\Scripts\pytest -v tests/
```

### Step 4 — Start the FastAPI Backend
```powershell
.\.venv\Scripts\uvicorn backend.api.main:app --host 0.0.0.0 --port 8000 --reload
```
API endpoints available at `http://localhost:8000` (`/health`, `/start`, `/stop`, `/metrics`) and `ws://localhost:8000/ws`.

### Step 5 — Start the React Frontend
```powershell
cd frontend
npm run dev
```
Open [http://localhost:5173](http://localhost:5173) in your browser.

---

## Architecture Documentation

For complete technical specifications, mathematical definitions, and API schemas:
- [`docs/architecture.md`](docs/architecture.md) — System architecture and design decisions
- [`docs/simulation.md`](docs/simulation.md) — Discrete-tick simulation engine, queuing, and metric formulas
- [`docs/cpp-python-integration.md`](docs/cpp-python-integration.md) — pybind11 in-process integration and GIL handling
- [`docs/api.md`](docs/api.md) — REST & WebSocket API specification
