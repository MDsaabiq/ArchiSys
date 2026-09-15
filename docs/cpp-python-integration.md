# C++ and Python Integration Guide (pybind11)

## Overview

ArchiSys achieves high performance by embedding the compiled C++ Simulation Engine directly within the FastAPI Python process using **pybind11**.

```
FastAPI Python Process (PID: 1234)
├── Python Event Loop (asyncio)
│   ├── REST Routes (/start, /stop, /health)
│   └── WebSocket Manager (/ws)
│
└── pybind11 Native Extension Boundary
    └── import archisys_cpp
        ├── C++ Simulator Class (In-Memory Facade)
        ├── C++ SimulationEngine
        └── C++ Components (Server, DB, Cache, LB, Queue)
```

> [!IMPORTANT]
> - There is **no subprocess execution**, **no socket IPC**, and **no stdin/stdout serialization** between Python and C++.
> - C++ is compiled into a native Python extension (`archisys_cpp.pyd` on Windows / `archisys_cpp.so` on Linux).
> - Python interacts with C++ via direct in-memory function calls.

---

## Directory Layout

The backend is unified under `backend/` with two distinct tiers:
- `backend/api/`: Python FastAPI routing, WebSocket streaming, and background orchestration.
- `backend/engine/`: C++ discrete simulation compute engine and pybind11 native bindings.

---

## The pybind11 Interface

The binding layer in [`backend/engine/src/bindings.cpp`](file:///c:/Users/saabi/OneDrive/Documents/code/projects/archisys/backend/engine/src/bindings.cpp) exposes a minimal facade:

```cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "simulator.hpp"

namespace py = pybind11;

PYBIND11_MODULE(archisys_cpp, m) {
    py::class_<archisys::Simulator>(m, "Simulator")
        .def(py::init<>())
        .def("load_architecture", &archisys::Simulator::loadArchitecture)
        .def("start",             &archisys::Simulator::start, py::call_guard<py::gil_scoped_release>())
        .def("step",              &archisys::Simulator::step,  py::call_guard<py::gil_scoped_release>())
        .def("stop",              &archisys::Simulator::stop)
        .def("is_running",        &archisys::Simulator::isRunning)
        .def("get_metrics",       &archisys::Simulator::getMetrics)
        .def("reset",             &archisys::Simulator::reset);
}
```

### Non-Blocking Concurrency & GIL Release
Long-running C++ operations like `start()` and `step()` are wrapped with `py::call_guard<py::gil_scoped_release>()`.
When Python calls into C++, the Global Interpreter Lock (GIL) is released for the duration of the C++ computation. This ensures that:
1. FastAPI remains responsive to incoming HTTP requests (`/stop`, `/health`).
2. WebSocket telemetry frames stream without latency spikes.
3. The event loop is not blocked by heavy simulation loops.

---

## Python Usage Example

```python
import archisys_cpp

# 1. Initialize simulator facade
sim = archisys_cpp.Simulator()

# 2. Load architecture JSON payload
sim.load_architecture('{"nodes": [...], "edges": [...]}')

# 3. Advance simulation by discrete steps (e.g. 50ms)
sim.step(0.05)

# 4. Extract telemetry snapshot directly as Python objects or dictionaries
metrics = sim.get_metrics()
print(f"Simulation Time: {metrics.sim_time_sec}s")
print(f"Throughput: {metrics.throughput_per_sec} req/s")
print(f"P99 Latency: {metrics.p99_latency_ms} ms")

# Or export as JSON-serializable dictionary
metrics_dict = metrics.to_dict()
```

---

## Build and Compilation Commands

### 1. Build Native Extension with CMake

```powershell
# Configure build with Python & pybind11
cmake -B backend/engine/build -S backend/engine -G "MinGW Makefiles" `
  -DPython_EXECUTABLE="$PWD/.venv/Scripts/python.exe" `
  -Dpybind11_DIR="$PWD/.venv/Lib/site-packages/pybind11/share/cmake/pybind11"

# Compile Release binary
cmake --build backend/engine/build --config Release

# Copy binary to site-packages for global venv import
Copy-Item "backend/engine/build/archisys_cpp.*.pyd" -Destination ".venv/Lib/site-packages/archisys_cpp.pyd" -Force
```

### 2. Output Artifacts

- Windows: `backend/engine/build/archisys_cpp.cp313-win_amd64.pyd`
- Linux/macOS: `backend/engine/build/archisys_cpp.so`

The compiled `.pyd` is linked statically (`-static-libgcc -static-libstdc++ -static`) ensuring zero external MinGW DLL dependencies.
