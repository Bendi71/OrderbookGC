# OrderbookGC — Real-Time Orderbook Trading System

OrderbookGC is a high-performance limit-order-book engine with a three-tier architecture:

1. **C++ Server** — async TCP server with matching engine, order generator, and snapshot broadcasting
2. **Python Bridge** — FastAPI middleware translating the binary TCP protocol to JSON over WebSocket
3. **React Frontend** — real-time dashboard showing the order book, depth chart, trades, and order management

```
┌──────────────┐       TCP (binary)      ┌──────────────┐      WebSocket (JSON)     ┌──────────────┐
│  C++ Server  │◄───────────────────────►│ Python Bridge │◄──────────────────────────►│ React GUI    │
│  (port 9001) │  4-byte LE + key=value  │  (port 3001) │   JSON messages           │ (port 3000)  │
└──────────────┘                         └──────────────┘                            └──────────────┘
```

## Table of Contents

- [Features](#features)
- [Architecture](#architecture)
- [Requirements](#requirements)
- [Building](#building)
- [Running](#running)
- [Wire Protocol](#wire-protocol)
- [Project Structure](#project-structure)
- [Testing](#testing)
- [GUI Overview](#gui-overview)
- [Order Generator](#order-generator)
- [Configuration Reference](#configuration-reference)
- [License](#license)

---

## Features

- **Price-time priority matching engine** with FIFO ordering at each price level
- **Configurable tick sizes** with automatic price rounding
- **Multi-symbol support** — create orderbooks for any number of symbols
- **Periodic snapshot broadcasting** to all subscribed clients
- **Realistic order generation** — Gaussian price distribution, spread-aware side selection, cancel generation
- **Trade and order status callbacks** — real-time notifications for fills, partial fills, and cancellations
- **Full cancel support** — clients can cancel their own resting orders
- **Web-based GUI** — low-latency visualization of the order book, depth chart, trade history, and order management
- **5 Google Test suites** covering matching, cancellation, edge cases, message serialization, and order lifecycle

---

## Architecture

### C++ Server (`server/`)

The server manages one or more `OrderBook` instances keyed by symbol. Each orderbook is a price-time priority continuous double auction. The server accepts TCP connections, frames messages with 4-byte little-endian length prefixes, and uses a key=value newline-delimited text protocol.

Key components:

| Class | Header | Purpose |
|---|---|---|
| `Server` | `server/include/server.h` | TCP acceptor, session management, message dispatch, snapshot timer |
| `Session` | `server/include/session.h` | Per-connection read/write state machine with write queue |
| `OrderGenerator` | `server/include/order_generator.h` | Generates realistic synthetic orders for testing |

### Common Library (`common/`)

Shared by server and client, provides the core data structures:

| Class | Header | Purpose |
|---|---|---|
| `Order` | `common/include/order.h` | Immutable order with fill/cancel lifecycle |
| `OrderBook` | `common/include/orderbook.h` | Thread-safe matching engine with bid/ask maps |
| `Message` (hierarchy) | `common/include/message.h` | 7 message types with serialization/parsing |
| `generateUuid()` | `common/include/uuid.h` | Cross-platform UUID generation |

### Python Bridge (`gui/bridge/`)

A FastAPI application that:
1. Connects to the C++ server via async TCP
2. Translates the binary protocol to/from JSON
3. Exposes a WebSocket at `/ws` for browser clients
4. Provides REST endpoints for order submission/cancellation

### React Frontend (`gui/frontend/`)

A Vite + React 18 + TypeScript + Tailwind CSS application that:
1. Connects to the bridge via WebSocket
2. Renders a real-time order book price ladder
3. Shows a cumulative depth chart (Recharts)
4. Provides order entry and management
5. Displays trade history and toast notifications

---

## Requirements

### C++ Server
- C++20 compatible compiler (GCC 12+, Clang 15+, MSVC 2022+)
- CMake 3.16+
- ASIO (automatically fetched via FetchContent if not installed)

### Python Bridge
- Python 3.10+
- pip packages: `fastapi`, `uvicorn`, `pydantic`, `websockets`

### React Frontend
- Node.js 18+ and npm

---

## Building

### 1. C++ Server & Tests

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
cmake --build . -j4
# or with MinGW:
cmake .. -G "MinGW Makefiles" -DBUILD_TESTS=ON
mingw32-make -j4
```

The build produces:
- `build/bin/orderbook_server` — the server executable
- `build/bin/orderbook_matching_test` — matching test suite
- `build/bin/orderbook_cancel_test` — cancel test suite
- `build/bin/orderbook_edge_test` — edge case test suite
- `build/bin/message_test` — message serialization test suite
- `build/bin/order_test` — order lifecycle test suite

### 2. Python Bridge

```bash
cd gui/bridge
pip install -r requirements.txt
```

### 3. React Frontend

```bash
cd gui/frontend
npm install
```

---

## Running

Start all three components in separate terminals:

### Terminal 1 — C++ Server

```bash
./build/bin/orderbook_server -p 9001 -s AAPL -g
```

Options:
| Flag | Description | Default |
|---|---|---|
| `-p, --port PORT` | TCP listen port | `8080` |
| `-s, --symbols SYM1,SYM2` | Comma-separated symbol list | `AAPL` |
| `-i, --interval MS` | Snapshot broadcast interval (ms) | `1000` |
| `-g, --generator` | Enable automatic order generation | disabled |
| `-b, --benchmark [N]` | Benchmark mode (implies `-g`) | disabled |

### Terminal 2 — Python Bridge

```bash
cd gui/bridge
python main.py --server-port 9001 --symbols AAPL
```

Options:
| Flag | Description | Default |
|---|---|---|
| `--server-host HOST` | C++ server hostname | `127.0.0.1` |
| `--server-port PORT` | C++ server port | `8080` |
| `--listen-port PORT` | Bridge HTTP/WS port | `3001` |
| `--symbols SYM1,SYM2` | Symbols to subscribe to | `AAPL` |

### Terminal 3 — React Frontend

```bash
cd gui/frontend
npm run dev
```

Opens at **http://localhost:3000**. Vite proxies `/ws` and `/api` to the bridge on port 3001.

For production:

```bash
npm run build
# Serve dist/ with any static file server, proxy /ws and /api to the bridge
```

---

## Wire Protocol

All messages are framed with a **4-byte little-endian length prefix** followed by a UTF-8 **key=value** payload delimited by newlines. The first line is always `type=<MESSAGE_TYPE>`.

### Client → Server Messages

#### ORDER_SUBMIT
```
type=ORDER_SUBMIT
client_id=gui_abc123
symbol=AAPL
side=BUY
price=150.25
quantity=100
```

#### ORDER_CANCEL
```
type=ORDER_CANCEL
order_id=550e8400-e29b-41d4-a716-446655440000
client_id=gui_abc123
```

#### SNAPSHOT_REQUEST
```
type=SNAPSHOT_REQUEST
symbol=AAPL
```

### Server → Client Messages

#### ORDER_STATUS
```
type=ORDER_STATUS
order_id=550e8400-...
client_id=gui_abc123
symbol=AAPL
side=BUY
price=150.25
quantity=100
filled_quantity=50
status=PARTIAL_FILL
timestamp=2026-02-22T10:30:00.123Z
```

Status values: `PENDING`, `PARTIAL`, `FILLED`, `CANCELED`, `REJECTED`

#### ORDERBOOK_SNAPSHOT
```
type=ORDERBOOK_SNAPSHOT
symbol=AAPL
BIDS:
price=150.20
quantity=500
order_count=3
price=150.10
quantity=200
order_count=1
ASKS:
price=150.30
quantity=300
order_count=2
```

#### TRADE_NOTIFICATION
```
type=TRADE_NOTIFICATION
buy_order_id=...
sell_order_id=...
symbol=AAPL
price=150.25
quantity=50
timestamp=2026-02-22T10:30:00.456Z
```

#### ERROR
```
type=ERROR
error_code=INVALID_SYMBOL
description=Orderbook not found for symbol: XYZ
```

---

## Project Structure

```
OrderbookGC/
├── CMakeLists.txt                  # Root CMake — fetches ASIO, adds subdirectories
├── LICENSE                         # GPL-3.0
├── README.md                       # This file
│
├── common/                         # Shared library (orderbook_common)
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── order.h                 # Order, OrderSide, OrderStatus, Trade
│   │   ├── orderbook.h             # OrderBook matching engine
│   │   ├── message.h               # Message hierarchy + framing
│   │   └── uuid.h                  # UUID generation
│   ├── src/
│   │   ├── order.cpp
│   │   ├── orderbook.cpp
│   │   ├── message.cpp
│   │   └── uuid.cpp
│   └── tests/                      # Google Test suites
│       ├── CMakeLists.txt          # Fetches GTest v1.14.0
│       ├── orderbook_matching_test.cpp
│       ├── orderbook_cancel_test.cpp
│       ├── orderbook_edge_test.cpp
│       ├── message_test.cpp
│       └── order_test.cpp
│
├── server/                         # TCP server application
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── server.h
│   │   ├── session.h
│   │   └── order_generator.h
│   └── src/
│       ├── main.cpp                # CLI entry point
│       ├── server.cpp
│       ├── session.cpp
│       └── order_generator.cpp
│
├── client/                         # Legacy C++ client (requires Qt6)
│   ├── CMakeLists.txt
│   ├── include/client.h
│   └── src/
│       ├── client.cpp
│       └── test_client.cpp
│
└── gui/                            # Web-based GUI
    ├── bridge/                     # Python bridge server
    │   ├── requirements.txt
    │   ├── main.py                 # FastAPI app + CLI
    │   ├── models.py               # Pydantic data models
    │   ├── protocol.py             # Binary protocol serialization
    │   └── tcp_client.py           # Async TCP client
    └── frontend/                   # React application
        ├── package.json
        ├── tsconfig.json
        ├── vite.config.ts
        ├── tailwind.config.js
        ├── index.html
        └── src/
            ├── main.tsx
            ├── App.tsx             # Root layout (header + 2-column dashboard)
            ├── vite-env.d.ts
            ├── styles/globals.css
            ├── store/
            │   └── orderbookStore.ts   # Zustand state management
            ├── hooks/
            │   └── useWebSocket.ts     # WS connection + auto-reconnect
            └── components/
                ├── OrderbookView.tsx    # Price ladder (bids/asks)
                ├── DepthChart.tsx       # Cumulative depth chart
                ├── OrderEntry.tsx       # Buy/sell order form
                ├── OrderList.tsx        # Active orders + cancel
                ├── TradeHistory.tsx     # Recent trades table
                └── Notifications.tsx    # Toast notifications
```

---

## Testing

The project uses [Google Test](https://github.com/google/googletest) v1.14.0 (fetched automatically).

### Run All Tests

```bash
cd build
ctest --output-on-failure
```

### Test Suites

| Suite | File | Tests | Coverage |
|---|---|---|---|
| **OrderbookMatchingTest** | `orderbook_matching_test.cpp` | 4 | Basic matching, price priority, time priority, tick rounding |
| **OrderbookCancelTest** | `orderbook_cancel_test.cpp` | 7 | Cancel buy/sell/non-existent/filled/partial, last-at-level, FIFO after cancel |
| **OrderbookEdgeTest** | `orderbook_edge_test.cpp` | 12 | Empty book, zero qty, exact fill, multi-level crossing, depth levels |
| **MessageTest** | `message_test.cpp` | ~20 | Round-trip serialization for all 7 types, edge cases, time conversion |
| **OrderTest** | `order_test.cpp` | ~15 | Fill/cancel lifecycle, overfill rejection, state transitions, comparisons |

### Run a Specific Suite

```bash
./build/bin/orderbook_cancel_test --gtest_filter="*CancelResting*"
```

---

## GUI Overview

### Order Book View
Two-column price ladder with bid bars (green, left) and ask bars (red, right). Each row shows order count, quantity, and price. The spread is displayed in the header and the mid price at the bottom.

### Depth Chart
Cumulative depth visualization using Recharts. Green area for cumulative bid volume (high → low price), red area for cumulative ask volume (low → high price). A dashed yellow line marks the mid price.

### Order Entry
Toggle between BUY (green) and SELL (red), select symbol, enter price and quantity, submit. The order is sent through the WebSocket to the bridge, which forwards it to the C++ server.

### Order List
Table of your submitted orders showing ID, side, price, quantity, filled quantity, and status. Active orders have a cancel button (✕).

### Trade History
Scrolling table of the most recent 100 trades showing price, quantity, symbol, and timestamp.

### Notifications
Toast notifications (bottom-right) for order fills, cancellations, connections, and errors via `react-hot-toast`.

---

## Order Generator

When started with `-g`, the server runs a background order generator that simulates realistic market activity.

### Configuration (set in `server/src/main.cpp`)

| Parameter | Default | Description |
|---|---|---|
| `price_range` | `0.03` | Standard deviation factor for Gaussian price distribution |
| `tick_size` | from orderbook | Minimum price increment |
| `min_quantity` / `max_quantity` | `1` / `100` | Uniform random quantity range |
| `min_interval_ms` / `max_interval_ms` | `1` / `10` | Random delay between orders |
| `buy_probability` | `0.55` | Base probability of generating a buy (before spread adjustment) |
| `cancel_probability` | `0.3` | Probability of canceling a random resting order after each new order |
| `spread_factor` | `0.005` | Controls spread-aware side selection via sigmoid function |
| `max_tracked_orders` | `500` | Maximum order IDs tracked for potential cancellation |

### Price Distribution
Prices are drawn from a **normal distribution** centered on the last trade price (or 100.0 initially). The standard deviation is `last_price * price_range / 3`, so roughly 99.7% of orders fall within the configured price range. Prices are rounded to the nearest tick.

### Side Selection
When `spread_factor > 0`, the side is chosen using a **sigmoid function** that biases toward buying when the generated price is below the rounded price and selling when above. This naturally creates a bid-ask spread around the last trade price.

### Cancel Generation
After each new order, the generator calls `tryCancel()` with probability `cancel_probability`. It picks a random order ID from its tracked deque and invokes the cancel callback, removing the order from the deque regardless of whether the cancel succeeded (the order may have already been filled).

---

## Configuration Reference

### CMake Options

| Option | Default | Description |
|---|---|---|
| `BUILD_TESTS` | `ON` | Build Google Test suites |
| `CMAKE_CXX_STANDARD` | `20` | C++ standard (requires ≥ 17) |

### Environment

| Component | Port | Description |
|---|---|---|
| C++ Server | `9001` (configurable) | Binary TCP protocol |
| Python Bridge | `3001` (configurable) | HTTP + WebSocket (JSON) |
| React Dev Server | `3000` | Vite HMR, proxies to bridge |

---

## License

This project is licensed under the **GNU General Public License v3.0** — see [LICENSE](LICENSE) for details.

Copyright (C) 2026 Bendeguz Horvath