#pragma once

#include "order.h"
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <mutex>

namespace orderbook {

struct PriceLevel {
    double price;
    uint32_t quantity;
    int order_count;
};

class OrderBook {
public:
    using OrderCallback = std::function<void(const OrderPtr&)>;
    using TradeCallback = std::function<void(const Trade&)>;
    
    explicit OrderBook(const std::string& symbol);
    ~OrderBook() = default;
    
    bool addOrder(const OrderPtr& order);
    
    bool cancelOrder(const std::string& order_id);
    
    std::vector<PriceLevel> getBidLevels(int depth = 10) const; // now it is hardcoded to 10 levels
    std::vector<PriceLevel> getAskLevels(int depth = 10) const;
    
    // Get the total volume at the best bid/ask
    double getBestBidVolume() const;
    double getBestAskVolume() const;
    
    // Get the best bid/ask price
    double getBestBidPrice() const;
    double getBestAskPrice() const;
    
    // Get a specific order by ID
    OrderPtr getOrder(const std::string& order_id) const;
    
    // Set callbacks for events
    void setOrderCallback(OrderCallback callback) { order_callback_ = callback; }
    void setTradeCallback(TradeCallback callback) { trade_callback_ = callback; }

private:
    // Match a new order with existing orders
    void matchOrder(const OrderPtr& order);
    
    // Generate a trade from two matching orders
    void executeTrade(const OrderPtr& buy_order, const OrderPtr& sell_order, uint32_t quantity);
    
    // Add an order to the appropriate side of the book
    void addOrderToBook(const OrderPtr& order);
    
    // Calculate price levels for a side of the book
    std::vector<PriceLevel> calculatePriceLevels(
        const std::map<double, std::set<OrderPtr, std::function<bool(const OrderPtr&, const OrderPtr&)>>>& orders,
        int depth
    ) const;

private:
    std::string symbol_;
    std::map<std::string, OrderPtr> orders_by_id_;
    std::map<double, std::set<OrderPtr, std::function<bool(const OrderPtr&, const OrderPtr&)>>> bids_;
    std::map<double, std::set<OrderPtr, std::function<bool(const OrderPtr&, const OrderPtr&)>>> asks_;
    mutable std::mutex mutex_;
    OrderCallback order_callback_;
    TradeCallback trade_callback_;
};

}