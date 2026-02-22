"""
FastAPI bridge server.

Connects to the C++ OrderbookGC server via TCP and exposes:
  - WebSocket  /ws           → pushes all server messages as JSON to browsers
  - POST       /api/order    → submit an order
  - DELETE     /api/order/{id} → cancel an order
  - GET        /api/status   → connection status
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import uuid
from contextlib import asynccontextmanager
from typing import Dict, Any, Set

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

from tcp_client import TcpClient
from models import OrderSubmit, OrderCancel, OrderSide, OrderType

logger = logging.getLogger("bridge")
logging.basicConfig(level=logging.INFO,
                    format="%(asctime)s [%(name)s] %(levelname)s: %(message)s")

# ── Global state ─────────────────────────────────────────────

tcp_client: TcpClient | None = None
ws_clients: Set[WebSocket] = set()
client_id: str = f"gui_{uuid.uuid4().hex[:12]}"
symbols: list[str] = ["AAPL"]

# Server connection config (set from CLI args)
server_host = "127.0.0.1"
server_port = 8080


# ── Callbacks from TCP client ────────────────────────────────

def on_server_message(msg: Dict[str, Any]):
    """Broadcast every message from the C++ server to all browser WS clients."""
    text = json.dumps(msg)
    stale: list[WebSocket] = []
    for ws in ws_clients:
        try:
            asyncio.ensure_future(ws.send_text(text))
        except Exception:
            stale.append(ws)
    for ws in stale:
        ws_clients.discard(ws)


def on_connect():
    """When TCP connects, request snapshots for all configured symbols."""
    logger.info("TCP connected — requesting snapshots for %s", symbols)
    if tcp_client:
        for sym in symbols:
            asyncio.ensure_future(tcp_client.request_snapshot(sym))


def on_disconnect():
    """Notify browser clients of TCP disconnect."""
    msg = json.dumps({"type": "CONNECTION_STATUS", "connected": False})
    for ws in list(ws_clients):
        try:
            asyncio.ensure_future(ws.send_text(msg))
        except Exception:
            pass


# ── FastAPI lifespan ─────────────────────────────────────────

@asynccontextmanager
async def lifespan(app: FastAPI):
    global tcp_client
    tcp_client = TcpClient(
        host=server_host,
        port=server_port,
        on_message=on_server_message,
        on_connect=on_connect,
        on_disconnect=on_disconnect,
    )
    await tcp_client.start()
    yield
    await tcp_client.stop()


app = FastAPI(title="OrderbookGC Bridge", lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


# ── WebSocket endpoint ───────────────────────────────────────

@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()
    ws_clients.add(ws)
    logger.info("Browser WS connected (%d total)", len(ws_clients))

    # Send current connection status
    await ws.send_text(json.dumps({
        "type": "CONNECTION_STATUS",
        "connected": tcp_client.connected if tcp_client else False,
    }))

    try:
        while True:
            # Keep alive — browser can send commands here too
            data = await ws.receive_text()
            try:
                cmd = json.loads(data)
                await _handle_ws_command(cmd)
            except json.JSONDecodeError:
                pass
    except WebSocketDisconnect:
        pass
    finally:
        ws_clients.discard(ws)
        logger.info("Browser WS disconnected (%d remaining)", len(ws_clients))


async def _handle_ws_command(cmd: dict):
    """Handle commands sent from browser over WebSocket."""
    action = cmd.get("action")
    if action in ("submit_order", "submit") and tcp_client:
        order_type_str = cmd.get("order_type", "LIMIT").upper()
        order_type = OrderType(order_type_str) if order_type_str in ("LIMIT", "MARKET") else OrderType.LIMIT
        msg = OrderSubmit(
            client_id=client_id,
            symbol=cmd.get("symbol", "AAPL"),
            side=OrderSide(cmd.get("side", "BUY")),
            order_type=order_type,
            price=float(cmd.get("price", 0)),
            quantity=int(cmd.get("quantity", 0)),
        )
        logger.info("Submitting %s order: %s %s %s @ %.2f x %d",
                     msg.order_type.value, msg.client_id, msg.side.value,
                     msg.symbol, msg.price, msg.quantity)
        await tcp_client.submit_order(msg)
    elif action in ("cancel_order", "cancel") and tcp_client:
        msg = OrderCancel(
            order_id=cmd.get("order_id", ""),
            client_id=client_id,
        )
        logger.info("Cancelling order: %s", msg.order_id)
        await tcp_client.cancel_order(msg)
    elif action in ("request_snapshot", "snapshot") and tcp_client:
        await tcp_client.request_snapshot(cmd.get("symbol", "AAPL"))
    else:
        logger.warning("Unknown WS command action: %s", action)


# ── REST endpoints ───────────────────────────────────────────

class OrderRequest(BaseModel):
    symbol: str = "AAPL"
    side: str = "BUY"
    order_type: str = "LIMIT"
    price: float = 0.0
    quantity: int


@app.post("/api/order")
async def submit_order(req: OrderRequest):
    if not tcp_client or not tcp_client.connected:
        return {"error": "Not connected to server"}
    msg = OrderSubmit(
        client_id=client_id,
        symbol=req.symbol,
        side=OrderSide(req.side),
        order_type=OrderType(req.order_type.upper()) if req.order_type.upper() in ("LIMIT", "MARKET") else OrderType.LIMIT,
        price=req.price,
        quantity=req.quantity,
    )
    await tcp_client.submit_order(msg)
    return {"status": "submitted", "client_id": client_id}


@app.delete("/api/order/{order_id}")
async def cancel_order(order_id: str):
    if not tcp_client or not tcp_client.connected:
        return {"error": "Not connected to server"}
    msg = OrderCancel(order_id=order_id, client_id=client_id)
    await tcp_client.cancel_order(msg)
    return {"status": "cancel_requested", "order_id": order_id}


@app.get("/api/status")
async def get_status():
    return {
        "connected": tcp_client.connected if tcp_client else False,
        "client_id": client_id,
        "symbols": symbols,
        "server": f"{server_host}:{server_port}",
    }


# ── CLI ──────────────────────────────────────────────────────

def main():
    global server_host, server_port, symbols

    parser = argparse.ArgumentParser(description="OrderbookGC Python Bridge")
    parser.add_argument("--server-host", default="127.0.0.1")
    parser.add_argument("--server-port", type=int, default=8080)
    parser.add_argument("--listen-port", type=int, default=3001)
    parser.add_argument("--symbols", default="AAPL",
                        help="Comma-separated symbols to subscribe to")
    args = parser.parse_args()

    server_host = args.server_host
    server_port = args.server_port
    symbols = [s.strip() for s in args.symbols.split(",") if s.strip()]

    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=args.listen_port, log_level="info")


if __name__ == "__main__":
    main()
