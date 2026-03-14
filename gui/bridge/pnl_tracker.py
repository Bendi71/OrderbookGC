"""
Bridge-level PnL tracker.

Tracks positions per symbol based on order fills and trade notifications.
Computes realized/unrealized PnL and sends PNL_UPDATE messages to
WebSocket clients.
"""

from __future__ import annotations

import logging
from dataclasses import dataclass, field
from typing import Dict, Any, List, Optional

logger = logging.getLogger("pnl_tracker")


@dataclass
class Position:
    """Net position in a single symbol."""
    symbol: str
    quantity: int = 0             # positive = long, negative = short
    avg_entry_price: float = 0.0  # weighted average entry price
    realized_pnl: float = 0.0    # cumulative realized PnL for this symbol
    last_trade_price: float = 0.0 # latest trade price for unrealized calc

    @property
    def unrealized_pnl(self) -> float:
        if self.quantity == 0 or self.last_trade_price == 0:
            return 0.0
        return (self.last_trade_price - self.avg_entry_price) * self.quantity

    @property
    def market_value(self) -> float:
        if self.last_trade_price == 0:
            return abs(self.quantity) * self.avg_entry_price
        return abs(self.quantity) * self.last_trade_price


@dataclass
class Fill:
    """Record of a single fill event."""
    order_id: str
    symbol: str
    side: str
    price: float
    quantity: int


class PnLTracker:
    """
    Tracks PnL at the bridge level.

    Workflow:
    1. `register_order()` — called when we submit an order, stores order metadata
    2. `process_order_status()` — called on ORDER_STATUS updates, detects fills
    3. `process_trade()` — called on TRADE_NOTIFICATION, updates last trade prices
    4. `get_pnl_update()` — returns a PNL_UPDATE dict for broadcasting
    """

    def __init__(self):
        self.initial_capital: float = 100_000.0
        self.positions: Dict[str, Position] = {}
        self.fills: List[Fill] = []
        self._order_meta: Dict[str, Dict[str, Any]] = {}  # order_id -> {side, symbol, last_filled}

    def set_initial_capital(self, amount: float):
        self.initial_capital = amount
        logger.info("Initial capital set to %.2f", amount)

    def register_order(self, order_submit) -> None:
        """Track an order we submitted (from OrderSubmit model)."""
        # We don't have the order_id yet; we'll match by client-side tracking
        # Orders get registered when we see ORDER_STATUS with ACCEPTED
        pass

    def process_order_status(self, msg: Dict[str, Any]) -> None:
        """Process an ORDER_STATUS message to detect new fills."""
        order_id = msg.get("order_id", "")
        if not order_id:
            return

        status = msg.get("status", "")
        filled_qty = int(msg.get("filled_quantity", 0))
        symbol = msg.get("symbol", "")
        side = msg.get("side", "BUY")
        price = float(msg.get("price", 0))
        order_type = msg.get("order_type", "LIMIT")
        stop_price = float(msg.get("stop_price", 0))

        # Track order metadata
        if order_id not in self._order_meta:
            self._order_meta[order_id] = {
                "side": side,
                "symbol": symbol,
                "last_filled": 0,
                "price": price,
                "order_type": order_type,
            }

        meta = self._order_meta[order_id]
        prev_filled = meta["last_filled"]

        if filled_qty > prev_filled:
            # New fill detected
            new_fill_qty = filled_qty - prev_filled
            meta["last_filled"] = filled_qty

            fill = Fill(
                order_id=order_id,
                symbol=symbol,
                side=side,
                price=price,
                quantity=new_fill_qty,
            )
            self.fills.append(fill)
            self._apply_fill(fill)
            logger.info("Fill: %s %s %d @ %.2f (order %s)",
                        side, symbol, new_fill_qty, price, order_id[:8])

    def process_trade(self, msg: Dict[str, Any]) -> None:
        """Process a TRADE_NOTIFICATION to update last trade prices."""
        symbol = msg.get("symbol", "")
        price = float(msg.get("price", 0))
        if symbol and price > 0:
            pos = self.positions.get(symbol)
            if pos:
                pos.last_trade_price = price
            # Also update for symbols we don't hold yet (for reference)
            if symbol not in self.positions:
                self.positions[symbol] = Position(symbol=symbol, last_trade_price=price)
            else:
                self.positions[symbol].last_trade_price = price

    def _apply_fill(self, fill: Fill) -> None:
        """Update position based on a fill."""
        if fill.symbol not in self.positions:
            self.positions[fill.symbol] = Position(symbol=fill.symbol)

        pos = self.positions[fill.symbol]
        signed_qty = fill.quantity if fill.side == "BUY" else -fill.quantity

        if pos.quantity == 0:
            # Opening new position
            pos.avg_entry_price = fill.price
            pos.quantity = signed_qty
        elif (pos.quantity > 0 and signed_qty > 0) or (pos.quantity < 0 and signed_qty < 0):
            # Adding to position — update avg entry price
            total_cost = pos.avg_entry_price * abs(pos.quantity) + fill.price * abs(signed_qty)
            pos.quantity += signed_qty
            pos.avg_entry_price = total_cost / abs(pos.quantity) if pos.quantity != 0 else 0
        else:
            # Reducing / closing / reversing position
            close_qty = min(abs(signed_qty), abs(pos.quantity))
            # Realized PnL = (exit_price - entry_price) * close_qty * direction
            if pos.quantity > 0:
                # We were long, now selling
                pos.realized_pnl += (fill.price - pos.avg_entry_price) * close_qty
            else:
                # We were short, now buying
                pos.realized_pnl += (pos.avg_entry_price - fill.price) * close_qty

            remaining = abs(signed_qty) - close_qty
            if remaining > 0:
                # Position reversed
                pos.quantity = signed_qty + (close_qty if signed_qty < 0 else -close_qty)
                pos.avg_entry_price = fill.price
            else:
                pos.quantity += signed_qty
                if pos.quantity == 0:
                    pos.avg_entry_price = 0

        pos.last_trade_price = fill.price

    @property
    def total_realized_pnl(self) -> float:
        return sum(p.realized_pnl for p in self.positions.values())

    @property
    def total_unrealized_pnl(self) -> float:
        return sum(p.unrealized_pnl for p in self.positions.values())

    @property
    def total_pnl(self) -> float:
        return self.total_realized_pnl + self.total_unrealized_pnl

    @property
    def equity(self) -> float:
        return self.initial_capital + self.total_pnl

    def get_pnl_update(self) -> Dict[str, Any]:
        """Build a PNL_UPDATE message dict for broadcasting."""
        positions_list = []
        for sym, pos in self.positions.items():
            if pos.quantity != 0 or pos.realized_pnl != 0:
                positions_list.append({
                    "symbol": sym,
                    "quantity": pos.quantity,
                    "avg_entry_price": round(pos.avg_entry_price, 4),
                    "last_price": round(pos.last_trade_price, 4),
                    "unrealized_pnl": round(pos.unrealized_pnl, 2),
                    "realized_pnl": round(pos.realized_pnl, 2),
                    "market_value": round(pos.market_value, 2),
                })

        return {
            "type": "PNL_UPDATE",
            "initial_capital": round(self.initial_capital, 2),
            "equity": round(self.equity, 2),
            "total_pnl": round(self.total_pnl, 2),
            "realized_pnl": round(self.total_realized_pnl, 2),
            "unrealized_pnl": round(self.total_unrealized_pnl, 2),
            "positions": positions_list,
            "fill_count": len(self.fills),
        }
