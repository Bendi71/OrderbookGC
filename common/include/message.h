#pragma once

#include "order.h"
#include "orderbook.h"
#include <iomanip>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <cstdint>

namespace orderbook {

// Message framing class to handle length-prefixed messages
class MessageFrame {
public:
    // Frame a message for sending
    static std::vector<uint8_t> frameMessage(const std::string& message) {
        std::vector<uint8_t> framedMessage;
        framedMessage.reserve(4 + message.size());
        
        // Add 4-byte length prefix (little-endian)
        uint32_t length = static_cast<uint32_t>(message.size());
        framedMessage.push_back(length & 0xFF);
        framedMessage.push_back((length >> 8) & 0xFF);
        framedMessage.push_back((length >> 16) & 0xFF);
        framedMessage.push_back((length >> 24) & 0xFF);
        
        framedMessage.insert(framedMessage.end(), message.begin(), message.end());
        
        return framedMessage;
    }
    
    // Extract length from a frame header
    static uint32_t extractLength(const uint8_t* header) {
        return static_cast<uint32_t>(header[0]) | 
               (static_cast<uint32_t>(header[1]) << 8) | 
               (static_cast<uint32_t>(header[2]) << 16) | 
               (static_cast<uint32_t>(header[3]) << 24);
    }
    
    // Size of the length prefix in bytes
    static constexpr size_t HEADER_SIZE = 4;
};

enum class MessageType {
    ORDER_SUBMIT,
    ORDER_CANCEL,
    ORDER_STATUS,
    ORDERBOOK_SNAPSHOT,
    TRADE_NOTIFICATION,
    ERROR_MSG,
    SNAPSHOT_REQUEST,
    LOGIN,
    LOGIN_RESPONSE,
    REGISTER,
    REGISTER_RESPONSE
};

// Convert MessageType to string
inline std::string messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::ORDER_SUBMIT: return "ORDER_SUBMIT";
        case MessageType::ORDER_CANCEL: return "ORDER_CANCEL";
        case MessageType::ORDER_STATUS: return "ORDER_STATUS";
        case MessageType::ORDERBOOK_SNAPSHOT: return "ORDERBOOK_SNAPSHOT";
        case MessageType::TRADE_NOTIFICATION: return "TRADE_NOTIFICATION";
        case MessageType::ERROR_MSG: return "ERROR";
        case MessageType::SNAPSHOT_REQUEST: return "SNAPSHOT_REQUEST";
        case MessageType::LOGIN: return "LOGIN";
        case MessageType::LOGIN_RESPONSE: return "LOGIN_RESPONSE";
        case MessageType::REGISTER: return "REGISTER";
        case MessageType::REGISTER_RESPONSE: return "REGISTER_RESPONSE";
        default: return "UNKNOWN";
    }
}

// Parse MessageType from string
inline MessageType messageTypeFromString(const std::string& typeStr) {
    if (typeStr == "ORDER_SUBMIT") return MessageType::ORDER_SUBMIT;
    if (typeStr == "ORDER_CANCEL") return MessageType::ORDER_CANCEL;
    if (typeStr == "ORDER_STATUS") return MessageType::ORDER_STATUS;
    if (typeStr == "ORDERBOOK_SNAPSHOT") return MessageType::ORDERBOOK_SNAPSHOT;
    if (typeStr == "TRADE_NOTIFICATION") return MessageType::TRADE_NOTIFICATION;
    if (typeStr == "ERROR") return MessageType::ERROR_MSG;
    if (typeStr == "SNAPSHOT_REQUEST") return MessageType::SNAPSHOT_REQUEST;
    if (typeStr == "LOGIN") return MessageType::LOGIN;
    if (typeStr == "LOGIN_RESPONSE") return MessageType::LOGIN_RESPONSE;
    if (typeStr == "REGISTER") return MessageType::REGISTER;
    if (typeStr == "REGISTER_RESPONSE") return MessageType::REGISTER_RESPONSE;
    throw std::runtime_error("Unknown message type: " + typeStr);
}

class Message { // Abstract base class for messages
public:
    virtual ~Message() = default;
    
    virtual MessageType getType() const = 0;
    
    virtual std::string serialize() const = 0;
};

// Shared pointer type for messages
using MessagePtr = std::shared_ptr<Message>;

// Parse a message from a string
MessagePtr parseMessage(const std::string& data);

// Helper for time point conversion (thread-safe)
inline std::string timePointToIsoString(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()).count() % 1000;
    
    std::tm tm = {};
#ifdef _WIN32
    gmtime_s(&tm, &time_t);
#else
    gmtime_r(&time_t, &tm);
#endif
    
    std::stringstream ss;
    ss << std::put_time(&tm, "%FT%T");
    ss << '.' << std::setfill('0') << std::setw(3) << ms << "Z";
    return ss.str();
}

inline std::chrono::system_clock::time_point isoStringToTimePoint(const std::string& iso) {
    std::tm tm = {};
    std::stringstream ss(iso);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    // Use UTC-aware conversion to match gmtime formatting
#ifdef _WIN32
    auto time_t = _mkgmtime(&tm);
#else
    auto time_t = timegm(&tm);
#endif
    auto tp = std::chrono::system_clock::from_time_t(time_t);
    
    // Extract milliseconds if present
    size_t pos = iso.find('.');
    if (pos != std::string::npos) {
        std::string ms_str = iso.substr(pos + 1, 3);
        int ms = std::stoi(ms_str);
        tp += std::chrono::milliseconds(ms);
    }
    
    return tp;
}

class OrderSubmitMessage : public Message {
public:
    OrderSubmitMessage() = default;
    
    MessageType getType() const override { return MessageType::ORDER_SUBMIT; }
    std::string serialize() const override;
    
    std::string client_id;
    std::string symbol;
    OrderSide side;
    OrderType order_type = OrderType::LIMIT;
    double price;
    double stop_price = 0.0;
    uint32_t quantity;
};

// Order cancellation message
class OrderCancelMessage : public Message {
public:
    OrderCancelMessage() = default;
    OrderCancelMessage(const std::string& order_id, const std::string& client_id)
        : order_id(order_id), client_id(client_id) {}
    
    MessageType getType() const override { return MessageType::ORDER_CANCEL; }
    std::string serialize() const override;
    
    std::string order_id;
    std::string client_id;
};

// Order status message
class OrderStatusMessage : public Message {
public:
    OrderStatusMessage() = default;
    
    MessageType getType() const override { return MessageType::ORDER_STATUS; }
    std::string serialize() const override;
    
    std::string order_id;
    std::string client_id;
    std::string symbol;
    OrderSide side;
    OrderType order_type = OrderType::LIMIT;
    double price;
    double stop_price = 0.0;
    uint32_t quantity;
    uint32_t filled_quantity;
    OrderStatus status;
    std::chrono::system_clock::time_point order_timestamp;
};

// Orderbook snapshot message
class OrderbookSnapshotMessage : public Message {
public:
    OrderbookSnapshotMessage() = default;
    
    MessageType getType() const override { return MessageType::ORDERBOOK_SNAPSHOT; }
    std::string serialize() const override;
    
    std::string symbol;
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
};

// Trade notification message
class TradeNotificationMessage : public Message {
public:
    TradeNotificationMessage() = default;
    
    MessageType getType() const override { return MessageType::TRADE_NOTIFICATION; }
    std::string serialize() const override;
    
    std::string buy_order_id;
    std::string sell_order_id;
    std::string symbol;
    double price;
    uint32_t quantity;
    std::chrono::system_clock::time_point trade_timestamp;
};

class ErrorMessage : public Message {
public:
    ErrorMessage() = default;
    ErrorMessage(const std::string& error_code, const std::string& description)
        : error_code(error_code), description(description) {}
    
    MessageType getType() const override { return MessageType::ERROR_MSG; }
    std::string serialize() const override;
    
    std::string error_code;
    std::string description;
};

class SnapshotRequestMessage : public Message {
public:
    SnapshotRequestMessage() = default;
    SnapshotRequestMessage(const std::string& symbol) : symbol(symbol) {}
    
    MessageType getType() const override { return MessageType::SNAPSHOT_REQUEST; }
    std::string serialize() const override;
    
    std::string symbol;
};

// Login message — client sends credentials to authenticate
class LoginMessage : public Message {
public:
    LoginMessage() = default;
    LoginMessage(const std::string& username, const std::string& password)
        : username(username), password(password) {}
    
    MessageType getType() const override { return MessageType::LOGIN; }
    std::string serialize() const override;
    
    std::string username;
    std::string password;
};

// Login response — server replies with success/failure + token
class LoginResponseMessage : public Message {
public:
    LoginResponseMessage() = default;
    
    MessageType getType() const override { return MessageType::LOGIN_RESPONSE; }
    std::string serialize() const override;
    
    bool success = false;
    std::string session_token;
    std::string username;
    std::string error_message;
};

// Register message — client sends desired username/password to create an account
class RegisterMessage : public Message {
public:
    RegisterMessage() = default;
    RegisterMessage(const std::string& username, const std::string& password)
        : username(username), password(password) {}
    
    MessageType getType() const override { return MessageType::REGISTER; }
    std::string serialize() const override;
    
    std::string username;
    std::string password;
};

// Register response — server replies with success/failure
class RegisterResponseMessage : public Message {
public:
    RegisterResponseMessage() = default;
    
    MessageType getType() const override { return MessageType::REGISTER_RESPONSE; }
    std::string serialize() const override;
    
    bool success = false;
    std::string username;
    std::string error_message;
};

}