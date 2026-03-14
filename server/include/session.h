#pragma once

#include "message.h"
#include <memory>
#include <string>
#include <asio.hpp>
#include <deque>
#include <functional>
#include <mutex>
#include <array>

namespace orderbook {

// Forward declarations
class Session;
using SessionPtr = std::shared_ptr<Session>;

class Session : public std::enable_shared_from_this<Session> {
public:
    // Message handler callback type
    using MessageCallback = std::function<void(const MessagePtr&, SessionPtr)>;
    // Disconnect callback — fired once when the session detects a closed connection
    using DisconnectCallback = std::function<void(SessionPtr)>;
    
    // Constructor
    Session(asio::ip::tcp::socket socket, MessageCallback callback);
    
    // Destructor
    ~Session();
    
    // Start the session
    void start();
    
    // Send a message to the client
    void sendMessage(const MessagePtr& message);
    
    // Close the session
    void close();
    
    // Test/Debug utilities
    void injectTestMessage(const std::string& symbol);
    
    // Client identifier accessors
    const std::string& getClientId() const { return client_id_; }
    void setClientId(const std::string& client_id) { client_id_ = client_id; }
    
    // Authentication accessors
    bool isAuthenticated() const { return authenticated_; }
    void setAuthenticated(bool auth) { authenticated_ = auth; }
    const std::string& getUsername() const { return username_; }
    void setUsername(const std::string& username) { username_ = username; }
    const std::string& getSessionToken() const { return session_token_; }
    void setSessionToken(const std::string& token) { session_token_ = token; }
    const std::string& getPermissions() const { return permissions_; }
    void setPermissions(const std::string& perms) { permissions_ = perms; }
    bool hasPermission(const std::string& perm) const {
        return permissions_.find(perm) != std::string::npos;
    }

    // Set disconnect callback
    void setDisconnectCallback(DisconnectCallback cb) { disconnect_callback_ = std::move(cb); }
    
private:
    // ASIO socket
    asio::ip::tcp::socket socket_;
    
    // Client identifier
    std::string client_id_;
    
    // Authentication state
    bool authenticated_ = false;
    std::string username_;
    std::string session_token_;
    std::string permissions_;  // comma-separated: "trade,view,admin"
    
    // Message callback
    MessageCallback message_callback_;
    
    // Disconnect callback
    DisconnectCallback disconnect_callback_;
    
    // Read state machine
    enum class ReadState { HEADER, CONTENT };
    ReadState read_state_ = ReadState::HEADER;
    std::array<uint8_t, 4> length_buffer_;
    std::vector<uint8_t> message_content_buffer_;
    
    // Queue of outgoing messages - important: these need to be binary vectors, not strings
    std::deque<std::vector<uint8_t>> write_messages_;
    std::mutex write_mutex_;
    bool writing_{false};
    
    // Private methods
    void doRead();
    // Starts async write of the front message. Caller must NOT hold write_mutex_.
    void doWriteNext();
    
    // Convert a string message to a framed binary message
    std::vector<uint8_t> frameMessage(const std::string& message);
};

}