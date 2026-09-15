from typing import List, Optional, Literal
from pydantic import BaseModel, Field

ComponentType = Literal['client', 'server', 'database', 'loadbalancer', 'redis', 'queue']


class NodeConfig(BaseModel):
    cpuCores: int = Field(default=1, description="Allocated CPU cores")
    procTime: float = Field(default=50.0, description="Base processing time (ms)")
    maxQueue: int = Field(default=100, description="Max waiting queue capacity")
    instances: int = Field(default=1, description="Parallel worker instances")
    # Client-specific traffic settings
    requestRate: Optional[float] = Field(default=None, description="Requests injected per second")
    totalRequests: Optional[int] = Field(default=None, description="Total requests before stopping (0 = unlimited)")


class ArchNode(BaseModel):
    id: int
    type: ComponentType
    name: Optional[str] = None
    x: Optional[float] = 0.0
    y: Optional[float] = 0.0
    config: NodeConfig = Field(default_factory=NodeConfig)


class ArchEdge(BaseModel):
    id: Optional[int] = None
    fromId: int
    toId: int


class SimulationConfig(BaseModel):
    durationSec: float = Field(default=60.0, description="Simulation run duration limit in seconds")
    tickSec: float = Field(default=0.01, description="Discrete simulation time step per tick (seconds)")
    requestRatePerSec: float = Field(default=100.0, description="Default traffic generation rate")
    totalRequests: int = Field(default=0, description="Max requests to inject (0 = unlimited)")
    seed: int = Field(default=42, description="Random seed for deterministic runs")


class ArchitecturePayload(BaseModel):
    nodes: List[ArchNode] = Field(default_factory=list, description="Architecture components")
    edges: List[ArchEdge] = Field(default_factory=list, description="Directed connections between nodes")
    simulation: Optional[SimulationConfig] = Field(default_factory=SimulationConfig, description="Global simulation settings")


class ComponentMetricsModel(BaseModel):
    id: int
    name: str
    type: str
    cpuUsagePct: float
    queueDepth: int
    maxQueue: int
    requestsReceived: int
    requestsCompleted: int
    requestsDropped: int
    avgProcessingMs: float
    avgQueueWaitMs: float
    throughputPerSec: float


class SystemMetricsResponse(BaseModel):
    type: str = "metrics"
    simTimeSec: float
    totalRequests: int
    completed: int
    failed: int
    dropped: int
    inFlight: int
    avgLatencyMs: float
    p99LatencyMs: float
    throughputPerSec: float
    components: List[ComponentMetricsModel] = Field(default_factory=list)


class StartResponse(BaseModel):
    status: str = "started"
    started: bool = True


class StopResponse(BaseModel):
    status: str = "stopped"
    stopped: bool = True


class HealthResponse(BaseModel):
    status: str = "ok"
    ok: bool = True
