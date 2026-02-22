"""
Pydantic models for all message types in the OrderbookGC protocol.
These serve as the JSON schema for the WebSocket API to the browser.
"""

from __future__ import annotations
from pydantic import BaseModel
from typing import List, Optional
from enum import Enum


class OrderSide(str, Enum):
    BUY = "BUY"
    SELL = "SELL"


class OrderType(str, Enum):
    LIMIT = "LIMIT"
    MARKET = "MARKET"


class OrderStatus(str, Enum):
    PENDING = "PENDING"
    PARTIAL = "PARTIAL"
    FILLED = "FILLED"
    CANCELED = "CANCELED"
    REJECTED = "REJECTED"


class PriceLevel(BaseModel):
    price: float
    quantity: int
    order_count: int


class OrderSubmit(BaseModel):
    client_id: str
    symbol: str
    side: OrderSide
    order_type: OrderType = OrderType.LIMIT
    price: float
    quantity: int


class OrderCancel(BaseModel):
    order_id: str
    client_id: str


class SnapshotRequest(BaseModel):
    symbol: str


class OrderStatusMsg(BaseModel):
    type: str = "ORDER_STATUS"
    order_id: str = ""
    client_id: str = ""
    symbol: str = ""
    side: OrderSide = OrderSide.BUY
    order_type: OrderType = OrderType.LIMIT
    price: float = 0.0
    quantity: int = 0
    filled_quantity: int = 0
    status: OrderStatus = OrderStatus.PENDING
    timestamp: str = ""


class OrderbookSnapshot(BaseModel):
    type: str = "ORDERBOOK_SNAPSHOT"
    symbol: str = ""
    bids: List[PriceLevel] = []
    asks: List[PriceLevel] = []


class TradeNotification(BaseModel):
    type: str = "TRADE_NOTIFICATION"
    buy_order_id: str = ""
    sell_order_id: str = ""
    symbol: str = ""
    price: float = 0.0
    quantity: int = 0
    timestamp: str = ""


class ErrorMsg(BaseModel):
    type: str = "ERROR"
    error_code: str = ""
    description: str = ""
