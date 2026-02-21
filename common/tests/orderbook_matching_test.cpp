#include "orderbook.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <memory>
#include <string>

using namespace orderbook;

// Floating-point approximate comparison (avoids IEEE 754 representation mismatches)
static bool approxEqual(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) < eps;
}

// Helper function to generate order IDs
std::string generateOrderId() {
    static int counter = 0;
    return "test_order_" + std::to_string(++counter);
}

// Helper to create buy orders
OrderPtr createBuyOrder(double price, uint32_t quantity) {
    return std::make_shared<Order>(
        generateOrderId(),
        OrderSide::BUY,
        price,
        quantity,
        "TEST",
        "test_client"
    );
}

// Helper to create sell orders
OrderPtr createSellOrder(double price, uint32_t quantity) {
    return std::make_shared<Order>(
        generateOrderId(),
        OrderSide::SELL,
        price,
        quantity,
        "TEST",
        "test_client"
    );
}

// Test cases
bool testBasicMatching() {
    OrderBook book("TEST");
    bool success = true;
    
    // Track executed trades
    std::vector<Trade> executed_trades;
    book.setTradeCallback([&executed_trades](const Trade& trade) {
        executed_trades.push_back(trade);
    });
    
    // Add a buy order
    auto buy_order = createBuyOrder(100.0, 10);
    book.addOrder(buy_order);
    
    // Verify it's in the book
    assert(book.getBestBidPrice() == 100.0);
    assert(book.getBestBidVolume() == 10);
    
    // Add a matching sell order
    auto sell_order = createSellOrder(100.0, 5);
    book.addOrder(sell_order);
    
    // Verify matching occurred
    assert(executed_trades.size() == 1);
    assert(executed_trades[0].quantity == 5);
    assert(executed_trades[0].price == 100.0);
    
    // Verify buy order was partially filled
    assert(buy_order->getRemainingQuantity() == 5);
    assert(buy_order->getStatus() == OrderStatus::PARTIAL);
    
    // Verify sell order was fully filled
    assert(sell_order->getRemainingQuantity() == 0);
    assert(sell_order->getStatus() == OrderStatus::FILLED);
    
    // Verify the book state
    assert(book.getBestBidPrice() == 100.0);
    assert(book.getBestBidVolume() == 5);
    
    return success;
}

bool testPricePriority() {
    OrderBook book("TEST");
    bool success = true;
    
    // Track executed trades
    std::vector<Trade> executed_trades;
    book.setTradeCallback([&executed_trades](const Trade& trade) {
        executed_trades.push_back(trade);
    });
    
    // Add buy orders at different prices
    auto buy_order1 = createBuyOrder(100.0, 10);
    auto buy_order2 = createBuyOrder(101.0, 10);
    auto buy_order3 = createBuyOrder(99.0, 10);
    
    book.addOrder(buy_order1);
    book.addOrder(buy_order2);
    book.addOrder(buy_order3);
    
    // Verify best bid is highest price
    assert(book.getBestBidPrice() == 101.0);
    
    // Add a matching sell order that should match at two price levels
    auto sell_order = createSellOrder(99.0, 15);
    book.addOrder(sell_order);
    
    // Verify matching occurred: first with 101.0 (10 qty), then with 100.0 (5 qty)
    assert(executed_trades.size() == 2);
    assert(executed_trades[0].quantity == 10);
    assert(executed_trades[0].price == 101.0);  // Resting order's price
    assert(executed_trades[1].quantity == 5);
    assert(executed_trades[1].price == 100.0);  // Resting order's price
    
    // Check remaining quantities
    assert(buy_order2->getRemainingQuantity() == 0);
    assert(buy_order1->getRemainingQuantity() == 5);
    assert(buy_order3->getRemainingQuantity() == 10);
    assert(sell_order->getRemainingQuantity() == 0);
    
    return success;
}

bool testTimePriority() {
    OrderBook book("TEST");
    bool success = true;
    
    // Track executed trades and order of fills
    std::vector<std::string> filled_order_ids;
    book.setTradeCallback([&filled_order_ids](const Trade& trade) {
        filled_order_ids.push_back(trade.buy_order_id);
    });
    
    // Add multiple buy orders at the same price (should be filled in FIFO order)
    auto buy_order1 = createBuyOrder(100.0, 10);
    auto buy_order2 = createBuyOrder(100.0, 10);
    auto buy_order3 = createBuyOrder(100.0, 10);
    
    book.addOrder(buy_order1);
    book.addOrder(buy_order2);
    book.addOrder(buy_order3);
    
    // Add a matching sell order
    auto sell_order = createSellOrder(100.0, 25);
    book.addOrder(sell_order);
    
    // Verify order1 was filled first, then order2, then order3 (partially)
    assert(buy_order1->getRemainingQuantity() == 0);
    assert(buy_order2->getRemainingQuantity() == 0);
    assert(buy_order3->getRemainingQuantity() == 5);
    assert(sell_order->getRemainingQuantity() == 0);
    
    // Verify time priority was respected
    assert(filled_order_ids.size() == 3);
    assert(filled_order_ids[0] == buy_order1->getId());
    assert(filled_order_ids[1] == buy_order2->getId());
    assert(filled_order_ids[2] == buy_order3->getId());
    
    return success;
}

bool testTickSizeRounding() {
    OrderBook book("TEST", 0.05);
    bool success = true;
    
    // Verify tick size is set correctly
    assert(book.getTickSize() == 0.05);
    
    // Test price rounding (using approxEqual to avoid IEEE 754 representation mismatches)
    assert(approxEqual(book.roundToTickSize(100.02), 100.0));
    assert(approxEqual(book.roundToTickSize(100.03), 100.05));
    assert(approxEqual(book.roundToTickSize(100.074), 100.05));
    assert(approxEqual(book.roundToTickSize(100.075), 100.1));
    
    // Test price validation
    assert(book.isValidPrice(100.0) == true);
    assert(book.isValidPrice(100.05) == true);
    assert(book.isValidPrice(100.1) == true);
    assert(book.isValidPrice(100.02) == false);
    
    return success;
}

// Main function to run all tests
int main() {
    int test_count = 0;
    int passed_count = 0;
    
    std::cout << "Running order matching integration tests..." << std::endl;
    
    // Run test cases
    if (testBasicMatching()) {
        std::cout << "✓ Basic matching test passed" << std::endl;
        passed_count++;
    } else {
        std::cout << "✗ Basic matching test failed" << std::endl;
    }
    test_count++;
    
    if (testPricePriority()) {
        std::cout << "✓ Price priority test passed" << std::endl;
        passed_count++;
    } else {
        std::cout << "✗ Price priority test failed" << std::endl;
    }
    test_count++;
    
    if (testTimePriority()) {
        std::cout << "✓ Time priority test passed" << std::endl;
        passed_count++;
    } else {
        std::cout << "✗ Time priority test failed" << std::endl;
    }
    test_count++;
    
    if (testTickSizeRounding()) {
        std::cout << "✓ Tick size rounding test passed" << std::endl;
        passed_count++;
    } else {
        std::cout << "✗ Tick size rounding test failed" << std::endl;
    }
    test_count++;
    
    // Report results
    std::cout << "\nTest Results: " << passed_count << " of " << test_count << " tests passed" << std::endl;
    
    // Return non-zero exit code if any test failed
    return (passed_count == test_count) ? 0 : 1;
}
