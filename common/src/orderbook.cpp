#include "orderbook.h"
#include <algorithm>
#include <iostream>

namespace orderbook {

OrderBook::OrderBook(const std::string& symbol)
    : symbol_(symbol)
{
}

bool OrderBook::addOrder(const OrderPtr& order) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // if order is null or remaining quantity of order is 0
    if (!order || order->getRemainingQuantity() == 0) { 
        return false;
    }
    
    // makes an iterator to find the order in the map by ID. if 
    auto it = orders_by_id_.find(order->getId());
    if (it != orders_by_id_.end()) {
        return false;
    }
    
    // Add the order to the map for quick lookup
    orders_by_id_[order->getId()] = order;
    
    // Try to match the order with existing orders
    matchOrder(order);
    
    // If the order is not fully filled, add it to the book
    if (order->getRemainingQuantity() > 0 && 
        order->getStatus() != OrderStatus::FILLED &&
        order->getStatus() != OrderStatus::CANCELED) {
        addOrderToBook(order);
    }
    
    // Notify via callback
    if (order_callback_) {
        order_callback_(order);
    }
    
    return true;
}

bool OrderBook::cancelOrder(const std::string& order_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = orders_by_id_.find(order_id);
    if (it == orders_by_id_.end()) {
        return false;
    }
    
    OrderPtr order = it->second; // if matches, then gets the order pointer
    
    if (!order->cancel()) {
        return false;
    }
    
    // Remove the order from the appropriate side of the book
    if (order->getSide() == OrderSide::BUY) {
        auto price_it = bids_.find(order->getPrice());
        if (price_it != bids_.end()) {
            price_it->second.erase(order);
            if (price_it->second.empty()) {
                bids_.erase(price_it);
            }
        }
    } else {
        auto price_it = asks_.find(order->getPrice());
        if (price_it != asks_.end()) {
            price_it->second.erase(order);
            if (price_it->second.empty()) {
                asks_.erase(price_it);
            }
        }
    }
    
    // Notify via callback
    if (order_callback_) {
        order_callback_(order);
    }
    
    return true;
}

OrderPtr OrderBook::getOrder(const std::string& order_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = orders_by_id_.find(order_id);
    if (it != orders_by_id_.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::vector<PriceLevel> OrderBook::getBidLevels(int depth) const {
    std::lock_guard<std::mutex> lock(mutex_); // Lock the mutex to ensure thread safety
    return calculatePriceLevels(bids_, depth);
}

std::vector<PriceLevel> OrderBook::getAskLevels(int depth) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return calculatePriceLevels(asks_, depth);
}

double OrderBook::getBestBidPrice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!bids_.empty()) {
        return bids_.rbegin()->first;  // Highest bid price
    }
    
    return 0.0;
}

double OrderBook::getBestAskPrice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!asks_.empty()) {
        return asks_.begin()->first;  // Lowest ask price
    }
    
    return 0.0;
}

double OrderBook::getBestBidVolume() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!bids_.empty()) {
        const auto& orders = bids_.rbegin()->second;
        uint32_t total = 0;
        for (const auto& order : orders) {
            total += order->getRemainingQuantity();
        }
        return total;
    }
    
    return 0.0;
}

double OrderBook::getBestAskVolume() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!asks_.empty()) {
        const auto& orders = asks_.begin()->second;
        uint32_t total = 0;
        for (const auto& order : orders) {
            total += order->getRemainingQuantity();
        }
        return total;
    }
    
    return 0.0;
}

void OrderBook::matchOrder(const OrderPtr& order) {
    if (order->getSide() == OrderSide::BUY) {
        // Match a buy order with existing sell orders
        while (order->getRemainingQuantity() > 0 && !asks_.empty()) {
            auto ask_it = asks_.begin();  // Get the lowest ask price
            
            // Check if the buy price is >= the lowest ask price
            if (order->getPrice() < ask_it->first) {
                break;  // No match possible
            }
            
            // Get the orders at this price level
            auto& orders_at_price = ask_it->second;
            
            // Match with orders at this price level
            auto order_it = orders_at_price.begin();
            while (order_it != orders_at_price.end() && order->getRemainingQuantity() > 0) {
                OrderPtr matching_order = *order_it;
                
                // Calculate the matching quantity
                uint32_t match_quantity = std::min(order->getRemainingQuantity(), 
                                                matching_order->getRemainingQuantity());
                
                if (match_quantity > 0) {
                    executeTrade(order, matching_order, match_quantity);
                }
                
                // If the matching order is fully filled, remove it
                if (matching_order->getRemainingQuantity() == 0) {
                    order_it = orders_at_price.erase(order_it);
                } else {
                    ++order_it;
                }
            }
            
            // If there are no more orders at this price level, remove it
            if (orders_at_price.empty()) {
                asks_.erase(ask_it);
            }
        }
    } else {
        // Match a sell order with existing buy orders
        while (order->getRemainingQuantity() > 0 && !bids_.empty()) {
            auto bid_it = bids_.rbegin();  // Get the highest bid price
            
            // Check if the sell price is <= the highest bid price
            if (order->getPrice() > bid_it->first) {
                break;  // No match possible
            }
            
            // Get the orders at this price level
            auto& orders_at_price = bid_it->second;
            
            // Match with orders at this price level
            auto order_it = orders_at_price.begin();
            while (order_it != orders_at_price.end() && order->getRemainingQuantity() > 0) {
                OrderPtr matching_order = *order_it;
                
                // Calculate the matching quantity
                uint32_t match_quantity = std::min(order->getRemainingQuantity(), 
                                                matching_order->getRemainingQuantity());
                
                if (match_quantity > 0) {
                    // Execute the trade
                    executeTrade(matching_order, order, match_quantity);
                }
                
                // If the matching order is fully filled, remove it
                if (matching_order->getRemainingQuantity() == 0) {
                    order_it = orders_at_price.erase(order_it);
                } else {
                    ++order_it;
                }
            }
            
            // If there are no more orders at this price level, remove it
            if (orders_at_price.empty()) {
                bids_.erase(--bid_it.base());
            }
        }
    }
}

void OrderBook::executeTrade(const OrderPtr& buy_order, const OrderPtr& sell_order, uint32_t quantity) {
    buy_order->fill(quantity);
    sell_order->fill(quantity);
    
    Trade trade;
    trade.buy_order_id = buy_order->getId();
    trade.sell_order_id = sell_order->getId();
    trade.symbol = symbol_;
    trade.price = sell_order->getPrice();  // Use the seller's price
    trade.quantity = quantity;
    trade.timestamp = std::chrono::system_clock::now();
    
    // Notify via callback
    if (trade_callback_) {
        trade_callback_(trade);
    }
    
    // Notify order updates via callback
    if (order_callback_) {
        order_callback_(buy_order);
        order_callback_(sell_order);
    }
    
    std::cout << "Trade executed: " << quantity << " @ " << trade.price 
              << " (" << trade.buy_order_id << " <-> " << trade.sell_order_id << ")" << std::endl; //logging
}

void OrderBook::addOrderToBook(const OrderPtr& order) {
    if (order->getSide() == OrderSide::BUY) {
        auto& orders_at_price = bids_[order->getPrice()];
        if (orders_at_price.empty()) {
            // Create a new price level with a new order set
            orders_at_price = std::set<OrderPtr, std::function<bool(const OrderPtr&, const OrderPtr&)>>(
                [](const OrderPtr& a, const OrderPtr& b) {
                    return a->getTimestamp() < b->getTimestamp();  // Time priority, FIFO
                }
            );
        }
        orders_at_price.insert(order);
    } else {
        auto& orders_at_price = asks_[order->getPrice()];
        if (orders_at_price.empty()) {
            // Create a new price level with a new order set
            orders_at_price = std::set<OrderPtr, std::function<bool(const OrderPtr&, const OrderPtr&)>>(
                [](const OrderPtr& a, const OrderPtr& b) {
                    return a->getTimestamp() < b->getTimestamp();  // Time priority, FIFO
                }
            );
        }
        orders_at_price.insert(order);
    }
}

std::vector<PriceLevel> OrderBook::calculatePriceLevels(
    const std::map<double, std::set<OrderPtr, std::function<bool(const OrderPtr&, const OrderPtr&)>>>& orders,
    int depth) const {
    std::vector<PriceLevel> levels;
    levels.reserve(std::min(static_cast<size_t>(depth), orders.size()));
    
    auto it = orders.begin();
    if (orders.begin() == orders.end()) {
        // Handle empty orders case
        return levels;
    }
    
    if (&orders == &bids_) {
        // For bids, iterate in reverse (highest price first)
        auto rit = orders.rbegin();
        int count = 0;
        while (rit != orders.rend() && count < depth) {
            PriceLevel level;
            level.price = rit->first;
            level.quantity = 0;
            level.order_count = rit->second.size();
            
            for (const auto& order : rit->second) {
                level.quantity += order->getRemainingQuantity();
            }
            
            levels.push_back(level);
            ++rit;
            ++count;
        }
    } else {
        // For asks, iterate forward (lowest price first)
        auto it = orders.begin();
        int count = 0;
        while (it != orders.end() && count < depth) {
            PriceLevel level;
            level.price = it->first;
            level.quantity = 0;
            level.order_count = it->second.size();
            
            for (const auto& order : it->second) {
                level.quantity += order->getRemainingQuantity();
            }
            
            levels.push_back(level);
            ++it;
            ++count;
        }
    }
    
    return levels;
}

}