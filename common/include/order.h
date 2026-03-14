#pragma once

#include <string>
#include <memory>
#include <chrono>

namespace orderbook {

enum class OrderSide {
    BUY,
    SELL
};

enum class OrderType {
    LIMIT,
    MARKET,
    STOP,
    STOP_LIMIT
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
          const std::string& client_id,
          OrderType type = OrderType::LIMIT,
          double stop_price = 0.0);
    
    const std::string& getId() const { return id_; }
    const std::string& getClientId() const { return client_id_; }
    const std::string& getSymbol() const { return symbol_; }
    OrderSide getSide() const { return side_; }
    OrderType getOrderType() const { return order_type_; }
    double getPrice() const { return price_; }
    double getStopPrice() const { return stop_price_; }
    bool isStopOrder() const { return order_type_ == OrderType::STOP || order_type_ == OrderType::STOP_LIMIT; }
    
    /// Trigger a stop order: STOP→MARKET, STOP_LIMIT→LIMIT. Returns true on success.
    bool trigger();
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
    OrderType order_type_;
    double price_;
    double stop_price_;           // trigger price for STOP/STOP_LIMIT orders               
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