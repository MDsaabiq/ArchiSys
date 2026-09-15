import json
import logging
from typing import List, Any
from fastapi import WebSocket

logger = logging.getLogger("archisys.ws")


class WebSocketManager:
    """
    Manages connected WebSocket clients and delivers real-time
    simulation metrics broadcasts to the React UI.
    """

    def __init__(self):
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)
        logger.info(f"WebSocket client connected. Active clients: {len(self.active_connections)}")

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
            logger.info(f"WebSocket client disconnected. Active clients: {len(self.active_connections)}")

    async def broadcast(self, data: Any):
        if not self.active_connections:
            return

        if isinstance(data, (dict, list)):
            message = json.dumps(data)
        else:
            message = str(data)

        stale: List[WebSocket] = []
        for connection in self.active_connections:
            try:
                await connection.send_text(message)
            except Exception as ex:
                logger.debug(f"Failed to send to WebSocket client: {ex}")
                stale.append(connection)

        for conn in stale:
            self.disconnect(conn)

    @property
    def has_clients(self) -> bool:
        return len(self.active_connections) > 0


# Global singleton manager instance
ws_manager = WebSocketManager()
