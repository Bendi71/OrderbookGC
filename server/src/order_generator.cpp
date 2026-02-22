#include "order_generator.h"
#include "uuid.h"
#include <chrono>
#include <cmath>

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
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;
    
    quantity_dist_ = std::uniform_int_distribution<>(config.min_quantity, config.max_quantity);
    interval_dist_ = std::uniform_int_distribution<>(config.min_interval_ms, config.max_interval_ms);
}

void OrderGenerator::updateLastPrice(double price) {
    last_price_.store(price, std::memory_order_relaxed);
}

void OrderGenerator::generatorLoop() {
    while (running_) {
        // Generate a random order based on the last price
        OrderPtr order = generateOrder();
        
        // Track the order ID for potential cancellation
        {
            std::lock_guard<std::mutex> lock(tracked_mutex_);
            tracked_order_ids_.push_back(order->getId());
            // Keep the deque bounded
            uint32_t max_tracked;
            {
                std::lock_guard<std::mutex> cfg_lock(config_mutex_);
                max_tracked = config_.max_tracked_orders;
            }
            while (tracked_order_ids_.size() > max_tracked) {
                tracked_order_ids_.pop_front();
            }
        }

        // Call the callback if registered
        if (order_callback_) {
            order_callback_(order);
            order_count_.fetch_add(1, std::memory_order_relaxed);
        }

        // Attempt a cancel with configured probability
        tryCancel();
        
        // Sleep for a random interval
        uint32_t interval_ms = interval_dist_(random_engine_);
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}

OrderPtr OrderGenerator::generateOrder() {    
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    double current_price = last_price_.load(std::memory_order_relaxed);

    // Gaussian price distribution centered on current_price
    // stddev is set so ±3σ covers approximately the configured price_range
    double stddev = current_price * config_.price_range / 3.0;
    std::normal_distribution<> price_dist(current_price, stddev);
    double order_price = price_dist(random_engine_);

    // Clamp to positive price  
    if (order_price <= 0.0) {
        order_price = config_.tick_size;
    }

    // Round to tick size
    long long ticks = std::llround(order_price / config_.tick_size);
    double rounded_price = ticks * config_.tick_size;
    if (rounded_price <= 0.0) {
        rounded_price = config_.tick_size;
    }

    uint32_t quantity = quantity_dist_(random_engine_);

    // Spread-aware side selection
    // If spread_factor > 0, bias buys below current_price and sells above
    OrderSide side;
    if (config_.spread_factor > 0.0) {
        // sigmoid((current_price - order_price) / (current_price * spread_factor))
        double x = (current_price - rounded_price) / (current_price * config_.spread_factor);
        double buy_prob = 1.0 / (1.0 + std::exp(-x));
        side = (side_dist_(random_engine_) < buy_prob) ? OrderSide::BUY : OrderSide::SELL;
    } else {
        side = (side_dist_(random_engine_) < config_.buy_probability)
                    ? OrderSide::BUY 
                    : OrderSide::SELL;
    }
    
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

void OrderGenerator::tryCancel() {
    double cancel_prob;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        cancel_prob = config_.cancel_probability;
    }

    if (cancel_prob <= 0.0 || !cancel_callback_) {
        return;
    }

    if (side_dist_(random_engine_) >= cancel_prob) {
        return;
    }

    std::string order_id_to_cancel;
    {
        std::lock_guard<std::mutex> lock(tracked_mutex_);
        if (tracked_order_ids_.empty()) {
            return;
        }
        // Pick a random order from the tracked deque
        std::uniform_int_distribution<size_t> idx_dist(0, tracked_order_ids_.size() - 1);
        size_t idx = idx_dist(random_engine_);
        order_id_to_cancel = tracked_order_ids_[idx];
        // Remove it from tracking
        tracked_order_ids_.erase(tracked_order_ids_.begin() + static_cast<std::ptrdiff_t>(idx));
    }

    if (cancel_callback_(order_id_to_cancel)) {
        cancel_count_.fetch_add(1, std::memory_order_relaxed);
    }
}

}