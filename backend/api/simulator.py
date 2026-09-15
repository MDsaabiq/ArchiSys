import asyncio
import logging
import json
from typing import Optional, Dict, Any
from .models import ArchitecturePayload
from .websocket_manager import ws_manager

try:
    import archisys_cpp
except ImportError:
    import sys
    from pathlib import Path
    build_dir = Path(__file__).resolve().parent.parent / "engine" / "build"
    if build_dir.exists():
        sys.path.insert(0, str(build_dir))
    import archisys_cpp

logger = logging.getLogger("archisys.simulation")


class SimulationService:
    """
    Orchestration service connecting FastAPI to the in-process C++ Simulation Engine (archisys_cpp).
    Manages non-blocking background simulation loops and delivers live metrics snapshots to WebSockets.
    """

    def __init__(self):
        self._simulator = archisys_cpp.Simulator()
        self._sim_task: Optional[asyncio.Task] = None
        self._is_active: bool = False

    def is_running(self) -> bool:
        return self._is_active or self._simulator.is_running()

    def get_metrics(self) -> Dict[str, Any]:
        metrics = self._simulator.get_metrics()
        return metrics.to_dict()

    async def start(self, payload: ArchitecturePayload):
        """
        Loads the architecture JSON into the C++ engine and starts the background execution loop.
        """
        await self.stop()

        json_str = payload.model_dump_json()
        logger.info(f"Loading architecture into C++ engine ({len(payload.nodes)} nodes, {len(payload.edges)} edges)")

        self._simulator.reset()
        self._simulator.load_architecture(json_str)
        self._is_active = True

        sim_config = payload.simulation or ArchitecturePayload().simulation
        duration_sec = sim_config.durationSec
        tick_sec = max(0.005, sim_config.tickSec)
        total_requests = sim_config.totalRequests

        self._sim_task = asyncio.create_task(
            self._run_simulation_loop(duration_sec, tick_sec, total_requests)
        )

    async def stop(self):
        """
        Stops the running simulation and cleans up background tasks.
        """
        self._is_active = False
        if self._simulator:
            self._simulator.stop()

        if self._sim_task and not self._sim_task.done():
            self._sim_task.cancel()
            try:
                await self._sim_task
            except asyncio.CancelledError:
                pass
            self._sim_task = None

        logger.info("Simulation stopped")

    async def _run_simulation_loop(self, duration_sec: float, tick_sec: float, total_requests: int):
        """
        Background loop executing C++ discrete simulation steps and broadcasting metrics.
        Runs smoothly in the background without blocking FastAPI HTTP or WebSocket requests.
        """
        logger.info(f"Simulation execution started (duration={duration_sec}s, tick={tick_sec}s, totalRequests={total_requests})")
        elapsed_sim_time = 0.0
        broadcast_interval = 0.05  # Broadcast metrics every 50ms (20 updates/sec)
        time_since_broadcast = 0.0

        try:
            while self._is_active and elapsed_sim_time <= duration_sec:
                batch_ticks = 5
                step_dt = tick_sec * batch_ticks

                # C++ step execution (GIL is released inside pybind11 step() binding)
                self._simulator.step(step_dt)
                elapsed_sim_time += step_dt
                time_since_broadcast += step_dt

                metrics_obj = self._simulator.get_metrics()
                metrics_dict = metrics_obj.to_dict()

                if total_requests > 0:
                    total_done = metrics_obj.completed + metrics_obj.dropped + metrics_obj.failed
                    if total_done >= total_requests and metrics_obj.in_flight == 0:
                        logger.info(f"Target request count reached ({total_done}/{total_requests}). Concluding simulation.")
                        await ws_manager.broadcast(metrics_dict)
                        break

                if time_since_broadcast >= broadcast_interval:
                    await ws_manager.broadcast(metrics_dict)
                    time_since_broadcast = 0.0

                await asyncio.sleep(0.01)

            final_metrics = self._simulator.get_metrics().to_dict()
            await ws_manager.broadcast(final_metrics)

        except asyncio.CancelledError:
            logger.info("Simulation loop cancelled.")
        except Exception as ex:
            logger.error(f"Error in simulation loop: {ex}", exc_info=True)
        finally:
            self._is_active = False
            self._simulator.stop()
            logger.info("Simulation loop concluded.")


# Global service instance
simulation_service = SimulationService()
