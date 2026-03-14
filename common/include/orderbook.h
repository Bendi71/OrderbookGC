#pragma once

#include "order.h"
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include <mutex>
#include <set>

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
    
    explicit OrderBook(const std::string& symbol, double tick_size = 0.01);
    ~OrderBook() = default;
    
    // Get and set the tick size
    double getTickSize() const;
    void setTickSize(double tick_size);
    
    // Round a price to the nearest valid tick
    double roundToTickSize(double price) const;
    
    // Validates if a price adheres to the tick size
    bool isValidPrice(double price) const;
    
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
    // Match a new order with existing orders, collecting events for deferred notification
    void matchOrder(const OrderPtr& order,
                    std::vector<Trade>& pending_trades,
                    std::vector<OrderPtr>& pending_order_updates);
    
    // Record a trade from two matching orders (does not fire callbacks)
    void executeTrade(const OrderPtr& buy_order, const OrderPtr& sell_order,
                      uint32_t quantity, double price,
                      std::vector<Trade>& pending_trades,
                      std::vector<OrderPtr>& pending_order_updates);
    
    // Add an order to the appropriate side of the book
    void addOrderToBook(const OrderPtr& order);

    // Store a stop order for later triggering
    void addStopOrder(const OrderPtr& order);

    // Check and trigger stop orders based on trade prices (called inside lock)
    void processStopOrders(const std::set<double>& trade_prices,
                           std::vector<Trade>& pending_trades,
                           std::vector<OrderPtr>& pending_order_updates);
      // Calculate price levels for a side of the book
    std::vector<PriceLevel> calculatePriceLevels(
        const std::map<double, std::vector<OrderPtr>>& orders,
        int depth
    ) const;

private:    std::string symbol_;
    double tick_size_;  // Minimum price increment
    std::map<std::string, OrderPtr> orders_by_id_;
    std::map<double, std::vector<OrderPtr>> bids_;
    std::map<double, std::vector<OrderPtr>> asks_;
    std::map<double, std::vector<OrderPtr>> stop_buy_orders_;   // stop_price → pending stop buys
    std::map<double, std::vector<OrderPtr>> stop_sell_orders_;  // stop_price → pending stop sells
    mutable std::mutex mutex_;
    OrderCallback order_callback_;
    TradeCallback trade_callback_;
};

}