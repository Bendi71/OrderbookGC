#pragma once

#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <queue>
#include <asio.hpp>
#include "message.h"
#include "orderbook.h"

namespace orderbook {

// Make Client inherit from std::enable_shared_from_this to allow it to safely
// pass itself to async operation handlers
class Client : public std::enable_shared_from_this<Client> {
public:
    using MessageCallback = std::function<void(const MessagePtr&)>;
    using ConnectionCallback = std::function<void(bool)>;
    
    // Creating a client should always return a shared_ptr
    static std::shared_ptr<Client> create() {
        return std::shared_ptr<Client>(new Client());
    }
    
    ~Client();
    
    // Connect to a server
    bool connect(const std::string& host, uint16_t port);
    
    // Disconnect from the server
    void disconnect();
    
    // Check if connected
    bool isConnected() const { return connected_; }
    
    // Send messages
    void submitOrder(const std::string& client_id, const std::string& symbol, 
                     OrderSide side, double price, uint32_t quantity);
    void cancelOrder(const std::string& order_id, const std::string& client_id);
    void requestOrderbookSnapshot(const std::string& symbol);
    void requestOrderStatus(const std::string& order_id, const std::string& client_id);
    
    // Set callbacks
    void setMessageCallback(MessageCallback callback) { message_callback_ = callback; }
    void setConnectionCallback(ConnectionCallback callback) { connection_callback_ = callback; }
    
    // Test/Debug utilities
    void injectTestMessage(const std::string& symbol);

private:
    // Make constructor private to enforce use of create() factory method
    Client();
    
    void startRead();
    
    void write(const std::string& message);
    
    // Starts async write of the front message. Caller must NOT hold write_mutex_.
    void doWriteNext();
    
    void handleData(const std::string& data);
    
    void runIOContext();
    
    asio::io_context io_context_;
    asio::ip::tcp::socket socket_;
    std::thread io_thread_;
    
    // Fixed-size array for header to ensure proper memory alignment and fixed size
    std::array<uint8_t, MessageFrame::HEADER_SIZE> length_buffer_;
    std::vector<uint8_t> message_content_buffer_;
    enum class ReadState { HEADER, CONTENT };
    ReadState read_state_ = ReadState::HEADER;
    
    // Using deque instead of queue for consistency with the server implementation
    std::deque<std::vector<uint8_t>> write_queue_;
    std::mutex write_mutex_;
    bool writing_ = false;
    
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    
    MessageCallback message_callback_;
    ConnectionCallback connection_callback_;
};

} // namespace orderbook