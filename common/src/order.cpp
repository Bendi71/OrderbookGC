#include "order.h"

namespace orderbook {

Order::Order(const std::string& id,
             OrderSide side,
             double price,
             uint32_t quantity,
             const std::string& symbol,
             const std::string& client_id,
             OrderType type)
    : id_(id)
    , client_id_(client_id)
    , symbol_(symbol)
    , side_(side)
    , order_type_(type)
    , price_(price)
    , quantity_(quantity)
    , remaining_quantity_(quantity)
    , status_(OrderStatus::PENDING)
    , timestamp_(std::chrono::system_clock::now())
{
}

bool Order::fill(uint32_t quantity) {
    // Check if the order can be filled
    if (status_ == OrderStatus::CANCELED || status_ == OrderStatus::REJECTED || status_ == OrderStatus::FILLED) {
        return false;
    }
    
    if (quantity > remaining_quantity_) {
        return false;
    }
    
    remaining_quantity_ -= quantity;
    
    if (remaining_quantity_ == 0) {
        status_ = OrderStatus::FILLED;
    } else {
        status_ = OrderStatus::PARTIAL;
    }
    
    return true;
}

bool Order::cancel() {
    // Check if the order can be canceled
    if (status_ == OrderStatus::FILLED || status_ == OrderStatus::CANCELED || status_ == OrderStatus::REJECTED) {
        return false;
    }
    
    status_ = OrderStatus::CANCELED;
    
    return true;
}

bool Order::comparePriceAscending(const std::shared_ptr<Order>& a, const std::shared_ptr<Order>& b) {
    // Sort by price (ascending)
    if (a->getPrice() != b->getPrice()) {
        return a->getPrice() < b->getPrice();
    }
    
    // If prices are equal, sort by timestamp (ascending) -> FIFO
    return a->getTimestamp() < b->getTimestamp();
}

bool Order::comparePriceDescending(const std::shared_ptr<Order>& a, const std::shared_ptr<Order>& b) {
    // Sort by price (descending)
    if (a->getPrice() != b->getPrice()) {
        return a->getPrice() > b->getPrice();
    }
    
    // If prices are equal, sort by timestamp (ascending) -> FIFO
    return a->getTimestamp() < b->getTimestamp();
}

}