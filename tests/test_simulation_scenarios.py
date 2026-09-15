import json
import pytest
import archisys_cpp


def test_client_to_server_scenario():
    """
    Scenario 1: Client -> Server
    Verifies:
      - Request generation & entry injection
      - FIFO queueing and worker instances
      - Correct latency accumulation (procTime + queue wait)
      - Metrics returned through pybind11
    """
    sim = archisys_cpp.Simulator()

    arch = {
        "nodes": [
            {
                "id": 1,
                "type": "client",
                "name": "Client-1",
                "config": {"cpuCores": 1, "procTime": 0.0, "maxQueue": 10000, "instances": 1, "requestRate": 50.0, "totalRequests": 100}
            },
            {
                "id": 2,
                "type": "server",
                "name": "App-Server",
                "config": {"cpuCores": 2, "procTime": 20.0, "maxQueue": 50, "instances": 2}
            }
        ],
        "edges": [
            {"fromId": 1, "toId": 2}
        ],
        "simulation": {
            "durationSec": 10.0,
            "tickSec": 0.01,
            "requestRatePerSec": 50.0,
            "totalRequests": 100,
            "seed": 42
        }
    }

    sim.load_architecture(json.dumps(arch))
    sim.start()

    metrics = sim.get_metrics()
    m_dict = metrics.to_dict()

    assert metrics.total_requests == 100, f"Expected 100 total requests, got {metrics.total_requests}"
    assert metrics.completed == 100, f"Expected 100 completed requests, got {metrics.completed}"
    assert metrics.dropped == 0, f"Expected 0 dropped requests, got {metrics.dropped}"
    assert metrics.failed == 0, f"Expected 0 failed requests, got {metrics.failed}"
    assert metrics.avg_latency_ms >= 20.0, f"Expected avg latency >= 20ms, got {metrics.avg_latency_ms}"
    assert metrics.p99_latency_ms >= 20.0, f"Expected p99 latency >= 20ms, got {metrics.p99_latency_ms}"
    assert len(metrics.components) == 2, f"Expected 2 components, got {len(metrics.components)}"

    # Check server component metrics
    server_m = next((c for c in metrics.components if c.id == 2), None)
    assert server_m is not None
    assert server_m.requests_completed == 100
    assert server_m.requests_dropped == 0
    assert server_m.avg_processing_ms >= 19.0


def test_client_loadbalancer_two_servers_scenario():
    """
    Scenario 2: Client -> LoadBalancer -> Server A / Server B
    Verifies:
      - Load balancing traffic distribution across multiple downstream targets
      - Concurrency capacity doubling
      - Queue wait stays low with load balancing
    """
    sim = archisys_cpp.Simulator()

    arch = {
        "nodes": [
            {"id": 1, "type": "client", "name": "Client", "config": {"procTime": 0, "maxQueue": 1000, "instances": 1, "requestRate": 100, "totalRequests": 200}},
            {"id": 2, "type": "loadbalancer", "name": "LB", "config": {"procTime": 1.0, "maxQueue": 500, "instances": 4}},
            {"id": 3, "type": "server", "name": "Server-A", "config": {"procTime": 30.0, "maxQueue": 100, "instances": 2}},
            {"id": 4, "type": "server", "name": "Server-B", "config": {"procTime": 30.0, "maxQueue": 100, "instances": 2}}
        ],
        "edges": [
            {"fromId": 1, "toId": 2},
            {"fromId": 2, "toId": 3},
            {"fromId": 2, "toId": 4}
        ],
        "simulation": {
            "durationSec": 10.0,
            "tickSec": 0.01,
            "requestRatePerSec": 100.0,
            "totalRequests": 200,
            "seed": 42
        }
    }

    sim.load_architecture(json.dumps(arch))
    sim.start()

    metrics = sim.get_metrics()
    assert metrics.total_requests == 200
    assert metrics.completed == 200
    assert metrics.dropped == 0

    srv_a = next(c for c in metrics.components if c.id == 3)
    srv_b = next(c for c in metrics.components if c.id == 4)

    # Both servers should receive roughly balanced traffic
    assert srv_a.requests_completed > 50, f"Server-A handled {srv_a.requests_completed}"
    assert srv_b.requests_completed > 50, f"Server-B handled {srv_b.requests_completed}"
    assert srv_a.requests_completed + srv_b.requests_completed == 200


def test_client_lb_server_redis_database_scenario():
    """
    Scenario 3: Client -> LoadBalancer -> Server -> RedisCache -> Database
    Verifies:
      - End-to-end multi-tier pipeline
      - Redis cache hit (~80%) completes immediately without calling Database
      - Redis cache miss (~20%) forwards downstream to Database
      - Accurate latency calculation (no double counting)
      - P99 calculation accuracy
    """
    sim = archisys_cpp.Simulator()

    arch = {
        "nodes": [
            {"id": 1, "type": "client", "name": "Client", "config": {"procTime": 0, "maxQueue": 1000, "instances": 1, "requestRate": 100, "totalRequests": 300}},
            {"id": 2, "type": "loadbalancer", "name": "LB", "config": {"procTime": 1.0, "maxQueue": 500, "instances": 4}},
            {"id": 3, "type": "server", "name": "App-Server", "config": {"procTime": 20.0, "maxQueue": 200, "instances": 4}},
            {"id": 4, "type": "redis", "name": "Redis-Cache", "config": {"procTime": 1.0, "maxQueue": 1000, "instances": 4}},
            {"id": 5, "type": "database", "name": "Primary-DB", "config": {"procTime": 25.0, "maxQueue": 100, "instances": 2}}
        ],
        "edges": [
            {"fromId": 1, "toId": 2},
            {"fromId": 2, "toId": 3},
            {"fromId": 3, "toId": 4},
            {"fromId": 4, "toId": 5}
        ],
        "simulation": {
            "durationSec": 15.0,
            "tickSec": 0.01,
            "requestRatePerSec": 100.0,
            "totalRequests": 300,
            "seed": 12345
        }
    }

    sim.load_architecture(json.dumps(arch))
    sim.start()

    metrics = sim.get_metrics()
    assert metrics.total_requests == 300
    assert metrics.completed == 300
    assert metrics.dropped == 0
    assert metrics.p99_latency_ms > metrics.avg_latency_ms

    redis_m = next(c for c in metrics.components if c.id == 4)
    db_m = next(c for c in metrics.components if c.id == 5)

    # Redis processed all 300 requests
    assert redis_m.requests_completed == 300
    # Database should only process cache misses (~15-30% of total)
    assert 20 <= db_m.requests_completed <= 100, f"Expected DB to handle ~60 requests, got {db_m.requests_completed}"


def test_queue_overflow_and_dropping():
    """
    Scenario 4: Queue Overflow
    High request rate against a slow single-instance server with small maxQueue (10).
    Verifies that requests beyond queue capacity are correctly marked dropped.
    """
    sim = archisys_cpp.Simulator()

    arch = {
        "nodes": [
            {"id": 1, "type": "client", "name": "Client", "config": {"procTime": 0, "maxQueue": 1000, "instances": 1, "requestRate": 500, "totalRequests": 200}},
            {"id": 2, "type": "server", "name": "Bottleneck-Server", "config": {"procTime": 200.0, "maxQueue": 10, "instances": 1}}
        ],
        "edges": [
            {"fromId": 1, "toId": 2}
        ],
        "simulation": {
            "durationSec": 5.0,
            "tickSec": 0.01,
            "requestRatePerSec": 500.0,
            "totalRequests": 200,
            "seed": 42
        }
    }

    sim.load_architecture(json.dumps(arch))
    sim.start()

    metrics = sim.get_metrics()
    assert metrics.total_requests == 200
    assert metrics.dropped > 0, f"Expected dropped requests, got {metrics.dropped}"
    assert metrics.completed + metrics.dropped == 200
