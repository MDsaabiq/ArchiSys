# ArchiSys GenAI Architecture Module

This module is designed for upcoming Generative AI features in ArchiSys.

## Architecture Flow

```
User Prompt (e.g. "Design a high-availability URL shortener with caching")
  │
  ▼
FastAPI AI Router (`POST /api/ai/generate-architecture`)
  │
  ▼
LLM Orchestrator (Structured Outputs via Pydantic schema)
  │
  ▼
ArchitecturePayload (Validated nodes & edges JSON)
  │
  ▼
C++ Simulation Engine (In-process validation & capacity simulation)
  │
  ▼
React Flow (Interactive diagram visualization + Live telemetry streaming)
```
