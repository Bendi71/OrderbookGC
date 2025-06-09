#include "order_generator.h"
#include "uuid.h"
#include <chrono>

namespace orderbook {

OrderGenerator::OrderGenerator(const Config& config)
    : config_(config)
    , random_engine_(std::random_device{}())
    , quantity_dist_(config.min_quantity, config.max_quantity)
    , interval_dist_(config.min_interval_ms, config.max_interval_ms)
    , side_dist_(0.0, 1.0)
{
}

OrderGenerator::~OrderGenerator() {
    stop();
}

void OrderGenerator::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    generator_thread_ = std::thread(&OrderGenerator::generatorLoop, this);
}

void OrderGenerator::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    if (generator_thread_.joinable()) {
        generator_thread_.join();
    }
}

void OrderGenerator::setConfig(const Config& config) {
    config_ = config;
    
    quantity_dist_ = std::uniform_int_distribution<>(config.min_quantity, config.max_quantity);
    interval_dist_ = std::uniform_int_distribution<>(config.min_interval_ms, config.max_interval_ms);
}

void OrderGenerator::updateLastPrice(double price) {
    last_price_ = price;
}

void OrderGenerator::generatorLoop() {
    while (running_) {
        // Generate a random order based on the last price
        OrderPtr order = generateOrder();
        
        // Call the callback if registered
        if (order_callback_) {
            order_callback_(order);
        }
        
        // Sleep for a random interval
        uint32_t interval_ms = interval_dist_(random_engine_);
        std::this_thread::sleep_for(std::chrono::microseconds(interval_ms));
    }
}

OrderPtr OrderGenerator::generateOrder() {    
    std::uniform_real_distribution<> price_dist(
        last_price_ * (1 - config_.price_range), 
        last_price_ * (1 + config_.price_range)
    );
    double order_price = price_dist(random_engine_);
    double rounded_price = std::round(order_price / config_.tick_size) * config_.tick_size;
    uint32_t quantity = quantity_dist_(random_engine_);
    
    OrderSide side = (side_dist_(random_engine_) < config_.buy_probability) 
                    ? OrderSide::BUY 
                    : OrderSide::SELL;
    
    // Generate an ID for the order
    std::string order_id = orderbook::generateUuid();
    
    // Create the order
    return std::make_shared<Order>(
        order_id,
        side,
        rounded_price,
        quantity,
        config_.symbol,
        "AUTO_GEN"
    );
}

}