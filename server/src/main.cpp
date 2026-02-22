#include "server.h"
#include "order_generator.h"
#include <iostream>
#include <string>
#include <thread>
#include <csignal>
#include <asio.hpp>

using namespace orderbook;

// Global variables for graceful shutdown
std::shared_ptr<Server> server;
asio::io_context io_context;
std::atomic<uint64_t> order_count{0};
std::atomic<uint64_t> trade_count{0};
std::chrono::steady_clock::time_point start_time;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    std::cout << "Received signal " << signal << ", initiating shutdown..." << std::endl;
    if (order_count > 0 || trade_count > 0) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        
        std::cout << "\n===== FINAL BENCHMARK RESULTS =====\n";
        if (elapsed > 0) {
            double orders_per_sec = static_cast<double>(order_count) / elapsed;
            double trades_per_sec = static_cast<double>(trade_count) / elapsed;
            
            std::cout << "Duration: " << elapsed << " seconds\n"
                      << "Total orders: " << order_count << " (" << orders_per_sec << "/sec)\n" 
                      << "Total trades: " << trade_count << " (" << trades_per_sec << "/sec)\n";
                      
            if (order_count > 0) {
                std::cout << "Trade/order ratio: " 
                          << (static_cast<double>(trade_count) / order_count) * 100.0 
                          << "%\n";
            }
        }
        std::cout << "====================================\n";
    }
    
    // Stop the server first
    if (server) {
        server->stop();
    }
    
    // Give some time for cleanup before stopping io_context
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop the io_context
    io_context.stop();
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  -p, --port PORT       Specify the port number (default: 8080)\n"
              << "  -s, --symbols SYMBOLS Comma-separated list of symbols to create orderbooks for\n"
              << "  -i, --interval MS     Snapshot interval in milliseconds (default: 1000)\n"
              << "  -g, --generator       Enable order generator for testing\n"
              << "  -b, --benchmark       Benchmarking orderbook speed\n"
              << "  -h, --help            Show this help message\n";
}

int main(int argc, char* argv[]) {
    unsigned short port = 8080;
    std::vector<std::string> symbols = {"AAPL"};
    int snapshot_interval_ms = 1000;
    bool enable_generator = false;
    bool benchmark_mode = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                try {
                    port = std::stoi(argv[++i]);
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid port number\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: Port number required after " << arg << "\n";
                return 1;
            }
        } else if (arg == "-s" || arg == "--symbols") {
            if (i + 1 < argc) {
                symbols.clear();
                std::string symbols_str = argv[++i];
                
                // Split the comma-separated string into individual symbols
                std::string::size_type prev_pos = 0, pos = 0;
                while ((pos = symbols_str.find(',', pos)) != std::string::npos) {
                    std::string symbol = symbols_str.substr(prev_pos, pos - prev_pos);
                    if (!symbol.empty()) {
                        symbols.push_back(symbol);
                    }
                    prev_pos = ++pos;
                }
                
                // Add the last symbol
                std::string symbol = symbols_str.substr(prev_pos, pos - prev_pos);
                if (!symbol.empty()) {
                    symbols.push_back(symbol);
                }
                
                // If no valid symbols were provided, use the default ones
                if (symbols.empty()) {
                    std::cerr << "Warning: No valid symbols provided, using default symbols\n";
                    symbols = {"AAPL"};
                }
            } else {
                std::cerr << "Error: Symbol list required after " << arg << "\n";
                return 1;
            }
        } else if (arg == "-i" || arg == "--interval") {
            if (i + 1 < argc) {
                try {
                    snapshot_interval_ms = std::stoi(argv[++i]);
                    if (snapshot_interval_ms < 100) {
                        std::cerr << "Warning: Snapshot interval too low, setting to 100ms minimum\n";
                        snapshot_interval_ms = 100;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid snapshot interval\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: Interval value required after " << arg << "\n";
                return 1;
            }
        } else if (arg == "-g" || arg == "--generator") {
            enable_generator = true;
        } else if (arg == "-b" || arg == "--benchmark") {
            enable_generator = true;
            benchmark_mode = true;
            if (i + 1 < argc && argv[i+1][0] != '-') {
                try {
                    int orders_per_second = std::stoi(argv[++i]);
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid orders per second value\n";
                    return 1;
                }
            }
        } else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    start_time = std::chrono::steady_clock::now();
    
    try {
        // Create and start the server
        server = std::make_shared<Server>(io_context, port);
        
        // Set the snapshot interval
        server->setSnapshotInterval(snapshot_interval_ms);
        
        // Create orderbooks for the specified symbols
        for (const auto& symbol : symbols) {
            if (!server->createOrderbook(symbol)) {
                std::cerr << "Failed to create orderbook for symbol: " << symbol << std::endl;
            }
        }

        std::shared_ptr<OrderGenerator> orderGenerator;
        if (enable_generator) {
            OrderGenerator::Config config;
            config.symbol = symbols[0];
            config.price_range = 0.03;
            config.tick_size = server->getOrderbook(config.symbol)->getTickSize();
            config.min_quantity = 1;
            config.max_quantity = 100;
            config.min_interval_ms = 1;
            config.max_interval_ms = 10;
            config.buy_probability = 0.55;
            config.cancel_probability = 0.3;   // 30% chance to cancel after each new order
            config.spread_factor = 0.005;       // Spread-aware side selection
            config.max_tracked_orders = 500;

            orderGenerator = std::make_shared<OrderGenerator>(config);
            orderGenerator->setOrderCallback([&](const OrderPtr& order) {
                auto orderbook = server->getOrderbook(order->getSymbol());
                if (orderbook) {
                    orderbook->addOrder(order);
                    order_count++;
                }
            });

            // Cancel callback: cancel on the orderbook directly
            orderGenerator->setCancelCallback([&](const std::string& order_id) -> bool {
                auto orderbook = server->getOrderbook(symbols[0]);
                if (orderbook) {
                    return orderbook->cancelOrder(order_id);
                }
                return false;
            });

            for (const auto& symbol : symbols) {
                auto orderbook = server->getOrderbook(symbol);
                if (orderbook) {
                    orderbook->setTradeCallback([&, gen = orderGenerator](const Trade& trade) {
                        trade_count++;
                        // Feed last price back to generator so the price walk works
                        gen->updateLastPrice(trade.price);
                    });
                }
            }
            
            orderGenerator->start();
            std::cout << "Order generator started for " << config.symbol << std::endl;
        }
        // Start the server
        server->start();
        
        std::cout << "Orderbook server started on port " << port << std::endl;
        std::cout << "Created orderbooks for " << symbols.size() << " symbols: ";
        for (size_t i = 0; i < symbols.size(); ++i) {
            std::cout << symbols[i];
            if (i < symbols.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << std::endl;
        std::cout << "Snapshot interval: " << snapshot_interval_ms << "ms" << std::endl;
        std::cout << "Press Ctrl+C to quit" << std::endl;
        
        // Run the io_context in the main thread
        io_context.run();
        
        std::cout << "Server shutdown complete" << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}