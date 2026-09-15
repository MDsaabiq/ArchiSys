import pytest
from fastapi.testclient import TestClient
from backend.api.main import app

client = TestClient(app)


def test_health_check_endpoint():
    response = client.get("/health")
    assert response.status_code == 200
    data = response.json()
    assert data["status"] == "ok"
    assert data["ok"] is True


def test_start_and_stop_endpoints():
    payload = {
        "nodes": [
            {
                "id": 1,
                "type": "client",
                "name": "Client",
                "config": {"cpuCores": 1, "procTime": 0, "maxQueue": 1000, "instances": 1, "requestRate": 100, "totalRequests": 100}
            },
            {
                "id": 2,
                "type": "server",
                "name": "Server",
                "config": {"cpuCores": 2, "procTime": 10, "maxQueue": 100, "instances": 2}
            }
        ],
        "edges": [
            {"fromId": 1, "toId": 2}
        ],
        "simulation": {
            "durationSec": 5.0,
            "tickSec": 0.01,
            "requestRatePerSec": 100.0,
            "totalRequests": 100,
            "seed": 42
        }
    }

    start_res = client.post("/start", json=payload)
    assert start_res.status_code == 200
    assert start_res.json()["status"] == "started"

    metrics_res = client.get("/metrics")
    assert metrics_res.status_code == 200
    assert "simTimeSec" in metrics_res.json()

    stop_res = client.post("/stop")
    assert stop_res.status_code == 200
    assert stop_res.json()["status"] == "stopped"


def test_start_endpoint_empty_nodes_validation():
    invalid_payload = {
        "nodes": [],
        "edges": []
    }
    res = client.post("/start", json=invalid_payload)
    assert res.status_code == 400
