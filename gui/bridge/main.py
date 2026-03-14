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
from pnl_tracker import PnLTracker

logger = logging.getLogger("bridge")
logging.basicConfig(level=logging.INFO,
                    format="%(asctime)s [%(name)s] %(levelname)s: %(message)s")

# ── Global state ─────────────────────────────────────────────

tcp_client: TcpClient | None = None
ws_clients: Set[WebSocket] = set()
client_id: str = f"gui_{uuid.uuid4().hex[:12]}"
symbols: list[str] = ["AAPL"]
pnl_tracker: PnLTracker = PnLTracker()
# Keep implementation but disable runtime PnL paths to reduce message overhead.
ENABLE_PNL_DASHBOARD = False

# Server connection config (set from CLI args)
server_host = "127.0.0.1"
server_port = 8080
server_username = ""
server_password = ""


# ── Callbacks from TCP client ────────────────────────────────

async def _safe_send(ws: WebSocket, text: str):
    """Send text to a WebSocket, returning False if the client is stale."""
    try:
        await ws.send_text(text)
        return True
    except Exception:
        return False


async def _broadcast(text: str):
    """Send *text* to every connected browser WS, pruning dead clients."""
    clients = list(ws_clients)
    if not clients:
        return
    results = await asyncio.gather(*(_safe_send(ws, text) for ws in clients))
    stale: list[WebSocket] = []
    for ws, ok in zip(clients, results):
        if not ok:
            stale.append(ws)
    for ws in stale:
        ws_clients.discard(ws)


def on_server_message(msg: Dict[str, Any]):
    """Broadcast every message from the C++ server to all browser WS clients."""
    # Track PnL from order fills and trades
    msg_type = msg.get("type")
    if ENABLE_PNL_DASHBOARD:
        if msg_type == "ORDER_STATUS":
            pnl_tracker.process_order_status(msg)
        elif msg_type == "TRADE_NOTIFICATION":
            pnl_tracker.process_trade(msg)

    # Broadcast original message to browsers.
    # TRADE_NOTIFICATION messages can be extremely frequent. We still forward
    # them (charts need trades) but mark them as non-notify so the frontend
    # can avoid showing pop-up toasts for every market trade.
    if msg_type == "TRADE_NOTIFICATION":
        forwarded = dict(msg)
        forwarded["notify"] = False
        text = json.dumps(forwarded)
    else:
        text = json.dumps(msg)

    asyncio.ensure_future(_broadcast(text))

    # If PnL changed, broadcast PnL update
    if ENABLE_PNL_DASHBOARD and msg_type in ("ORDER_STATUS", "TRADE_NOTIFICATION"):
        pnl_update = json.dumps(pnl_tracker.get_pnl_update())
        asyncio.ensure_future(_broadcast(pnl_update))


def on_connect():
    """When TCP connects, login if CLI credentials are provided and request snapshots.

    If no CLI username is supplied the bridge waits for the browser to send a
    login/register command via WebSocket before requesting snapshots.
    """
    logger.info("TCP connected")
    if tcp_client and server_username:
        asyncio.ensure_future(tcp_client.login(server_username, server_password))
        # Auto-request snapshots only when using CLI-based auth
        for sym in symbols:
            asyncio.ensure_future(tcp_client.request_snapshot(sym))


def on_disconnect():
    """Notify browser clients of TCP disconnect."""
    msg = json.dumps({"type": "CONNECTION_STATUS", "connected": False})
    asyncio.ensure_future(_broadcast(msg))


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
        username=server_username,
        password=server_password,
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
        try:
            order_type = OrderType(order_type_str)
        except ValueError:
            order_type = OrderType.LIMIT
        msg = OrderSubmit(
            client_id=client_id,
            symbol=cmd.get("symbol", "AAPL"),
            side=OrderSide(cmd.get("side", "BUY")),
            order_type=order_type,
            price=float(cmd.get("price", 0)),
            stop_price=float(cmd.get("stop_price", 0)),
            quantity=int(cmd.get("quantity", 0)),
        )
        logger.info("Submitting %s order: %s %s %s @ %.2f x %d (stop=%.2f)",
                     msg.order_type.value, msg.client_id, msg.side.value,
                     msg.symbol, msg.price, msg.quantity, msg.stop_price)
        if ENABLE_PNL_DASHBOARD:
            pnl_tracker.register_order(msg)
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
    elif action == "login" and tcp_client:
        username = cmd.get("username", "")
        password = cmd.get("password", "")
        logger.info("GUI login request for user: %s", username)
        await tcp_client.login(username, password)
    elif action == "register" and tcp_client:
        username = cmd.get("username", "")
        password = cmd.get("password", "")
        logger.info("GUI register request for user: %s", username)
        await tcp_client.register(username, password)
    elif action == "request_all_snapshots" and tcp_client:
        # Frontend sends this after successful login to bootstrap data
        for sym in symbols:
            await tcp_client.request_snapshot(sym)
    else:
        logger.warning("Unknown WS command action: %s", action)


# ── REST endpoints ───────────────────────────────────────────

class OrderRequest(BaseModel):
    symbol: str = "AAPL"
    side: str = "BUY"
    order_type: str = "LIMIT"
    price: float = 0.0
    stop_price: float = 0.0
    quantity: int


@app.post("/api/order")
async def submit_order(req: OrderRequest):
    if not tcp_client or not tcp_client.connected:
        return {"error": "Not connected to server"}
    msg = OrderSubmit(
        client_id=client_id,
        symbol=req.symbol,
        side=OrderSide(req.side),
        order_type=OrderType(req.order_type.upper()) if req.order_type.upper() in ("LIMIT", "MARKET", "STOP", "STOP_LIMIT") else OrderType.LIMIT,
        price=req.price,
        stop_price=req.stop_price,
        quantity=req.quantity,
    )
    if ENABLE_PNL_DASHBOARD:
        pnl_tracker.register_order(msg)
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


# ── PnL endpoints ────────────────────────────────────────────

@app.get("/api/pnl")
async def get_pnl():
    """Get current PnL state."""
    if not ENABLE_PNL_DASHBOARD:
        return {"enabled": False, "message": "PnL dashboard is disabled"}
    return pnl_tracker.get_pnl_update()


@app.post("/api/pnl/capital")
async def set_capital(body: dict):
    """Set initial capital. Body: {"amount": 100000.0}"""
    if not ENABLE_PNL_DASHBOARD:
        return {"error": "PnL dashboard is disabled"}
    amount = float(body.get("amount", 0))
    if amount <= 0:
        return {"error": "Amount must be positive"}
    pnl_tracker.set_initial_capital(amount)
    # Broadcast PnL update
    pnl_update = json.dumps(pnl_tracker.get_pnl_update())
    await _broadcast(pnl_update)
    return {"status": "ok", "initial_capital": amount}


# ── CLI ──────────────────────────────────────────────────────

def main():
    global server_host, server_port, symbols, server_username, server_password

    parser = argparse.ArgumentParser(description="OrderbookGC Python Bridge")
    parser.add_argument("--server-host", default="127.0.0.1")
    parser.add_argument("--server-port", type=int, default=8080)
    parser.add_argument("--listen-port", type=int, default=3001)
    parser.add_argument("--symbols", default="AAPL",
                        help="Comma-separated symbols to subscribe to")
    parser.add_argument("--username", default="",
                        help="Username for server authentication")
    parser.add_argument("--password", default="",
                        help="Password for server authentication")
    parser.add_argument("--initial-capital", type=float, default=100000.0,
                        help="Initial capital for PnL tracking")
    args = parser.parse_args()

    server_host = args.server_host
    server_port = args.server_port
    symbols = [s.strip() for s in args.symbols.split(",") if s.strip()]
    server_username = args.username
    server_password = args.password
    if ENABLE_PNL_DASHBOARD:
        pnl_tracker.set_initial_capital(args.initial_capital)
    else:
        logger.info("PnL dashboard runtime is disabled")

    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=args.listen_port, log_level="info")


if __name__ == "__main__":
    main()
