#include "message.h"
#include <sstream>
#include <iomanip>
#include <chrono>

namespace orderbook {

namespace {
    // Convert a time_point to a string (thread-safe)
    std::string timePointToString(const std::chrono::system_clock::time_point& tp) {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            tp.time_since_epoch()).count() % 1000;
        
        std::tm tm = {};
#ifdef _WIN32
        localtime_s(&tm, &time_t);
#else
        localtime_r(&time_t, &tm);
#endif
        
        std::stringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms;
        return ss.str();
    }
    
    // Parse a time_point from a string
    std::chrono::system_clock::time_point stringToTimePoint(const std::string& str) {
        std::tm tm = {};
        int ms = 0;
        std::stringstream ss(str);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S") >> ms;
        
        auto time_t = std::mktime(&tm);
        auto tp = std::chrono::system_clock::from_time_t(time_t);
        tp += std::chrono::milliseconds(ms);
        
        return tp;
    }
    
    // Helper to extract a value from a line
    template<typename T>
    T extractValue(const std::string& line) {
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            return T{};
        }
        
        std::string value = line.substr(pos + 1);
        std::stringstream ss(value);
        T result;
        ss >> result;
        return result;
    }
    
    // Extract a string value from a line
    std::string extractString(const std::string& line) {
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            return "";
        }
        
        return line.substr(pos + 1);
    }
    
    // Extract an enum value from a line
    template<typename EnumType>
    EnumType extractEnum(const std::string& line);
    
    // Specialization for OrderSide
    template<>
    OrderSide extractEnum<OrderSide>(const std::string& line) {
        std::string value = extractString(line);
        if (value == "BUY") {
            return OrderSide::BUY;
        } else if (value == "SELL") {
            return OrderSide::SELL;
        }
        return OrderSide::BUY;
    }
    
    // Specialization for OrderStatus
    template<>
    OrderStatus extractEnum<OrderStatus>(const std::string& line) {
        std::string value = extractString(line);
        if (value == "PENDING") {
            return OrderStatus::PENDING;
        } else if (value == "PARTIAL") {
            return OrderStatus::PARTIAL;
        } else if (value == "FILLED") {
            return OrderStatus::FILLED;
        } else if (value == "CANCELED") {
            return OrderStatus::CANCELED;
        } else if (value == "REJECTED") {
            return OrderStatus::REJECTED;
        }
        return OrderStatus::PENDING;
    }

    template<>
    OrderType extractEnum<OrderType>(const std::string& line) {
        std::string value = extractString(line);
        if (value == "MARKET") {
            return OrderType::MARKET;
        } else if (value == "STOP") {
            return OrderType::STOP;
        } else if (value == "STOP_LIMIT") {
            return OrderType::STOP_LIMIT;
        }
        return OrderType::LIMIT;
    }
}

MessagePtr parseMessage(const std::string& data) {
    std::istringstream iss(data);
    std::string line;
    
    // Get the message type
    if (!std::getline(iss, line)) {
        return std::make_shared<ErrorMessage>("PARSE_ERROR", "Empty message");
    }
    
    std::string message_type = extractString(line);
    
    if (message_type == "ORDER_SUBMIT") {
        auto message = std::make_shared<OrderSubmitMessage>();
        
        while (std::getline(iss, line)) {
            if (line.find("client_id=") == 0) {
                message->client_id = extractString(line);
            } else if (line.find("symbol=") == 0) {
                message->symbol = extractString(line);
            } else if (line.find("side=") == 0) {
                message->side = extractEnum<OrderSide>(line);
            } else if (line.find("order_type=") == 0) {
                message->order_type = extractEnum<OrderType>(line);
            } else if (line.find("price=") == 0) {
                message->price = extractValue<double>(line);
            } else if (line.find("quantity=") == 0) {
                message->quantity = extractValue<uint32_t>(line);
            } else if (line.find("stop_price=") == 0) {
                message->stop_price = extractValue<double>(line);
            }
        }
        
        return message;
    } else if (message_type == "ORDER_CANCEL") {
        auto message = std::make_shared<OrderCancelMessage>();
        
        while (std::getline(iss, line)) {
            if (line.find("order_id=") == 0) {
                message->order_id = extractString(line);
            } else if (line.find("client_id=") == 0) {
                message->client_id = extractString(line);
            }
        }
        
        return message;
    } else if (message_type == "ORDER_STATUS") {
        auto message = std::make_shared<OrderStatusMessage>();
        
        while (std::getline(iss, line)) {
            if (line.find("order_id=") == 0) {
                message->order_id = extractString(line);
            } else if (line.find("client_id=") == 0) {
                message->client_id = extractString(line);
            } else if (line.find("symbol=") == 0) {
                message->symbol = extractString(line);
            } else if (line.find("side=") == 0) {
                message->side = extractEnum<OrderSide>(line);
            } else if (line.find("price=") == 0) {
                message->price = extractValue<double>(line);
            } else if (line.find("quantity=") == 0) {
                message->quantity = extractValue<uint32_t>(line);
            } else if (line.find("filled_quantity=") == 0) {
                message->filled_quantity = extractValue<uint32_t>(line);
            } else if (line.find("status=") == 0) {
                message->status = extractEnum<OrderStatus>(line);
            } else if (line.find("order_type=") == 0) {
                message->order_type = extractEnum<OrderType>(line);
            } else if (line.find("stop_price=") == 0) {
                message->stop_price = extractValue<double>(line);
            } else if (line.find("timestamp=") == 0) {
                message->order_timestamp = stringToTimePoint(extractString(line));
            }
        }
        
        return message;
    } else if (message_type == "ORDERBOOK_SNAPSHOT") {
        auto message = std::make_shared<OrderbookSnapshotMessage>();
        
        bool in_bids = false;
        bool in_asks = false;
        PriceLevel current_level;
        
        while (std::getline(iss, line)) {
            if (line.find("symbol=") == 0) {
                message->symbol = extractString(line);
            } else if (line == "BIDS:") {
                in_bids = true;
                in_asks = false;
            } else if (line == "ASKS:") {
                in_bids = false;
                in_asks = true;
            } else if (in_bids || in_asks) {
                if (line.find("price=") == 0) {
                    current_level.price = extractValue<double>(line);
                } else if (line.find("quantity=") == 0) {
                    current_level.quantity = extractValue<uint32_t>(line);
                } else if (line.find("order_count=") == 0) {
                    current_level.order_count = extractValue<int>(line);
                    
                    if (in_bids) {
                        message->bids.push_back(current_level);
                    } else {
                        message->asks.push_back(current_level);
                    }
                    
                    // Reset for next level
                    current_level = PriceLevel{};
                }
            }
        }
        
        return message;
    } else if (message_type == "TRADE_NOTIFICATION") {
        auto message = std::make_shared<TradeNotificationMessage>();
        
        while (std::getline(iss, line)) {
            if (line.find("buy_order_id=") == 0) {
                message->buy_order_id = extractString(line);
            } else if (line.find("sell_order_id=") == 0) {
                message->sell_order_id = extractString(line);
            } else if (line.find("symbol=") == 0) {
                message->symbol = extractString(line);
            } else if (line.find("price=") == 0) {
                message->price = extractValue<double>(line);
            } else if (line.find("quantity=") == 0) {
                message->quantity = extractValue<uint32_t>(line);
            } else if (line.find("timestamp=") == 0) {
                message->trade_timestamp = stringToTimePoint(extractString(line));
            }
        }
        
        return message;
    } else if (message_type == "ERROR") {
        auto message = std::make_shared<ErrorMessage>();
        
        while (std::getline(iss, line)) {
            if (line.find("error_code=") == 0) {
                message->error_code = extractString(line);
            } else if (line.find("description=") == 0) {
                message->description = extractString(line);
            }
        }
        
        return message;
    } else if (message_type == "SNAPSHOT_REQUEST") {
        auto message = std::make_shared<SnapshotRequestMessage>();
        
        while (std::getline(iss, line)) {
            if (line.find("symbol=") == 0) {
                message->symbol = extractString(line);
            }
        }
        
        return message;    } else if (message_type == "LOGIN") {
        auto message = std::make_shared<LoginMessage>();
        
        while (std::getline(iss, line)) {
            if (line.find("username=") == 0) {
                message->username = extractString(line);
            } else if (line.find("password=") == 0) {
                message->password = extractString(line);
            }
        }
        
        return message;
    } else if (message_type == "LOGIN_RESPONSE") {
        auto message = std::make_shared<LoginResponseMessage>();
        
        while (std::getline(iss, line)) {
            if (line.find("success=") == 0) {
                message->success = (extractString(line) == "true");
            } else if (line.find("session_token=") == 0) {
                message->session_token = extractString(line);
            } else if (line.find("username=") == 0) {
                message->username = extractString(line);
            } else if (line.find("error_message=") == 0) {
                message->error_message = extractString(line);
            }
        }
        
        return message;
    } else if (message_type == "REGISTER") {
        auto message = std::make_shared<RegisterMessage>();
        
        while (std::getline(iss, line)) {
            if (line.find("username=") == 0) {
                message->username = extractString(line);
            } else if (line.find("password=") == 0) {
                message->password = extractString(line);
            }
        }
        
        return message;
    } else if (message_type == "REGISTER_RESPONSE") {
        auto message = std::make_shared<RegisterResponseMessage>();
        
        while (std::getline(iss, line)) {
            if (line.find("success=") == 0) {
                message->success = (extractString(line) == "true");
            } else if (line.find("username=") == 0) {
                message->username = extractString(line);
            } else if (line.find("error_message=") == 0) {
                message->error_message = extractString(line);
            }
        }
        
        return message;
    }
    
    // Unknown message type
    return std::make_shared<ErrorMessage>("PARSE_ERROR", "Unknown message type: " + message_type);
}

// Helper: OrderType to wire string
static const char* orderTypeToWireString(OrderType t) {
    switch (t) {
        case OrderType::MARKET: return "MARKET";
        case OrderType::STOP: return "STOP";
        case OrderType::STOP_LIMIT: return "STOP_LIMIT";
        default: return "LIMIT";
    }
}

// OrderSubmitMessage serialization
std::string OrderSubmitMessage::serialize() const {
    std::stringstream ss;
    ss << "type=ORDER_SUBMIT\n";
    ss << "client_id=" << client_id << "\n";
    ss << "symbol=" << symbol << "\n";
    ss << "side=" << (side == OrderSide::BUY ? "BUY" : "SELL") << "\n";
    ss << "order_type=" << orderTypeToWireString(order_type) << "\n";
    ss << "price=" << std::fixed << std::setprecision(2) << price << "\n";
    if (stop_price > 0.0) {
        ss << "stop_price=" << std::fixed << std::setprecision(2) << stop_price << "\n";
    }
    ss << "quantity=" << quantity;
    
    return ss.str();
}

// OrderCancelMessage serialization
std::string OrderCancelMessage::serialize() const {
    std::stringstream ss;
    ss << "type=ORDER_CANCEL\n";
    ss << "order_id=" << order_id << "\n";
    ss << "client_id=" << client_id;
    
    return ss.str();
}

// OrderStatusMessage serialization
std::string OrderStatusMessage::serialize() const {
    std::stringstream ss;
    ss << "type=ORDER_STATUS\n";
    ss << "order_id=" << order_id << "\n";
    ss << "client_id=" << client_id << "\n";
    ss << "symbol=" << symbol << "\n";
    ss << "side=" << (side == OrderSide::BUY ? "BUY" : "SELL") << "\n";
    ss << "order_type=" << orderTypeToWireString(order_type) << "\n";
    ss << "price=" << std::fixed << std::setprecision(2) << price << "\n";
    if (stop_price > 0.0) {
        ss << "stop_price=" << std::fixed << std::setprecision(2) << stop_price << "\n";
    }
    ss << "quantity=" << quantity << "\n";
    ss << "filled_quantity=" << filled_quantity << "\n";
    
    ss << "status=";
    switch (status) {
        case OrderStatus::PENDING: ss << "PENDING"; break;
        case OrderStatus::PARTIAL: ss << "PARTIAL"; break;
        case OrderStatus::FILLED: ss << "FILLED"; break;
        case OrderStatus::CANCELED: ss << "CANCELED"; break;
        case OrderStatus::REJECTED: ss << "REJECTED"; break;
        default: ss << "UNKNOWN"; break;
    }
    ss << "\n";
    
    ss << "timestamp=" << timePointToString(order_timestamp);
    
    return ss.str();
}

// OrderbookSnapshotMessage serialization
std::string OrderbookSnapshotMessage::serialize() const {
    std::stringstream ss;
    ss << "type=ORDERBOOK_SNAPSHOT\n";
    ss << "symbol=" << symbol << "\n";
    
    ss << "BIDS:\n";
    for (const auto& level : bids) {
        ss << "price=" << std::fixed << std::setprecision(2) << level.price << "\n";
        ss << "quantity=" << level.quantity << "\n";
        ss << "order_count=" << level.order_count << "\n";
    }
    
    ss << "ASKS:\n";
    for (const auto& level : asks) {
        ss << "price=" << std::fixed << std::setprecision(2) << level.price << "\n";
        ss << "quantity=" << level.quantity << "\n";
        ss << "order_count=" << level.order_count << "\n";
    }
    
    return ss.str();
}

// TradeNotificationMessage serialization
std::string TradeNotificationMessage::serialize() const {
    std::stringstream ss;
    ss << "type=TRADE_NOTIFICATION\n";
    ss << "buy_order_id=" << buy_order_id << "\n";
    ss << "sell_order_id=" << sell_order_id << "\n";
    ss << "symbol=" << symbol << "\n";
    ss << "price=" << std::fixed << std::setprecision(2) << price << "\n";
    ss << "quantity=" << quantity << "\n";
    ss << "timestamp=" << timePointToString(trade_timestamp);
    
    return ss.str();
}

// ErrorMessage serialization
std::string ErrorMessage::serialize() const {
    std::stringstream ss;
    ss << "type=ERROR\n"; 
    ss << "error_code=" << error_code << "\n";
    ss << "description=" << description;
    
    return ss.str();
}

// SnapshotRequestMessage serialization
std::string SnapshotRequestMessage::serialize() const {
    std::stringstream ss;
    ss << "type=SNAPSHOT_REQUEST\n";
    ss << "symbol=" << symbol << "\n";
    
    return ss.str();
}

// LoginMessage serialization
std::string LoginMessage::serialize() const {
    std::stringstream ss;
    ss << "type=LOGIN\n";
    ss << "username=" << username << "\n";
    ss << "password=" << password;
    
    return ss.str();
}

// LoginResponseMessage serialization
std::string LoginResponseMessage::serialize() const {
    std::stringstream ss;
    ss << "type=LOGIN_RESPONSE\n";
    ss << "success=" << (success ? "true" : "false") << "\n";
    ss << "session_token=" << session_token << "\n";
    ss << "username=" << username << "\n";
    ss << "error_message=" << error_message;
    
    return ss.str();
}

// RegisterMessage serialization
std::string RegisterMessage::serialize() const {
    std::stringstream ss;
    ss << "type=REGISTER\n";
    ss << "username=" << username << "\n";
    ss << "password=" << password;
    
    return ss.str();
}

// RegisterResponseMessage serialization
std::string RegisterResponseMessage::serialize() const {
    std::stringstream ss;
    ss << "type=REGISTER_RESPONSE\n";
    ss << "success=" << (success ? "true" : "false") << "\n";
    ss << "username=" << username << "\n";
    ss << "error_message=" << error_message;
    
    return ss.str();
}

}