#include "session.h"
#include <iostream>

namespace orderbook {

Session::Session(asio::ip::tcp::socket socket, MessageCallback callback)
    : socket_(std::move(socket))
    , message_callback_(callback)
    , read_state_(ReadState::HEADER)
    , length_buffer_({0})
    , message_content_buffer_()
{
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
    
    // Frame the message
    std::vector<uint8_t> framedMessage = frameMessage(serialized);
    
    bool should_start_write = false;
    {
        // Lock the mutex before accessing the queue
        std::lock_guard<std::mutex> lock(write_mutex_);
        
        write_messages_.push_back(std::move(framedMessage));
        
        // If no write operation is in progress, start one
        if (!writing_) {
            writing_ = true;
            should_start_write = true;
        }
    }
    
    // Start the write outside the lock to avoid recursive locking
    if (should_start_write) {
        doWriteNext();
    }
}

void Session::close() {
    // Close the socket if it's open
    if (socket_.is_open()) {
        try {
            // Cancel any pending write operations under lock
            {
                std::lock_guard<std::mutex> lock(write_mutex_);
                writing_ = false;
            }
            
            asio::error_code ec;
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket_.close(ec);
        } catch (const std::exception& e) {
            std::cerr << "Exception during session close: " << e.what() << std::endl;
        }
    }
}

void Session::doRead() {
    auto self = shared_from_this();
    
    if (read_state_ == ReadState::HEADER) {
        // Read the 4-byte length header
        asio::async_read(
            socket_,
            asio::buffer(length_buffer_),
            [this, self](std::error_code ec, std::size_t bytes_transferred) {
                if (!ec && bytes_transferred == MessageFrame::HEADER_SIZE) {
                    // Extract message length using the proper method from MessageFrame
                    uint32_t message_length = MessageFrame::extractLength(length_buffer_.data());
                    
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
                        [this, self](std::error_code ec, std::size_t /*bytes_transferred*/) {
                            if (!ec) {
                                // Convert binary content to string
                                std::string message_string(message_content_buffer_.begin(), message_content_buffer_.end());

                                // Process the message
                                try {
                                    MessagePtr message = parseMessage(message_string);
                                    if (message) {
                                        if (message_callback_) {
                                            message_callback_(message, self);
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
                        std::cerr << "Error reading header: " << ec.message() << " (error code: " << ec.value() << ")" << std::endl;
                        std::cerr << "Bytes transferred: " << bytes_transferred << " (expected: " << MessageFrame::HEADER_SIZE << ")" << std::endl;
                    } else {
                        std::cerr << "Connection closed by client (EOF while reading header)" << std::endl;
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

void Session::doWriteNext() {
    auto self = shared_from_this();
    
    // No mutex lock here — writing_ is true, so only we touch the front of the queue.
    // Other threads may push_back (which doesn't invalidate deque element references).
    asio::async_write(
        socket_,
        asio::buffer(write_messages_.front()),
        [this, self](std::error_code ec, std::size_t /*bytes_transferred*/) {
            if (!ec) {
                // Lock the mutex before modifying the queue
                std::lock_guard<std::mutex> lock(write_mutex_);
                
                // Remove the completed message
                write_messages_.pop_front();
                
                if (write_messages_.empty()) {
                    writing_ = false;
                    // Don't call doWriteNext — nothing to write
                } else {
                    // Unlock happens when lock_guard goes out of scope;
                    // we can safely call doWriteNext after that.
                    // But we need to call it outside the lock, so use a flag.
                    // Actually, since we DON'T lock in doWriteNext, we can call it here.
                    // The lock_guard will destruct at end of this block.
                }
            }
            else {
                close();
                return;
            }
            
            // Check outside the lock scope whether we need to continue
            // (lock_guard is destroyed at the end of the if block above)
            bool more = false;
            {
                std::lock_guard<std::mutex> lock(write_mutex_);
                more = writing_ && !write_messages_.empty();
            }
            if (more) {
                doWriteNext();
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