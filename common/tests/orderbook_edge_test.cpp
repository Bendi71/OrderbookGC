#include <gtest/gtest.h>
#include "orderbook.h"
#include <vector>

using namespace orderbook;

namespace {

static int g_edge_counter = 0;

std::string genId() {
    return "edge_test_" + std::to_string(++g_edge_counter);
}

OrderPtr makeBuy(double price, uint32_t qty, const std::string& client = "client1") {
    return std::make_shared<Order>(genId(), OrderSide::BUY, price, qty, "TEST", client);
}

OrderPtr makeSell(double price, uint32_t qty, const std::string& client = "client1") {
    return std::make_shared<Order>(genId(), OrderSide::SELL, price, qty, "TEST", client);
}

} // namespace

// Empty book returns 0 for best prices
TEST(OrderbookEdge, EmptyBookBestPrices) {
    OrderBook book("TEST");
    EXPECT_DOUBLE_EQ(book.getBestBidPrice(), 0.0);
    EXPECT_DOUBLE_EQ(book.getBestAskPrice(), 0.0);
    EXPECT_EQ(book.getBestBidVolume(), 0);
    EXPECT_EQ(book.getBestAskVolume(), 0);
}

// Empty book returns empty levels
TEST(OrderbookEdge, EmptyBookLevels) {
    OrderBook book("TEST");
    EXPECT_TRUE(book.getBidLevels().empty());
    EXPECT_TRUE(book.getAskLevels().empty());
}

// Null order pointer
TEST(OrderbookEdge, NullOrderPointer) {
    OrderBook book("TEST");
    EXPECT_FALSE(book.addOrder(nullptr));
}

// Zero-quantity order
TEST(OrderbookEdge, ZeroQuantityOrder) {
    OrderBook book("TEST");
    auto order = std::make_shared<Order>(genId(), OrderSide::BUY, 100.0, 0, "TEST", "c1");
    EXPECT_FALSE(book.addOrder(order));
}

// Exact fill: incoming order quantity exactly matches resting order
TEST(OrderbookEdge, ExactFill) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    auto buy = makeBuy(100.0, 50);
    auto sell = makeSell(100.0, 50);
    book.addOrder(buy);
    book.addOrder(sell);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 50u);
    EXPECT_EQ(buy->getStatus(), OrderStatus::FILLED);
    EXPECT_EQ(sell->getStatus(), OrderStatus::FILLED);

    // Book should be empty
    EXPECT_DOUBLE_EQ(book.getBestBidPrice(), 0.0);
    EXPECT_DOUBLE_EQ(book.getBestAskPrice(), 0.0);
}

// Sell crossing multiple bid levels
TEST(OrderbookEdge, SellCrossesMultipleBidLevels) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    book.addOrder(makeBuy(100.0, 10));
    book.addOrder(makeBuy(101.0, 10));
    book.addOrder(makeBuy(102.0, 10));

    // Sell at 99.0 should cross all three levels (highest first)
    auto sell = makeSell(99.0, 25);
    book.addOrder(sell);

    ASSERT_EQ(trades.size(), 3u);
    EXPECT_DOUBLE_EQ(trades[0].price, 102.0);
    EXPECT_EQ(trades[0].quantity, 10u);
    EXPECT_DOUBLE_EQ(trades[1].price, 101.0);
    EXPECT_EQ(trades[1].quantity, 10u);
    EXPECT_DOUBLE_EQ(trades[2].price, 100.0);
    EXPECT_EQ(trades[2].quantity, 5u);

    EXPECT_EQ(sell->getRemainingQuantity(), 0u);
}

// Buy crossing multiple ask levels
TEST(OrderbookEdge, BuyCrossesMultipleAskLevels) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    book.addOrder(makeSell(100.0, 10));
    book.addOrder(makeSell(99.0, 10));
    book.addOrder(makeSell(98.0, 10));

    // Buy at 101.0 should cross all three ask levels (lowest first)
    auto buy = makeBuy(101.0, 25);
    book.addOrder(buy);

    ASSERT_EQ(trades.size(), 3u);
    EXPECT_DOUBLE_EQ(trades[0].price, 98.0);  // lowest ask first
    EXPECT_EQ(trades[0].quantity, 10u);
    EXPECT_DOUBLE_EQ(trades[1].price, 99.0);
    EXPECT_EQ(trades[1].quantity, 10u);
    EXPECT_DOUBLE_EQ(trades[2].price, 100.0);
    EXPECT_EQ(trades[2].quantity, 5u);

    EXPECT_EQ(buy->getRemainingQuantity(), 0u);
}

// Order that does not cross (no match)
TEST(OrderbookEdge, NoMatch) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    book.addOrder(makeBuy(99.0, 10));
    book.addOrder(makeSell(101.0, 10));

    EXPECT_TRUE(trades.empty());
    EXPECT_DOUBLE_EQ(book.getBestBidPrice(), 99.0);
    EXPECT_DOUBLE_EQ(book.getBestAskPrice(), 101.0);
}

// Incoming order partially fills and rests
TEST(OrderbookEdge, PartialFillAndRest) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    book.addOrder(makeSell(100.0, 5));

    auto buy = makeBuy(100.0, 20);
    book.addOrder(buy);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 5u);
    EXPECT_EQ(buy->getRemainingQuantity(), 15u);
    EXPECT_EQ(buy->getStatus(), OrderStatus::PARTIAL);

    // Buy should rest on the book
    EXPECT_DOUBLE_EQ(book.getBestBidPrice(), 100.0);
    EXPECT_EQ(book.getBestBidVolume(), 15);
}

// Get order by ID
TEST(OrderbookEdge, GetOrderById) {
    OrderBook book("TEST");
    auto buy = makeBuy(100.0, 10);
    book.addOrder(buy);

    auto found = book.getOrder(buy->getId());
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->getId(), buy->getId());
    EXPECT_DOUBLE_EQ(found->getPrice(), 100.0);
}

// Get non-existent order
TEST(OrderbookEdge, GetNonExistentOrder) {
    OrderBook book("TEST");
    EXPECT_EQ(book.getOrder("no_such_id"), nullptr);
}

// Multiple price levels depth query
TEST(OrderbookEdge, DepthLevels) {
    OrderBook book("TEST");

    for (int i = 0; i < 15; ++i) {
        book.addOrder(makeBuy(100.0 - i * 0.01, 10));
        book.addOrder(makeSell(101.0 + i * 0.01, 10));
    }

    auto bids = book.getBidLevels(5);
    auto asks = book.getAskLevels(5);

    EXPECT_EQ(bids.size(), 5u);
    EXPECT_EQ(asks.size(), 5u);

    // Best bid should be highest
    EXPECT_NEAR(bids[0].price, 100.0, 1e-9);
    // Best ask should be lowest
    EXPECT_NEAR(asks[0].price, 101.0, 1e-9);
}

// Tick size change
TEST(OrderbookEdge, TickSizeChange) {
    OrderBook book("TEST", 0.01);
    EXPECT_DOUBLE_EQ(book.getTickSize(), 0.01);

    book.setTickSize(0.05);
    EXPECT_DOUBLE_EQ(book.getTickSize(), 0.05);

    // Invalid tick size should be ignored
    book.setTickSize(-1.0);
    EXPECT_DOUBLE_EQ(book.getTickSize(), 0.05);

    book.setTickSize(0.0);
    EXPECT_DOUBLE_EQ(book.getTickSize(), 0.05);
}
