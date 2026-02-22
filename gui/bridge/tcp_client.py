"""
Asyncio TCP client that speaks the C++ server's binary protocol
(4-byte LE length prefix + text payload).
"""

from __future__ import annotations

import asyncio
import logging
from typing import Callable, Optional, Dict, Any

from protocol import (
    HEADER_SIZE, MAX_MESSAGE_SIZE,
    frame_message, parse_length, parse_message,
    serialize_order_submit, serialize_order_cancel, serialize_snapshot_request,
)
from models import OrderSubmit, OrderCancel, SnapshotRequest

logger = logging.getLogger("tcp_client")


class TcpClient:
    """Async TCP client for the OrderbookGC C++ server."""

    def __init__(
        self,
        host: str = "127.0.0.1",
        port: int = 8080,
        on_message: Optional[Callable[[Dict[str, Any]], None]] = None,
        on_connect: Optional[Callable[[], None]] = None,
        on_disconnect: Optional[Callable[[], None]] = None,
    ):
        self.host = host
        self.port = port
        self.on_message = on_message
        self.on_connect = on_connect
        self.on_disconnect = on_disconnect

        self._reader: Optional[asyncio.StreamReader] = None
        self._writer: Optional[asyncio.StreamWriter] = None
        self._connected = False
        self._read_task: Optional[asyncio.Task] = None
        self._reconnect_task: Optional[asyncio.Task] = None
        self._write_queue: asyncio.Queue[bytes] = asyncio.Queue()
        self._write_task: Optional[asyncio.Task] = None
        self._should_run = False

    @property
    def connected(self) -> bool:
        return self._connected

    async def start(self):
        """Connect to the server and start read/write loops."""
        self._should_run = True
        await self._connect()

    async def stop(self):
        """Disconnect gracefully."""
        self._should_run = False
        await self._close()

    # ── Public send helpers ──────────────────────────────────

    async def submit_order(self, msg: OrderSubmit):
        text = serialize_order_submit(msg)
        await self._send(text)

    async def cancel_order(self, msg: OrderCancel):
        text = serialize_order_cancel(msg)
        await self._send(text)

    async def request_snapshot(self, symbol: str):
        text = serialize_snapshot_request(SnapshotRequest(symbol=symbol))
        await self._send(text)

    # ── Internal ─────────────────────────────────────────────

    async def _connect(self):
        try:
            self._reader, self._writer = await asyncio.open_connection(
                self.host, self.port)
            self._connected = True
            logger.info("Connected to %s:%d", self.host, self.port)
            if self.on_connect:
                self.on_connect()
            self._read_task = asyncio.create_task(self._read_loop())
            self._write_task = asyncio.create_task(self._write_loop())
        except OSError as e:
            logger.error("Connection failed: %s", e)
            self._connected = False
            if self._should_run:
                self._reconnect_task = asyncio.create_task(self._reconnect())

    async def _close(self):
        self._connected = False
        for task in (self._read_task, self._write_task, self._reconnect_task):
            if task and not task.done():
                task.cancel()
                try:
                    await task
                except asyncio.CancelledError:
                    pass
        if self._writer:
            try:
                self._writer.close()
                await self._writer.wait_closed()
            except Exception:
                pass
        self._reader = None
        self._writer = None

    async def _reconnect(self):
        """Exponential backoff reconnection."""
        delay = 1.0
        max_delay = 30.0
        while self._should_run and not self._connected:
            logger.info("Reconnecting in %.1fs...", delay)
            await asyncio.sleep(delay)
            await self._connect()
            delay = min(delay * 2, max_delay)

    async def _send(self, text: str):
        data = frame_message(text)
        await self._write_queue.put(data)

    async def _write_loop(self):
        try:
            while self._connected:
                data = await self._write_queue.get()
                if self._writer is None:
                    break
                self._writer.write(data)
                await self._writer.drain()
        except (ConnectionError, OSError) as e:
            logger.warning("Write error: %s", e)
            await self._handle_disconnect()
        except asyncio.CancelledError:
            pass

    async def _read_loop(self):
        try:
            while self._connected and self._reader:
                # Read 4-byte header
                header = await self._reader.readexactly(HEADER_SIZE)
                length = parse_length(header)

                if length == 0 or length > MAX_MESSAGE_SIZE:
                    logger.warning("Invalid message length: %d", length)
                    break

                # Read body
                body = await self._reader.readexactly(length)
                text = body.decode("utf-8", errors="replace")

                # Parse and dispatch
                msg = parse_message(text)
                if msg and self.on_message:
                    self.on_message(msg)

        except asyncio.IncompleteReadError:
            logger.info("Server closed connection")
        except (ConnectionError, OSError) as e:
            logger.warning("Read error: %s", e)
        except asyncio.CancelledError:
            return
        finally:
            await self._handle_disconnect()

    async def _handle_disconnect(self):
        if not self._connected:
            return
        self._connected = False
        logger.info("Disconnected from server")
        if self.on_disconnect:
            self.on_disconnect()
        if self._should_run:
            self._reconnect_task = asyncio.create_task(self._reconnect())
