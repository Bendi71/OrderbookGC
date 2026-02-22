#include <gtest/gtest.h>
#include "orderbook.h"
#include <vector>

using namespace orderbook;

namespace {

static int g_cancel_counter = 0;

std::string genId() {
    return "cancel_test_" + std::to_string(++g_cancel_counter);
}

OrderPtr makeBuy(double price, uint32_t qty) {
    return std::make_shared<Order>(genId(), OrderSide::BUY, price, qty, "TEST", "client1");
}

OrderPtr makeSell(double price, uint32_t qty) {
    return std::make_shared<Order>(genId(), OrderSide::SELL, price, qty, "TEST", "client1");
}

} // namespace

// Cancel a resting buy order and verify book updates
TEST(OrderbookCancel, CancelRestingBuyOrder) {
    OrderBook book("TEST");

    auto buy1 = makeBuy(100.0, 10);
    auto buy2 = makeBuy(101.0, 20);
    book.addOrder(buy1);
    book.addOrder(buy2);

    EXPECT_DOUBLE_EQ(book.getBestBidPrice(), 101.0);

    EXPECT_TRUE(book.cancelOrder(buy2->getId()));
    EXPECT_EQ(buy2->getStatus(), OrderStatus::CANCELED);

    // Best bid should now be the remaining order
    EXPECT_DOUBLE_EQ(book.getBestBidPrice(), 100.0);
    EXPECT_EQ(book.getBestBidVolume(), 10);
}

// Cancel a resting sell order
TEST(OrderbookCancel, CancelRestingSellOrder) {
    OrderBook book("TEST");

    auto sell1 = makeSell(100.0, 10);
    auto sell2 = makeSell(99.0, 20);
    book.addOrder(sell1);
    book.addOrder(sell2);

    EXPECT_DOUBLE_EQ(book.getBestAskPrice(), 99.0);

    EXPECT_TRUE(book.cancelOrder(sell2->getId()));
    EXPECT_DOUBLE_EQ(book.getBestAskPrice(), 100.0);
}

// Cancel a non-existent order ID
TEST(OrderbookCancel, CancelNonExistentOrder) {
    OrderBook book("TEST");
    EXPECT_FALSE(book.cancelOrder("non_existent_id_12345"));
}

// Cancel an already-filled order should fail
TEST(OrderbookCancel, CancelFilledOrder) {
    OrderBook book("TEST");

    auto buy = makeBuy(100.0, 10);
    auto sell = makeSell(100.0, 10);
    book.addOrder(buy);
    book.addOrder(sell);

    EXPECT_EQ(buy->getStatus(), OrderStatus::FILLED);
    // The filled order should have already been removed from the book
    EXPECT_FALSE(book.cancelOrder(buy->getId()));
}

// Cancel a partially-filled order
TEST(OrderbookCancel, CancelPartiallyFilledOrder) {
    OrderBook book("TEST");
    std::vector<OrderPtr> updated_orders;
    book.setOrderCallback([&](const OrderPtr& o) { updated_orders.push_back(o); });

    auto buy = makeBuy(100.0, 20);
    auto sell = makeSell(100.0, 5);
    book.addOrder(buy);
    book.addOrder(sell);

    EXPECT_EQ(buy->getRemainingQuantity(), 15u);
    EXPECT_EQ(buy->getStatus(), OrderStatus::PARTIAL);

    EXPECT_TRUE(book.cancelOrder(buy->getId()));
    EXPECT_EQ(buy->getStatus(), OrderStatus::CANCELED);

    // Book should be empty on bid side
    EXPECT_DOUBLE_EQ(book.getBestBidPrice(), 0.0);
}

// Cancel the last order at a price level removes the level
TEST(OrderbookCancel, CancelLastOrderAtPriceLevel) {
    OrderBook book("TEST");

    auto buy1 = makeBuy(100.0, 10);
    auto buy2 = makeBuy(101.0, 10);
    book.addOrder(buy1);
    book.addOrder(buy2);

    // Cancel the only order at 101.0
    EXPECT_TRUE(book.cancelOrder(buy2->getId()));

    auto levels = book.getBidLevels();
    ASSERT_EQ(levels.size(), 1u);
    EXPECT_DOUBLE_EQ(levels[0].price, 100.0);
}

// Cancel one of multiple orders at same price level
TEST(OrderbookCancel, CancelOneOfMultipleAtSamePrice) {
    OrderBook book("TEST");

    auto buy1 = makeBuy(100.0, 10);
    auto buy2 = makeBuy(100.0, 20);
    auto buy3 = makeBuy(100.0, 30);
    book.addOrder(buy1);
    book.addOrder(buy2);
    book.addOrder(buy3);

    EXPECT_TRUE(book.cancelOrder(buy2->getId()));

    auto levels = book.getBidLevels();
    ASSERT_EQ(levels.size(), 1u);
    EXPECT_EQ(levels[0].quantity, 40u);  // 10 + 30
    EXPECT_EQ(levels[0].order_count, 2);

    // Verify FIFO is preserved: buy1 should fill before buy3
    std::vector<std::string> filled_ids;
    book.setTradeCallback([&](const Trade& t) { filled_ids.push_back(t.buy_order_id); });

    auto sell = makeSell(100.0, 40);
    book.addOrder(sell);

    ASSERT_EQ(filled_ids.size(), 2u);
    EXPECT_EQ(filled_ids[0], buy1->getId());
    EXPECT_EQ(filled_ids[1], buy3->getId());
}
