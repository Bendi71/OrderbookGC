#pragma once

#include <string>
#include <memory>
#include <chrono>

namespace orderbook {

enum class OrderSide {
    BUY,
    SELL
};

enum class OrderStatus {
    PENDING,
    PARTIAL,
    FILLED,
    CANCELED,
    REJECTED
};

class Order {
public:
    Order(const std::string& id,
          OrderSide side,
          double price,
          uint32_t quantity,
          const std::string& symbol,
          const std::string& client_id);
    
    const std::string& getId() const { return id_; }
    const std::string& getClientId() const { return client_id_; }
    const std::string& getSymbol() const { return symbol_; }
    OrderSide getSide() const { return side_; }
    double getPrice() const { return price_; }
    uint32_t getQuantity() const { return quantity_; }
    uint32_t getRemainingQuantity() const { return remaining_quantity_; }
    OrderStatus getStatus() const { return status_; }
    std::chrono::system_clock::time_point getTimestamp() const { return timestamp_; }
    
    bool fill(uint32_t quantity);
    
    bool cancel();
    
    // orderbook sorting functions
    static bool comparePriceAscending(const std::shared_ptr<Order>& a, const std::shared_ptr<Order>& b);
    static bool comparePriceDescending(const std::shared_ptr<Order>& a, const std::shared_ptr<Order>& b);
    
private:
    std::string id_;
    std::string client_id_;        
    std::string symbol_;             
    OrderSide side_;              
    double price_;               
    uint32_t quantity_;               
    uint32_t remaining_quantity_;     
    OrderStatus status_;             
    std::chrono::system_clock::time_point timestamp_;
};

using OrderPtr = std::shared_ptr<Order>;

struct Trade {
    std::string buy_order_id;
    std::string sell_order_id;
    std::string symbol;
    double price;
    uint32_t quantity;
    std::chrono::system_clock::time_point timestamp;
};

}