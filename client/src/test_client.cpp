#include "client.h"
#include "message.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <typeinfo>

using namespace orderbook;

// Global variables for synchronization
std::mutex mtx;
std::condition_variable cv;
std::atomic<bool> received_response(false);
std::atomic<bool> connection_event(false);
std::atomic<bool> test_complete(false);

// Function to display the price level (for orderbook snapshots)
void displayPriceLevel(const PriceLevel& level) {
    std::cout << std::fixed << std::setprecision(2) << level.price 
              << " | " << level.quantity
              << " | " << level.order_count << std::endl;
}

// Function to handle messages received from the server
void messageHandler(const MessagePtr& msg) {
    std::cout << "\n=== MESSAGE RECEIVED ===" << std::endl;
    
    // Set the response flag for most message types
    bool should_signal = true;
    
    if (auto snapshot_msg = std::dynamic_pointer_cast<OrderbookSnapshotMessage>(msg)) {
        std::cout << "Orderbook Snapshot Message:" << std::endl;
        std::cout << "  Symbol: " << snapshot_msg->symbol << std::endl;
        
        std::cout << "\nBids:" << std::endl;
        std::cout << "Price | Quantity | Orders" << std::endl;
        std::cout << "-------------------------" << std::endl;
        for (const auto& bid : snapshot_msg->bids) {
            displayPriceLevel(bid);
        }
        
        std::cout << "\nAsks:" << std::endl;
        std::cout << "Price | Quantity | Orders" << std::endl;
        std::cout << "-------------------------" << std::endl;
        for (const auto& ask : snapshot_msg->asks) {
            displayPriceLevel(ask);
        }
    }
    else if (auto order_status_msg = std::dynamic_pointer_cast<OrderStatusMessage>(msg)) {
        std::cout << "Order Status Message:" << std::endl;
        std::cout << "  Order ID: " << order_status_msg->order_id << std::endl;
        std::cout << "  Client ID: " << order_status_msg->client_id << std::endl;
        std::cout << "  Symbol: " << order_status_msg->symbol << std::endl;
        std::cout << "  Side: " << (order_status_msg->side == OrderSide::BUY ? "BUY" : "SELL") << std::endl;
        std::cout << "  Price: " << std::fixed << std::setprecision(2) << order_status_msg->price << std::endl;
        std::cout << "  Quantity: " << order_status_msg->quantity << std::endl;
        std::cout << "  Filled Quantity: " << order_status_msg->filled_quantity << std::endl;
        std::cout << "  Status: " << (order_status_msg->status == OrderStatus::PENDING ? "PENDING" : 
                                          order_status_msg->status == OrderStatus::FILLED ? "FILLED" : 
                                          order_status_msg->status == OrderStatus::CANCELED ? "CANCELLED" : 
                                          "UNKNOWN") << std::endl;
        std::cout << "  Timestamp: " << order_status_msg->order_timestamp.time_since_epoch().count() << std::endl;
    }
    else if (auto trade_msg = std::dynamic_pointer_cast<TradeNotificationMessage>(msg)) {
        std::cout << "Trade Notification:" << std::endl;
        std::cout << "  Buy Order ID: " << trade_msg->buy_order_id << std::endl;
        std::cout << "  Sell Order ID: " << trade_msg->sell_order_id << std::endl;
        std::cout << "  Symbol: " << trade_msg->symbol << std::endl;
        std::cout << "  Price: " << std::fixed << std::setprecision(2) << trade_msg->price << std::endl;
        std::cout << "  Quantity: " << trade_msg->quantity << std::endl;
        std::cout << "  Timestamp: " << trade_msg->trade_timestamp.time_since_epoch().count() << std::endl;
        
        // Trade notifications are informational and don't need to trigger test completion
        // Only set the flag if we're explicitly waiting for a trade notification
        should_signal = false;
    }
    else if (auto error_msg = std::dynamic_pointer_cast<ErrorMessage>(msg)) {
        std::cout << "Error Message:" << std::endl;
        std::cout << "  Error Code: " << error_msg->error_code << std::endl;
        std::cout << "  Description: " << error_msg->description << std::endl;
    }
    else {
        std::cout << "Unknown message type received." << std::endl;
    }
    
    std::cout << "=== END MESSAGE ===\n" << std::endl;
    
    if (should_signal) {
        std::lock_guard<std::mutex> lock(mtx);
        received_response = true;
        cv.notify_one();
    }
}

// Function to handle connection events
void connectionHandler(bool connected) {
    std::lock_guard<std::mutex> lock(mtx);
    if (connected) {
        std::cout << "Connected to server successfully." << std::endl;
    } else {
        std::cout << "Disconnected from server." << std::endl;
    }
    
    connection_event = true;
    cv.notify_one();
}

// Wait for server response with timeout
bool waitForResponse(int timeout_seconds) {
    std::unique_lock<std::mutex> lock(mtx);
    auto result = cv.wait_for(lock, std::chrono::seconds(timeout_seconds), []{ 
        return received_response.load(); 
    });
    
    // If we got a response, reset the flag for the next test
    if (result) {
        received_response = false;
    }
    
    return result;
}

// Wait for connection event with timeout
bool waitForConnection(int timeout_seconds) {
    std::unique_lock<std::mutex> lock(mtx);
    auto result = cv.wait_for(lock, std::chrono::seconds(timeout_seconds), []{ 
        return connection_event.load(); 
    });
    
    // If we got a connection event, reset the flag
    if (result) {
        connection_event = false;
    }
    
    return result;
}

// Function to run a specific test case with improved synchronization
void runTestCase(std::shared_ptr<Client> client, const std::string& test_name, 
                std::function<void()> test_func, bool verbose_debug) {
    std::cout << "\n=== Running Test: " << test_name << " ===" << std::endl;
    
    // Reset flags before starting the test
    {
        std::lock_guard<std::mutex> lock(mtx);
        received_response = false;
    }
    
    // Run the test
    test_func();
    
    if (verbose_debug) {
        std::cout << "Test function executed. Now waiting for response..." << std::endl;
    }
    
    // Wait for response with timeout
    if (waitForResponse(5)) {  // Reduced timeout to 5 seconds, still plenty of time
        std::cout << "Test '" << test_name << "' completed successfully." << std::endl;
    } else {
        std::cout << "Test '" << test_name << "' timed out waiting for response." << std::endl;
        if (verbose_debug) {
            std::cout << "DEBUG: No response received within timeout period." << std::endl;
            
        }
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

int main(int argc, char* argv[]) {
    // Default settings
    std::string host = "127.0.0.1";
    uint16_t port = 8080;
    std::string client_id = "test_client_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string symbol = "AAPL";
    bool verbose_debug = false;
    
    // Process command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--debug") {
            verbose_debug = true;
            continue;
        }
        
        if (i + 1 < argc) {
            std::string value = argv[i + 1];
            
            if (arg == "--host") {
                host = value;
                i++;
            } else if (arg == "--port") {
                port = static_cast<uint16_t>(std::stoi(value));
                i++;
            } else if (arg == "--client") {
                client_id = value;
                i++;
            } else if (arg == "--symbol") {
                symbol = value;
                i++;
            }
        }
    }    std::cout << "Test Client Configuration:" << std::endl;
    std::cout << "  Host: " << host << std::endl;
    std::cout << "  Port: " << port << std::endl;
    std::cout << "  Client ID: " << client_id << std::endl;
    std::cout << "  Symbol: " << symbol << std::endl;
    std::cout << "  Debug Mode: " << (verbose_debug ? "ON" : "OFF") << std::endl;
    
    // Create client
    auto client = Client::create();
    
    // Set up callbacks
    client->setMessageCallback([&](const MessagePtr& msg) {
        messageHandler(msg);
    });
    
    client->setConnectionCallback([&](bool connected) {
        connectionHandler(connected);
    });
    
    // Connect to server
    std::cout << "\nConnecting to server..." << std::endl;
    if (!client->connect(host, port)) {
        std::cerr << "Failed to connect to server. Exiting." << std::endl;
        return 1;
    }
    
    if (verbose_debug) {
        std::cout << "DEBUG: connect() call returned successfully. Waiting for connection callback..." << std::endl;
    }
    
    // Wait for connection callback
    if (!waitForConnection(5)) {
        std::cerr << "Timed out waiting for connection confirmation. Exiting." << std::endl;
        return 1;
    }
    
    // Run interactive mode or automated tests
    std::cout << "\nRunning automated tests..." << std::endl;
      // Test 1: Request orderbook snapshot
    runTestCase(client, "Request Orderbook Snapshot", [&]() {
        std::cout << "Requesting orderbook snapshot for " << symbol << std::endl;
        client->requestOrderbookSnapshot(symbol);
        if (verbose_debug) {
            std::cout << "DEBUG: requestOrderbookSnapshot() called" << std::endl;
        }
    }, verbose_debug);
    
    // Test 2: Submit a buy order
    std::string buy_order_id;
    runTestCase(client, "Submit Buy Order", [&]() {
        double price = 155.50;  // Example price for AAPL
        uint32_t quantity = 100;   // Example quantity
        
        std::cout << "Submitting BUY order:" << std::endl;
        std::cout << "  Client ID: " << client_id << std::endl;
        std::cout << "  Symbol: " << symbol << std::endl;
        std::cout << "  Price: " << price << std::endl;
        std::cout << "  Quantity: " << quantity << std::endl;
        
        client->submitOrder(client_id, symbol, OrderSide::BUY, price, quantity);
        if (verbose_debug) {
            std::cout << "DEBUG: submitOrder(BUY) called" << std::endl;
        }
    }, verbose_debug);
    
    // Test 3: Submit a sell order
    std::string sell_order_id;
    runTestCase(client, "Submit Sell Order", [&]() {
        double price = 156.00;  // Slightly higher than buy
        uint32_t quantity = 100;
        
        std::cout << "Submitting SELL order:" << std::endl;
        std::cout << "  Client ID: " << client_id << std::endl;
        std::cout << "  Symbol: " << symbol << std::endl;
        std::cout << "  Price: " << price << std::endl;
        std::cout << "  Quantity: " << quantity << std::endl;
        
        client->submitOrder(client_id, symbol, OrderSide::SELL, price, quantity);
        if (verbose_debug) {
            std::cout << "DEBUG: submitOrder(SELL) called" << std::endl;
        }
    }, verbose_debug);
    
    // Test 4: Request orderbook snapshot again to see changes
    runTestCase(client, "Request Updated Orderbook", [&]() {
        std::cout << "Requesting updated orderbook snapshot for " << symbol << std::endl;
        client->requestOrderbookSnapshot(symbol);
        if (verbose_debug) {
            std::cout << "DEBUG: requestOrderbookSnapshot() called again" << std::endl;
        }
    }, verbose_debug);
      // Allow some time for any async messages to arrive (like trade notifications)
    std::cout << "Waiting for any additional messages..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Optionally run in interactive mode
    if (verbose_debug) {
        std::cout << "\n=== Entering Interactive Mode ===" << std::endl;
        std::cout << "Available commands:" << std::endl;
        std::cout << "  snapshot <symbol> - Request orderbook snapshot" << std::endl;
        std::cout << "  buy <symbol> <price> <quantity> - Submit buy order" << std::endl;
        std::cout << "  sell <symbol> <price> <quantity> - Submit sell order" << std::endl;
        std::cout << "  status <order_id> - Request order status" << std::endl;
        std::cout << "  exit - Exit the program" << std::endl;
        
        std::string command;
        while (true) {
            std::cout << "\nEnter command: ";
            std::getline(std::cin, command);
            
            if (command == "exit") {
                break;
            }
            
            std::istringstream iss(command);
            std::string cmd;
            iss >> cmd;
            
            if (cmd == "snapshot") {
                std::string sym;
                iss >> sym;
                if (sym.empty()) sym = symbol;
                
                std::cout << "Requesting snapshot for " << sym << std::endl;
                client->requestOrderbookSnapshot(sym);
            }
            else if (cmd == "buy" || cmd == "sell") {
                std::string sym;
                double price;
                uint32_t qty;
                
                iss >> sym >> price >> qty;
                
                if (sym.empty()) sym = symbol;
                if (price <= 0) price = 100.0;
                if (qty <= 0) qty = 100;
                
                OrderSide side = (cmd == "buy") ? OrderSide::BUY : OrderSide::SELL;
                
                std::cout << "Submitting " << cmd << " order for " << sym 
                          << " at price " << price << " qty " << qty << std::endl;
                          
                client->submitOrder(client_id, sym, side, price, qty);
            }
            else if (cmd == "status") {
                std::string order_id;
                iss >> order_id;
                
                if (!order_id.empty()) {
                    std::cout << "Requesting status for order " << order_id << std::endl;
                    client->requestOrderStatus(order_id, client_id);
                } else {
                    std::cout << "Order ID required" << std::endl;
                }
            }
            else {
                std::cout << "Unknown command: " << cmd << std::endl;
            }
            
            // Give some time for response to arrive
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
    // Clean up and exit
    std::cout << "Tests completed. Disconnecting from server..." << std::endl;
    
    // Allow time for any pending responses to be processed
    std::cout << "Waiting for final messages to be processed..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10)); // Much shorter than 2s
    
    // Now disconnect with improved graceful shutdown
    client->disconnect();
    
    std::cout << "Test client finished. Exiting." << std::endl;
    return 0;
}
