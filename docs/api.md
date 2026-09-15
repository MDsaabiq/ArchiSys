# ArchiSys API Reference

## Base URLs

- **REST API**: `http://localhost:8000`
- **WebSocket**: `ws://localhost:8000/ws`

---

## REST Endpoints

### 1. Health Check
Checks if the FastAPI server and simulation orchestrator are active.

- **Method**: `GET /health`
- **Response**: `200 OK`
```json
{
  "status": "ok",
  "ok": true
}
```

---

### 2. Start Simulation
Validates the architecture graph and starts the C++ simulation in a non-blocking background context.

- **Method**: `POST /start`
- **Request Headers**: `Content-Type: application/json`
- **Request Body**:
```json
{
  "nodes": [
    {
      "id": 1,
      "type": "client",
      "name": "Client",
      "config": {
        "cpuCores": 1,
        "procTime": 0,
        "maxQueue": 10000,
        "instances": 1,
        "requestRate": 100,
        "totalRequests": 500
      }
    },
    {
      "id": 2,
      "type": "server",
      "name": "App Server",
      "config": {
        "cpuCores": 4,
        "procTime": 25,
        "maxQueue": 200,
        "instances": 2
      }
    }
  ],
  "edges": [
    {
      "fromId": 1,
      "toId": 2
    }
  ],
  "simulation": {
    "durationSec": 60.0,
    "tickSec": 0.01,
    "requestRatePerSec": 100.0,
    "totalRequests": 500,
    "seed": 42
  }
}
```
- **Response**: `200 OK`
```json
{
  "status": "started",
  "started": true
}
```

---

### 3. Stop Simulation
Immediately halts any currently running simulation in the C++ engine.

- **Method**: `POST /stop`
- **Response**: `200 OK`
```json
{
  "status": "stopped",
  "stopped": true
}
```

---

### 4. Get Current Metrics (Snapshot)
Returns the latest telemetry metrics snapshot from the C++ engine.

- **Method**: `GET /metrics`
- **Response**: `200 OK`
```json
{
  "type": "metrics",
  "simTimeSec": 1.25,
  "totalRequests": 125,
  "completed": 100,
  "failed": 0,
  "dropped": 0,
  "inFlight": 25,
  "avgLatencyMs": 26.5,
  "p99LatencyMs": 42.0,
  "throughputPerSec": 80.0,
  "components": [
    {
      "id": 2,
      "name": "App Server",
      "type": "server",
      "cpuUsagePct": 62.5,
      "queueDepth": 5,
      "maxQueue": 200,
      "requestsReceived": 125,
      "requestsCompleted": 100,
      "requestsDropped": 0,
      "avgProcessingMs": 25.0,
      "avgQueueWaitMs": 1.5,
      "throughputPerSec": 80.0
    }
  ]
}
```

---

## WebSocket Telemetry Stream

- **URL**: `ws://localhost:8000/ws`
- **Protocol**: JSON text messages pushed at ~20 FPS.
- **Message Format**:
```json
{
  "type": "metrics",
  "simTimeSec": 2.5,
  "totalRequests": 250,
  "completed": 200,
  "failed": 0,
  "dropped": 0,
  "inFlight": 50,
  "avgLatencyMs": 31.2,
  "p99LatencyMs": 55.0,
  "throughputPerSec": 80.0,
  "components": [ ... ]
}
```
