#include "client.h"
#include "message.h"
#include <iostream>

namespace orderbook {

Client::Client()
    : socket_(io_context_)
    , read_state_(ReadState::HEADER)
    , length_buffer_({0})
    , message_content_buffer_()
{
}

Client::~Client() {
    disconnect();
}

bool Client::connect(const std::string& host, uint16_t port) {
    if (connected_) {
        return true;
    }
    
    try {
        // Resolve the host
        asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(host, std::to_string(port));
        
        // Connect to the server
        asio::connect(socket_, endpoints);
        
        if (socket_.is_open()) {
            connected_ = true;
            running_ = true;
            
            // Start the IO context in a separate thread
            io_thread_ = std::thread(&Client::runIOContext, this);
            
            // Start reading from the socket
            startRead();
            
            // Call the connection callback if set
            if (connection_callback_) {
                connection_callback_(true);
            }
            
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "Connection error: " << e.what() << std::endl;
    }
    
    return false;
}

void Client::disconnect() {
    if (!connected_) {
        return;
    }
    
    running_ = false;
    
    try {
        // Close the socket
        if (socket_.is_open()) {
            asio::error_code ec;
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket_.close(ec);
        }
        
        // Stop the IO context
        io_context_.stop();
        
        // Wait for the IO thread to finish
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
        
        connected_ = false;
        
        // Call the connection callback if set
        if (connection_callback_) {
            connection_callback_(false);
        }
    } catch (const std::exception& e) {
        std::cerr << "Disconnection error: " << e.what() << std::endl;
    }
}

void Client::submitOrder(const std::string& client_id, const std::string& symbol, 
                        OrderSide side, double price, uint32_t quantity) {
    if (!connected_) {
        return;
    }
    
    auto message = std::make_shared<OrderSubmitMessage>();
    message->client_id = client_id;
    message->symbol = symbol;
    message->side = side;
    message->price = price;
    message->quantity = quantity;
    
    write(message->serialize());
}

void Client::cancelOrder(const std::string& order_id, const std::string& client_id) {
    if (!connected_) {
        return;
    }
    
    auto message = std::make_shared<OrderCancelMessage>(order_id, client_id);
    write(message->serialize());
}

void Client::requestOrderbookSnapshot(const std::string& symbol) {
    if (!connected_) {
        return;
    }
    
    // Use SnapshotRequestMessage instead of OrderbookSnapshotMessage
    auto message = std::make_shared<SnapshotRequestMessage>(symbol);
    write(message->serialize());
}

void Client::requestOrderStatus(const std::string& order_id, const std::string& client_id) {
    if (!connected_) {
        return;
    }
    
    auto message = std::make_shared<OrderStatusMessage>();
    message->order_id = order_id;
    message->client_id = client_id;
    
    write(message->serialize());
}

void Client::startRead() {
    if (!running_ || !socket_.is_open()) {
        return;
    }
    
    // Create a shared_ptr to this client to keep it alive during async operations
    auto self = shared_from_this();
    
    if (read_state_ == ReadState::HEADER) {
        // Read the 4-byte length header using a fixed-size buffer
        asio::async_read(
            socket_,
            asio::buffer(length_buffer_),
            [this, self](std::error_code ec, std::size_t bytes_transferred) {
                if (!ec && bytes_transferred == MessageFrame::HEADER_SIZE) {
                    // Extract message length
                    uint32_t message_length = MessageFrame::extractLength(length_buffer_.data());
                    
                    // Validate the message length to prevent buffer overflow
                    if (message_length == 0 || message_length > 1024 * 1024) { // Max 1MB message size
                        std::cerr << "Invalid message length: " << message_length << std::endl;
                        socket_.close();
                        connected_ = false;
                        if (connection_callback_) {
                            connection_callback_(false);
                        }
                        return;
                    }
                    
                    // Prepare buffer for content
                    message_content_buffer_.resize(message_length);
                    
                    // Switch to reading content
                    read_state_ = ReadState::CONTENT;
                    
                    // Read the content directly instead of calling startRead again
                    asio::async_read(
                        socket_,
                        asio::buffer(message_content_buffer_),
                        [this, self](std::error_code ec, std::size_t /*bytes_transferred*/) {
                            if (!ec) {
                                // Convert binary content to string
                                std::string message_string(message_content_buffer_.begin(), message_content_buffer_.end());
                                
                                // Process the message
                                handleData(message_string);
                                
                                // Switch back to reading header for next message
                                read_state_ = ReadState::HEADER;
                                
                                // Continue reading
                                startRead();
                            } else {
                                // Handle error
                                if (ec != asio::error::eof) {
                                    std::cerr << "Error reading content: " << ec.message() << std::endl;
                                } else {
                                    std::cerr << "Connection closed by server (EOF while reading content)" << std::endl;
                                }
                                socket_.close();
                                connected_ = false;
                                if (connection_callback_) {
                                    connection_callback_(false);
                                }
                            }
                        });
                } else {
                    // Handle error
                    if (ec != asio::error::eof) {
                        std::cerr << "Error reading header: " << ec.message() << std::endl;
                    } else {
                        std::cerr << "Connection closed by server (EOF while reading header)" << std::endl;
                    }
                    socket_.close();
                    connected_ = false;
                    if (connection_callback_) {
                        connection_callback_(false);
                    }
                }
            });
    } else { // ReadState::CONTENT
        // This branch should never be executed because we now handle content reading in the header callback
        std::cerr << "Warning: Unexpected ReadState::CONTENT in startRead()" << std::endl;
        read_state_ = ReadState::HEADER;
        startRead();
    }
}

void Client::write(const std::string& message) {
    if (!connected_ || !socket_.is_open()) {
        return;
    }
    
    // Frame the message with length prefix
    std::vector<uint8_t> framedMessage = MessageFrame::frameMessage(message);
    
    // Post to io_context to ensure thread safety
    asio::post(io_context_, [this, framedMessage = std::move(framedMessage)]() mutable {
        bool should_start_write = false;
        {
            std::lock_guard<std::mutex> lock(write_mutex_);
            write_queue_.push_back(std::move(framedMessage));
            if (!writing_) {
                writing_ = true;
                should_start_write = true;
            }
        }
        if (should_start_write) {
            doWriteNext();
        }
    });
}

void Client::doWriteNext() {
    auto self = shared_from_this();
    
    // No mutex lock here — writing_ is true, so only we touch the front of the queue.
    asio::async_write(
        socket_,
        asio::buffer(write_queue_.front()),
        [this, self](std::error_code ec, std::size_t /*bytes_transferred*/) {
            if (!ec) {
                bool more = false;
                {
                    std::lock_guard<std::mutex> lock(write_mutex_);
                    if (!write_queue_.empty()) {
                        write_queue_.pop_front();
                    }
                    if (write_queue_.empty()) {
                        writing_ = false;
                    } else {
                        more = true;
                    }
                }
                if (more) {
                    doWriteNext();
                }
            }
            else {
                std::cerr << "CLIENT ERROR: Write error: " << ec.message() << std::endl;
                connected_ = false;
                if (connection_callback_) {
                    connection_callback_(false);
                }
            }
        });
}

void Client::handleData(const std::string& data) {
    try {
        MessagePtr message = parseMessage(data);
        
        if (message) {
            if (message_callback_) {
                message_callback_(message);
            }
        } else {
            std::cerr << "Error: parseMessage returned nullptr" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling data: " << e.what() << std::endl;
    }
}

void Client::runIOContext() {
    try {
        asio::io_context::work work(io_context_);
        io_context_.run();
    } catch (const std::exception& e) {
        std::cerr << "IO context error: " << e.what() << std::endl;
    }
}

void Client::injectTestMessage(const std::string& symbol) {
    std::cout << "DEBUG: Injecting test orderbook snapshot message for symbol: " << symbol << std::endl;
    
    // Create a test orderbook snapshot message
    auto snapshot = std::make_shared<OrderbookSnapshotMessage>();
    snapshot->symbol = symbol;
    
    // Add some dummy bid price levels
    for (int i = 0; i < 3; i++) {
        PriceLevel bid;
        bid.price = 1000.0 - (i * 10.0);  // 1000, 990, 980
        bid.quantity = 5.0 + i;           // 5, 6, 7
        bid.order_count = i + 1;          // 1, 2, 3
        snapshot->bids.push_back(bid);
    }
    
    // Add some dummy ask price levels
    for (int i = 0; i < 3; i++) {
        PriceLevel ask;
        ask.price = 1010.0 + (i * 10.0);  // 1010, 1020, 1030
        ask.quantity = 3.0 + i;           // 3, 4, 5
        ask.order_count = i + 1;          // 1, 2, 3
        snapshot->asks.push_back(ask);
    }
    
    // Serialize the message to a string
    std::string serialized = snapshot->serialize();
    std::cout << "DEBUG: Serialized test message: " << serialized << std::endl;
    
    try {
        // Process the message directly as if it came from the network
        handleData(serialized);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Exception while injecting test message: " << e.what() << std::endl;
    }
}

} // namespace orderbook