#pragma once

#include "orderbook.h"
#include "session.h"
#include "order_generator.h"
#include <asio.hpp>
#include <string>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

namespace orderbook {

class Server {
public:
    // Constructor with the IO context
    Server(asio::io_context& io_context, unsigned short port);
    
    // Start accepting connections
    void start();
    
    // Stop the server
    void stop();
    
    // Create a new orderbook
    bool createOrderbook(const std::string& symbol);

    bool createOrderbook(const std::string& symbol, double tick_size);

    // Set tick size for an existing orderbook
    bool setOrderbookTickSize(const std::string& symbol, double tick_size);
    
    // Get an orderbook by symbol
    std::shared_ptr<OrderBook> getOrderbook(const std::string& symbol);
    
    // Get a list of all orderbook symbols
    std::vector<std::string> getOrderbookSymbols() const;
    
    // Periodically send orderbook snapshots to clients
    void startSnapshotTimer(int interval_ms);

    // Configure the built-in order generator
    void configureOrderGenerator(const OrderGenerator::Config& config);
    
    // Set the snapshot interval
    void setSnapshotInterval(int interval_ms) { snapshot_interval_ms_ = interval_ms; }
    
private:
    // Accept a new connection
    void acceptConnection();
    
    // Handle a received message
    void handleMessage(const MessagePtr& message, SessionPtr session);
    
    // Handle different types of messages
    void handleOrderSubmit(const std::shared_ptr<OrderSubmitMessage>& message, SessionPtr session);
    void handleOrderCancel(const std::shared_ptr<OrderCancelMessage>& message, SessionPtr session);
    
    // Handle snapshot request
    void handleSnapshotRequest(const std::shared_ptr<SnapshotRequestMessage>& message, SessionPtr session);
    
    // Handle order status request
    void handleOrderStatus(const std::shared_ptr<OrderStatusMessage>& message, SessionPtr session);
    
    // Remove a disconnected session and clean up its resources
    void removeSession(const SessionPtr& session);
    
    // Callback for order updates
    void onOrderUpdated(const OrderPtr& order);
    
    // Callback for trade notifications
    void onTradeExecuted(const Trade& trade);
    
    // Send a snapshot of the orderbook to all subscribed clients
    void sendOrderbookSnapshots();
    
private:
    // ASIO io_context and acceptor
    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;
    
    // Map of symbols to orderbooks
    std::map<std::string, std::shared_ptr<OrderBook>> orderbooks_;
    mutable std::mutex orderbooks_mutex_;
    
    // Map of client_id to active session (O(1) lookup)
    std::unordered_map<std::string, SessionPtr> sessions_;
    std::mutex sessions_mutex_;
    
    // Map of session ID to subscribed symbols
    std::map<std::string, std::set<std::string>> subscriptions_;
    std::mutex subscriptions_mutex_;
    
    // Map of order ID to client ID for tracking ownership
    std::map<std::string, std::string> order_owners_;
    std::mutex order_owners_mutex_;
    
    // Timer for sending orderbook snapshots
    asio::steady_timer snapshot_timer_;
    int snapshot_interval_ms_ = 1000;  // Default 1 second

    // Order generator for automated trading simulation
    std::shared_ptr<OrderGenerator> order_generator_;
};

}