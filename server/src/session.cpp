#include "session.h"
#include <iostream>

namespace orderbook {

Session::Session(asio::ip::tcp::socket socket, MessageCallback callback)
    : socket_(std::move(socket))
    , message_callback_(callback)
    , read_state_(ReadState::HEADER)
    , length_buffer_()
{
    // Initialize length_buffer_ and message_content_buffer_ to appropriate sizes
    read_buffer_.resize(8192); // Keep this for backward compatibility
}

Session::~Session() {
    try {
        close();
    } catch (const std::exception& e) {
        std::cerr << "Exception in session destructor: " << e.what() << std::endl;
    }
}

void Session::start() {
    doRead();
}

void Session::sendMessage(const MessagePtr& message) {
    if (!message) {
        return;
    }
    
    // Serialize the message to a string
    std::string serialized = message->serialize();

    // Get a string representation of the socket's remote endpoint
    std::string endpoint_str;
    try {
        auto endpoint = socket_.remote_endpoint();
        endpoint_str = endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
    } catch (const std::exception& e) {
        endpoint_str = "[unknown endpoint]";
    }
    
    // Log what we're sending
    std::cout << "Server sending message of type: " << messageTypeToString(message->getType()) 
              << " to " << endpoint_str << " (" << serialized.size() << " bytes)" << std::endl;
    
    // Lock the mutex before accessing the queue
    std::lock_guard<std::mutex> lock(write_mutex_);
    
    // Frame the message and add it to the write queue
    std::vector<uint8_t> framedMessage = frameMessage(serialized);
    
    bool write_in_progress = !write_messages_.empty();
    write_messages_.push_back(std::move(framedMessage));
    
    // If no write operation is in progress, start one
    if (!write_in_progress && !writing_) {
        doWrite();
    }
}

void Session::close() {
    // Close the socket if it's open
    if (socket_.is_open()) {
        try {
            std::cout << "SERVER DEBUG: Closing session for client: ";
            try {
                auto endpoint = socket_.remote_endpoint();
                std::cout << endpoint.address().to_string() << ":" << endpoint.port() << std::endl;
            } catch (const std::exception& e) {
                std::cout << "[unknown endpoint - " << e.what() << "]" << std::endl;
            }
        asio::error_code ec;

        if (!write_messages_.empty()) {
                std::cout << "SERVER DEBUG: Flushing " << write_messages_.size() 
                          << " pending messages before closing" << std::endl;
                // We don't actually wait for them to complete since the client might be gone
            }
        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        if (ec) {
                std::cerr << "Socket shutdown error: " << ec.message() << std::endl;
            }
        socket_.close(ec);
        if (ec) {
                std::cerr << "Socket close error: " << ec.message() << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception during session close: " << e.what() << std::endl;
        }
    } else {
        std::cout << "SERVER DEBUG: Session already closed" << std::endl;
    }
}

void Session::doRead() {
    auto self = shared_from_this();
    
    if (read_state_ == ReadState::HEADER) {
        try {
            std::cout << "SERVER DEBUG: Attempting to read message header from client: " 
                    << socket_.remote_endpoint().address().to_string() << ":" 
                    << socket_.remote_endpoint().port() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "SERVER DEBUG: Attempting to read message header from unknown client (endpoint error: "
                    << e.what() << ")" << std::endl;
        }
        // Read the 4-byte length header
        asio::async_read(
            socket_,
            asio::buffer(length_buffer_),
            [this, self](std::error_code ec, std::size_t bytes_transferred) {
                if (!ec && bytes_transferred == MessageFrame::HEADER_SIZE) {
                    // Extract message length using the proper method from MessageFrame
                    uint32_t message_length = MessageFrame::extractLength(length_buffer_.data());
                    std::cout << "Server reading header, message length: " << message_length << " bytes" << std::endl;
                    
                    // Validate the message length to prevent buffer overflow
                    if (message_length == 0 || message_length > 1024 * 1024) { // Max 1MB message size
                        std::cerr << "Invalid message length: " << message_length << std::endl;
                        close();
                        return;
                    }
                    
                    // Prepare buffer for content
                    message_content_buffer_.resize(message_length);
                    
                    // Switch to reading content
                    read_state_ = ReadState::CONTENT;
                    
                    // Read the content directly instead of calling doRead again
                    asio::async_read(
                        socket_,
                        asio::buffer(message_content_buffer_),
                        [this, self](std::error_code ec, std::size_t bytes_transferred) {
                            if (!ec) {
                                // Convert binary content to string
                                std::string message_string(message_content_buffer_.begin(), message_content_buffer_.end());
                                
                                std::cout << "\n=== SERVER RECEIVED MESSAGE DETAILS ===" << std::endl;
                                std::cout << "Raw message size: " << message_string.size() << " bytes" << std::endl;

                                // Process the message
                                try {
                                    MessagePtr message = parseMessage(message_string);
                                    if (message) {
                                        std::cout << "Successfully parsed message of type: " << messageTypeToString(message->getType()) << std::endl;
                                        if (message_callback_) {
                                            std::cout << "Calling message callback..." << std::endl;
                                            message_callback_(message, self);
                                            std::cout << "Message callback completed" << std::endl;
                                        } else {
                                            std::cerr << "No message callback set!" << std::endl;
                                        }
                                    } else {
                                        std::cerr << "parseMessage returned null pointer" << std::endl;
                                    }
                                } catch (const std::exception& e) {
                                    std::cerr << "Error parsing message: " << e.what() << std::endl;
                                }
                                
                                // Switch back to reading header for next message
                                read_state_ = ReadState::HEADER;
                                
                                // Continue reading
                                doRead();
                            } else {
                                // Handle error
                                if (ec != asio::error::eof) {
                                    std::cerr << "Error reading content: " << ec.message() << std::endl;
                                } else {
                                    std::cerr << "Connection closed by client (EOF while reading content)" << std::endl;
                                }
                                close();
                            }
                        });
                } else {
                    // Handle error
                    if (ec != asio::error::eof) {
                        std::cerr << "Error reading header: " << ec.message() << std::endl;
                    }
                    close();
                }
            });
    } else { // ReadState::CONTENT
        // This branch should never be executed because we now handle content reading in the header callback
        std::cerr << "Warning: Unexpected ReadState::CONTENT in doRead()" << std::endl;
        read_state_ = ReadState::HEADER;
        doRead();
    }
}

void Session::doWrite() {
    auto self = shared_from_this();
    
    // Lock the mutex before accessing the queue
    std::lock_guard<std::mutex> lock(write_mutex_);
    
    // Check if the queue is empty
    if (write_messages_.empty()) {
        writing_ = false;
        return;
    }
    
    // Mark that we're writing
    writing_ = true;
    
    // Write the message at the front of the queue
    asio::async_write(
        socket_,
        asio::buffer(write_messages_.front()),
        [this, self](std::error_code ec, std::size_t /*bytes_transferred*/) {
            if (!ec) {
                // Lock the mutex before modifying the queue
                std::lock_guard<std::mutex> lock(write_mutex_);
                
                // Remove the message from the queue
                write_messages_.pop_front();
                
                // If there are more messages, continue writing
                if (!write_messages_.empty()) {
                    doWrite();
                }
                else {
                    // No more messages, mark that we're not writing
                    writing_ = false;
                }
            }
            else {
                // Handle error
                std::cerr << "Error writing to socket: " << ec.message() << std::endl;
                close();
            }
        });
}

// Implementation of frameMessage using MessageFrame from message.h
std::vector<uint8_t> Session::frameMessage(const std::string& message) {
    return MessageFrame::frameMessage(message);
}

void Session::injectTestMessage(const std::string& symbol) {
    std::cout << "SERVER DEBUG: Injecting test snapshot request message for symbol: " << symbol << std::endl;
    
    // Create a test snapshot request message as if it came from the client
    auto request = std::make_shared<SnapshotRequestMessage>();
    request->symbol = symbol;
    
    // Serialize the message to a string
    std::string serialized = request->serialize();
    std::cout << "SERVER DEBUG: Serialized test message: " << serialized << std::endl;
    
    try {
        // Process the message directly as if it came from the network
        if (message_callback_) {
            std::cout << "SERVER DEBUG: Calling message callback with injected message..." << std::endl;
            message_callback_(request, shared_from_this());
            std::cout << "SERVER DEBUG: Message callback completed" << std::endl;
        } else {
            std::cerr << "SERVER DEBUG: No message callback set!" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "SERVER ERROR: Exception while injecting test message: " << e.what() << std::endl;
    }
}

}