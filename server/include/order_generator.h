#pragma once

#include "orderbook.h"
#include <thread>
#include <atomic>
#include <random>
#include <functional>
#include <mutex>
#include <deque>

namespace orderbook {

class OrderGenerator {
public:
    struct Config {
        std::string symbol;
        
        // Price range (standard deviations for Gaussian distribution)
        double price_range; // Controls the stddev: stddev = last_price * price_range / 3
        double tick_size;
        // Quantity range
        uint32_t min_quantity;
        uint32_t max_quantity;
        
        // Time between orders (milliseconds)
        uint32_t min_interval_ms;
        uint32_t max_interval_ms;
        
        // Probability of generating a buy order vs a sell order (base probability)
        double buy_probability;

        // Cancel probability: chance of canceling an old order after each new order
        double cancel_probability;

        // Spread factor: controls how strongly side selection is biased by price
        // Higher values create a tighter spread. 0 disables spread logic.
        double spread_factor;

        // Maximum number of generated order IDs to track for cancellation
        uint32_t max_tracked_orders;

        // Default constructor with initialization
        Config() 
            : symbol("AAPL")
            , price_range(0.01)
            , tick_size(0.01)
            , min_quantity(10)
            , max_quantity(1000)
            , min_interval_ms(10)
            , max_interval_ms(1000)
            , buy_probability(0.5)
            , cancel_probability(0.0)
            , spread_factor(0.0)
            , max_tracked_orders(500)
        {}
    };

    using OrderCallback = std::function<void(const OrderPtr&)>;
    using CancelCallback = std::function<bool(const std::string&)>;

    OrderGenerator(const Config& config = Config());
    ~OrderGenerator();
    
    // Start generating orders
    void start();
    
    // Stop generating orders
    void stop();
    
    // Set callback for newly generated orders
    void setOrderCallback(OrderCallback callback) { order_callback_ = callback; }

    // Set callback for cancel requests (should call orderbook->cancelOrder)
    void setCancelCallback(CancelCallback callback) { cancel_callback_ = callback; }
    
    // Modify configuration
    void setConfig(const Config& config);
    const Config& getConfig() const { return config_; }

    void updateLastPrice(double price);

    // Statistics
    uint64_t getOrderCount() const { return order_count_.load(); }
    uint64_t getCancelCount() const { return cancel_count_.load(); }

private:
    // Main order generation loop
    void generatorLoop();
      // Generate a random order
    OrderPtr generateOrder();

    // Attempt to cancel a random tracked order
    void tryCancel();
    
    Config config_;
    std::mutex config_mutex_;  // Protects config_ and distributions
    std::atomic<bool> running_{false};
    std::thread generator_thread_;
    std::mt19937 random_engine_;
    OrderCallback order_callback_;
    CancelCallback cancel_callback_;
    
    // Reference price for generating new orders (atomic for cross-thread access)
    std::atomic<double> last_price_{100.0};
    
    // Random distributions for various order parameters
    std::uniform_int_distribution<> quantity_dist_;
    std::uniform_int_distribution<> interval_dist_;
    std::uniform_real_distribution<> side_dist_;

    // Tracked order IDs for cancel generation
    std::deque<std::string> tracked_order_ids_;
    std::mutex tracked_mutex_;

    // Statistics
    std::atomic<uint64_t> order_count_{0};
    std::atomic<uint64_t> cancel_count_{0};
};

}