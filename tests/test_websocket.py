import pytest
from fastapi.testclient import TestClient
from backend.api.main import app

client = TestClient(app)


def test_websocket_stream():
    with client.websocket_connect("/ws") as websocket:
        websocket.send_text("ping")
        response = websocket.receive_text()
        assert response == "pong"
