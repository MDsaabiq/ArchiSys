import logging
from contextlib import asynccontextmanager
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.middleware.cors import CORSMiddleware

from .models import (
    ArchitecturePayload,
    StartResponse,
    StopResponse,
    HealthResponse,
    SystemMetricsResponse
)
from .websocket_manager import ws_manager
from .simulator import simulation_service

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s"
)
logger = logging.getLogger("archisys.api")


@asynccontextmanager
async def lifespan(app: FastAPI):
    logger.info("ArchiSys FastAPI Orchestration Backend initialized.")
    yield
    logger.info("Shutting down ArchiSys FastAPI backend...")
    await simulation_service.stop()


app = FastAPI(
    title="ArchiSys Simulation Orchestration API",
    description="FastAPI orchestration layer interfacing with the in-process C++ simulation engine",
    version="2.0.0",
    lifespan=lifespan
)

# Enable CORS for the React frontend
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/health", response_model=HealthResponse)
async def health_check():
    """
    Health check endpoint for the FastAPI server and C++ simulation engine.
    """
    return HealthResponse(status="ok", ok=True)


@app.post("/start", response_model=StartResponse)
async def start_simulation(payload: ArchitecturePayload):
    """
    Validates the architecture graph payload and initiates simulation execution in the C++ engine.
    """
    if not payload.nodes:
        raise HTTPException(status_code=400, detail="Architecture must contain at least one node")

    try:
        await simulation_service.start(payload)
        return StartResponse(status="started", started=True)
    except Exception as ex:
        logger.error(f"Failed to start simulation: {ex}", exc_info=True)
        raise HTTPException(status_code=500, detail=str(ex))


@app.post("/stop", response_model=StopResponse)
async def stop_simulation():
    """
    Stops the active simulation in the C++ engine.
    """
    try:
        await simulation_service.stop()
        return StopResponse(status="stopped", stopped=True)
    except Exception as ex:
        logger.error(f"Failed to stop simulation: {ex}", exc_info=True)
        raise HTTPException(status_code=500, detail=str(ex))


@app.get("/metrics")
async def get_current_metrics():
    """
    Returns the latest live metrics snapshot from the C++ engine.
    """
    return simulation_service.get_metrics()


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """
    WebSocket endpoint streaming real-time simulation metrics to connected React frontend clients.
    """
    await ws_manager.connect(websocket)
    try:
        while True:
            data = await websocket.receive_text()
            if data == "ping":
                await websocket.send_text("pong")
    except WebSocketDisconnect:
        ws_manager.disconnect(websocket)
    except Exception as ex:
        logger.debug(f"WebSocket client connection closed: {ex}")
        ws_manager.disconnect(websocket)


if __name__ == "__main__":
    import uvicorn
    uvicorn.run("backend.api.main:app", host="0.0.0.0", port=8000, reload=True)
