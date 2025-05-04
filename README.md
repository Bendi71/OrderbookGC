# OrderbookGC - Orderbook Trading System

OrderbookGC is a high-performance order book trading system with a client-server architecture. The system consists of a server component that manages order books for multiple trading symbols and a client application with a modern GUI for interacting with the trading system.

## Features

- Real-time order book management for multiple trading symbols
- Fast order matching and trade execution
- Client-server architecture using asynchronous network communication
- Support for multiple trading pairs (configurable symbols)
- Modern, dark-themed Qt client interface
- Configurable snapshot intervals and subscriptions for order book updates
- UUID-based order tracking
- Comprehensive API for order submission, cancellation, and trade notifications

## Requirements

### Server
- C++17 compatible compiler
- CMake 3.16 or later
- ASIO networking library (automatically fetched if not found)

### Client
- Qt6 Widgets module
- C++17 compatible compiler

## Building the Application

### Setup Build Environment

1. Clone the repository:
```bash
git clone https://github.com/yourusername/OrderbookGC.git
cd OrderbookGC
```

2. Create a build directory:
```bash
mkdir build
cd build
```

3. Configure the project with CMake:
```bash
cmake ..
```

4. Build the project:
```bash
cmake --build .
```

### Windows-Specific Instructions

If you're using Visual Studio on Windows, make sure you have the necessary build tools:

1. Open a Developer Command Prompt for Visual Studio
2. Navigate to your project directory
3. Run the CMake commands:
```bash
mkdir build
cd build
cmake .. -G "Ninja"  # or your preferred generator
cmake --build .
```

## Running the Application

### Server

The server application supports several command-line options:

```bash
./bin/orderbook_server [options]
```

Options:
- `-p, --port PORT`: Specify the server port number (default: 8080)
- `-s, --symbols SYMBOLS`: Comma-separated list of symbols to create orderbooks for (default: "BTC-USD,ETH-USD")
- `-i, --interval MS`: Snapshot interval in milliseconds (default: 1000)
- `-h, --help`: Show help message

Example:
```bash
./bin/orderbook_server --port 9000 --symbols "BTC-USD,ETH-USD,LTC-USD" --interval 500
```

### Client

Launch the client application:

```bash
./bin/orderbook_client
```

The client will automatically connect to the server running on the local machine (localhost) on the default port unless configured otherwise.

## Using the Client Application

The client application provides a graphical user interface for interacting with the orderbook system:

1. **Connection Panel**: Connect to the server by specifying host and port
2. **Symbol Selection**: Select from available trading pairs
3. **Order Entry**: Submit buy/sell orders with price and quantity
4. **Order Book Display**: View real-time order book depth with bids and asks
5. **Trade History**: View executed trades for the selected symbol
6. **Order Management**: View and cancel your active orders

## Advanced Configuration

### Server Configuration

The server creates default order books for BTC-USD and ETH-USD, but you can configure additional trading pairs through command-line arguments.

### Documentation

Generate API documentation using Doxygen:
```bash
cmake --build . --target docs
```

The documentation will be generated in the `build/docs` directory.

## Development

### Project Structure

- `common/`: Shared components used by both client and server
  - Order and orderbook data structures
  - Message definitions for network communication
  
- `server/`: Server implementation
  - Order matching engine
  - Network session management
  - Order book management
  
- `client/`: Qt-based client application
  - GUI implementation
  - Network client
  - Order book visualization