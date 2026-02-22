#include <gtest/gtest.h>
#include "orderbook.h"
#include <vector>
#include <cmath>
#include <memory>
#include <string>

using namespace orderbook;

namespace {

// Counter for unique order IDs per test
static int g_order_counter = 0;

std::string generateOrderId() {
    return "test_order_" + std::to_string(++g_order_counter);
}

OrderPtr createBuyOrder(double price, uint32_t quantity) {
    return std::make_shared<Order>(
        generateOrderId(), OrderSide::BUY, price, quantity, "TEST", "test_client");
}

OrderPtr createSellOrder(double price, uint32_t quantity) {
    return std::make_shared<Order>(
        generateOrderId(), OrderSide::SELL, price, quantity, "TEST", "test_client");
}

} // namespace

TEST(OrderbookMatching, BasicMatching) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    auto buy = createBuyOrder(100.0, 10);
    book.addOrder(buy);

    EXPECT_DOUBLE_EQ(book.getBestBidPrice(), 100.0);
    EXPECT_EQ(book.getBestBidVolume(), 10);

    auto sell = createSellOrder(100.0, 5);
    book.addOrder(sell);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 5u);
    EXPECT_DOUBLE_EQ(trades[0].price, 100.0);

    EXPECT_EQ(buy->getRemainingQuantity(), 5u);
    EXPECT_EQ(buy->getStatus(), OrderStatus::PARTIAL);
    EXPECT_EQ(sell->getRemainingQuantity(), 0u);
    EXPECT_EQ(sell->getStatus(), OrderStatus::FILLED);

    EXPECT_DOUBLE_EQ(book.getBestBidPrice(), 100.0);
    EXPECT_EQ(book.getBestBidVolume(), 5);
}

TEST(OrderbookMatching, PricePriority) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    auto buy1 = createBuyOrder(100.0, 10);
    auto buy2 = createBuyOrder(101.0, 10);
    auto buy3 = createBuyOrder(99.0, 10);
    book.addOrder(buy1);
    book.addOrder(buy2);
    book.addOrder(buy3);

    EXPECT_DOUBLE_EQ(book.getBestBidPrice(), 101.0);

    auto sell = createSellOrder(99.0, 15);
    book.addOrder(sell);

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].quantity, 10u);
    EXPECT_DOUBLE_EQ(trades[0].price, 101.0);
    EXPECT_EQ(trades[1].quantity, 5u);
    EXPECT_DOUBLE_EQ(trades[1].price, 100.0);

    EXPECT_EQ(buy2->getRemainingQuantity(), 0u);
    EXPECT_EQ(buy1->getRemainingQuantity(), 5u);
    EXPECT_EQ(buy3->getRemainingQuantity(), 10u);
    EXPECT_EQ(sell->getRemainingQuantity(), 0u);
}

TEST(OrderbookMatching, TimePriority) {
    OrderBook book("TEST");
    std::vector<std::string> filled_ids;
    book.setTradeCallback([&](const Trade& t) { filled_ids.push_back(t.buy_order_id); });

    auto buy1 = createBuyOrder(100.0, 10);
    auto buy2 = createBuyOrder(100.0, 10);
    auto buy3 = createBuyOrder(100.0, 10);
    book.addOrder(buy1);
    book.addOrder(buy2);
    book.addOrder(buy3);

    auto sell = createSellOrder(100.0, 25);
    book.addOrder(sell);

    EXPECT_EQ(buy1->getRemainingQuantity(), 0u);
    EXPECT_EQ(buy2->getRemainingQuantity(), 0u);
    EXPECT_EQ(buy3->getRemainingQuantity(), 5u);
    EXPECT_EQ(sell->getRemainingQuantity(), 0u);

    ASSERT_EQ(filled_ids.size(), 3u);
    EXPECT_EQ(filled_ids[0], buy1->getId());
    EXPECT_EQ(filled_ids[1], buy2->getId());
    EXPECT_EQ(filled_ids[2], buy3->getId());
}

TEST(OrderbookMatching, TickSizeRounding) {
    OrderBook book("TEST", 0.05);

    EXPECT_DOUBLE_EQ(book.getTickSize(), 0.05);

    EXPECT_NEAR(book.roundToTickSize(100.02), 100.0, 1e-9);
    EXPECT_NEAR(book.roundToTickSize(100.03), 100.05, 1e-9);
    EXPECT_NEAR(book.roundToTickSize(100.074), 100.05, 1e-9);
    EXPECT_NEAR(book.roundToTickSize(100.075), 100.1, 1e-9);

    EXPECT_TRUE(book.isValidPrice(100.0));
    EXPECT_TRUE(book.isValidPrice(100.05));
    EXPECT_TRUE(book.isValidPrice(100.1));
    EXPECT_FALSE(book.isValidPrice(100.02));
}