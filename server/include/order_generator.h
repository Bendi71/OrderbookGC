#pragma once

#include "orderbook.h"
#include <thread>
#include <atomic>
#include <random>
#include <functional>
#include <mutex>

namespace orderbook {

class OrderGenerator {
public:
    struct Config {
        std::string symbol;
        
        // Price range
        double price_range; // This is the maximum price deviation from the mid-price
        double tick_size;
        // Quantity range
        uint32_t min_quantity;
        uint32_t max_quantity;
        
        // Time between orders (milliseconds)
        uint32_t min_interval_ms;
        uint32_t max_interval_ms;
        
        // Probability of generating a buy order vs a sell order
        double buy_probability;

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
        {}
    };

    using OrderCallback = std::function<void(const OrderPtr&)>;

    OrderGenerator(const Config& config = Config());
    ~OrderGenerator();
    
    // Start generating orders
    void start();
    
    // Stop generating orders
    void stop();
    
    // Set callback for newly generated orders
    void setOrderCallback(OrderCallback callback) { order_callback_ = callback; }
    
    // Modify configuration
    void setConfig(const Config& config);
    const Config& getConfig() const { return config_; }

    void updateLastPrice(double price);

private:
    // Main order generation loop
    void generatorLoop();
      // Generate a random order
    OrderPtr generateOrder();
    
    // Update the last price (call this when a trade happens)
    
    Config config_;
    std::mutex config_mutex_;  // Protects config_ and distributions
    std::atomic<bool> running_{false};
    std::thread generator_thread_;
    std::mt19937 random_engine_;
    OrderCallback order_callback_;
    
    // Reference price for generating new orders (atomic for cross-thread access)
    std::atomic<double> last_price_{100.0};
    
    // Random distributions for various order parameters
    std::uniform_int_distribution<> quantity_dist_;
    std::uniform_int_distribution<> interval_dist_;
    std::uniform_real_distribution<> side_dist_;
};

}