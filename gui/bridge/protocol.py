"""
Protocol module — mirrors common/include/message.h.

Handles:
  - 4-byte little-endian length-prefixed framing
  - key=value newline-delimited text serialization/deserialization
"""

from __future__ import annotations

import struct
from typing import Dict, Any, Optional

from models import (
    OrderSubmit, OrderCancel, SnapshotRequest,
    OrderStatusMsg, OrderbookSnapshot, TradeNotification, ErrorMsg,
    PriceLevel, OrderSide, OrderType, OrderStatus,
)

HEADER_SIZE = 4
MAX_MESSAGE_SIZE = 1024 * 1024  # 1 MB


# ── Framing ──────────────────────────────────────────────────

def frame_message(text: str) -> bytes:
    """Prepend a 4-byte little-endian length header to a text payload."""
    payload = text.encode("utf-8")
    return struct.pack("<I", len(payload)) + payload


def parse_length(header: bytes) -> int:
    """Extract message length from a 4-byte LE header."""
    return struct.unpack("<I", header)[0]


# ── Serialization (Python → C++ server) ─────────────────────

def serialize_order_submit(msg: OrderSubmit) -> str:
    lines = [
        "type=ORDER_SUBMIT",
        f"client_id={msg.client_id}",
        f"symbol={msg.symbol}",
        f"side={msg.side.value}",
        f"order_type={msg.order_type.value}",
        f"price={msg.price:.2f}",
        f"quantity={msg.quantity}",
    ]
    return "\n".join(lines)


def serialize_order_cancel(msg: OrderCancel) -> str:
    lines = [
        "type=ORDER_CANCEL",
        f"order_id={msg.order_id}",
        f"client_id={msg.client_id}",
    ]
    return "\n".join(lines)


def serialize_snapshot_request(msg: SnapshotRequest) -> str:
    return f"type=SNAPSHOT_REQUEST\nsymbol={msg.symbol}\n"


def serialize_order_status_request(order_id: str, client_id: str) -> str:
    lines = [
        "type=ORDER_STATUS",
        f"order_id={order_id}",
        f"client_id={client_id}",
    ]
    return "\n".join(lines)


# ── Deserialization (C++ server → Python) ────────────────────

def _extract_string(line: str) -> str:
    pos = line.find("=")
    return line[pos + 1:] if pos != -1 else ""


def _extract_float(line: str) -> float:
    try:
        return float(_extract_string(line))
    except ValueError:
        return 0.0


def _extract_int(line: str) -> int:
    try:
        return int(_extract_string(line))
    except ValueError:
        return 0


def parse_message(data: str) -> Optional[Dict[str, Any]]:
    """
    Parse a key=value text message from the server.
    Returns a dict with a 'type' key and all parsed fields.
    """
    lines = data.split("\n")
    if not lines:
        return None

    msg_type = _extract_string(lines[0])

    if msg_type == "ORDER_STATUS":
        return _parse_order_status(lines[1:])
    elif msg_type == "ORDERBOOK_SNAPSHOT":
        return _parse_snapshot(lines[1:])
    elif msg_type == "TRADE_NOTIFICATION":
        return _parse_trade(lines[1:])
    elif msg_type == "ERROR":
        return _parse_error(lines[1:])
    else:
        return {"type": "UNKNOWN", "raw": data}


def _parse_order_status(lines: list[str]) -> dict:
    result: dict = {"type": "ORDER_STATUS"}
    for line in lines:
        if not line:
            continue
        if line.startswith("order_id="):
            result["order_id"] = _extract_string(line)
        elif line.startswith("client_id="):
            result["client_id"] = _extract_string(line)
        elif line.startswith("symbol="):
            result["symbol"] = _extract_string(line)
        elif line.startswith("side="):
            result["side"] = _extract_string(line)
        elif line.startswith("price="):
            result["price"] = _extract_float(line)
        elif line.startswith("quantity="):
            result["quantity"] = _extract_int(line)
        elif line.startswith("filled_quantity="):
            result["filled_quantity"] = _extract_int(line)
        elif line.startswith("status="):
            result["status"] = _extract_string(line)
        elif line.startswith("order_type="):
            result["order_type"] = _extract_string(line)
        elif line.startswith("timestamp="):
            result["timestamp"] = _extract_string(line)
    return result


def _parse_snapshot(lines: list[str]) -> dict:
    result: dict = {"type": "ORDERBOOK_SNAPSHOT", "symbol": "", "bids": [], "asks": []}
    in_bids = False
    in_asks = False
    current: dict = {}

    for line in lines:
        if not line:
            continue
        if line.startswith("symbol="):
            result["symbol"] = _extract_string(line)
        elif line == "BIDS:":
            in_bids = True
            in_asks = False
        elif line == "ASKS:":
            in_bids = False
            in_asks = True
        elif in_bids or in_asks:
            if line.startswith("price="):
                current["price"] = _extract_float(line)
            elif line.startswith("quantity="):
                current["quantity"] = _extract_int(line)
            elif line.startswith("order_count="):
                current["order_count"] = _extract_int(line)
                target = "bids" if in_bids else "asks"
                result[target].append(current)
                current = {}

    return result


def _parse_trade(lines: list[str]) -> dict:
    result: dict = {"type": "TRADE_NOTIFICATION"}
    for line in lines:
        if not line:
            continue
        if line.startswith("buy_order_id="):
            result["buy_order_id"] = _extract_string(line)
        elif line.startswith("sell_order_id="):
            result["sell_order_id"] = _extract_string(line)
        elif line.startswith("symbol="):
            result["symbol"] = _extract_string(line)
        elif line.startswith("price="):
            result["price"] = _extract_float(line)
        elif line.startswith("quantity="):
            result["quantity"] = _extract_int(line)
        elif line.startswith("timestamp="):
            result["timestamp"] = _extract_string(line)
    return result


def _parse_error(lines: list[str]) -> dict:
    result: dict = {"type": "ERROR"}
    for line in lines:
        if not line:
            continue
        if line.startswith("error_code="):
            result["error_code"] = _extract_string(line)
        elif line.startswith("description="):
            result["description"] = _extract_string(line)
    return result
