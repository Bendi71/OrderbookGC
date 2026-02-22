#include <gtest/gtest.h>
#include "order.h"

using namespace orderbook;

// ============================================================
// Construction tests
// ============================================================

TEST(Order, ConstructorSetsFields) {
    Order order("id-1", OrderSide::BUY, 100.5, 50, "AAPL", "client-1");

    EXPECT_EQ(order.getId(), "id-1");
    EXPECT_EQ(order.getClientId(), "client-1");
    EXPECT_EQ(order.getSymbol(), "AAPL");
    EXPECT_EQ(order.getSide(), OrderSide::BUY);
    EXPECT_DOUBLE_EQ(order.getPrice(), 100.5);
    EXPECT_EQ(order.getQuantity(), 50u);
    EXPECT_EQ(order.getRemainingQuantity(), 50u);
    EXPECT_EQ(order.getStatus(), OrderStatus::PENDING);
}

TEST(Order, SellOrder) {
    Order order("id-2", OrderSide::SELL, 200.0, 10, "MSFT", "client-2");
    EXPECT_EQ(order.getSide(), OrderSide::SELL);
}

// ============================================================
// Fill tests
// ============================================================

TEST(Order, PartialFill) {
    Order order("id-3", OrderSide::BUY, 100.0, 100, "TEST", "c");

    EXPECT_TRUE(order.fill(30));
    EXPECT_EQ(order.getRemainingQuantity(), 70u);
    EXPECT_EQ(order.getStatus(), OrderStatus::PARTIAL);
}

TEST(Order, FullFill) {
    Order order("id-4", OrderSide::BUY, 100.0, 100, "TEST", "c");

    EXPECT_TRUE(order.fill(100));
    EXPECT_EQ(order.getRemainingQuantity(), 0u);
    EXPECT_EQ(order.getStatus(), OrderStatus::FILLED);
}

TEST(Order, MultiplePartialFillsThenFull) {
    Order order("id-5", OrderSide::SELL, 50.0, 30, "TEST", "c");

    EXPECT_TRUE(order.fill(10));
    EXPECT_EQ(order.getRemainingQuantity(), 20u);
    EXPECT_EQ(order.getStatus(), OrderStatus::PARTIAL);

    EXPECT_TRUE(order.fill(15));
    EXPECT_EQ(order.getRemainingQuantity(), 5u);
    EXPECT_EQ(order.getStatus(), OrderStatus::PARTIAL);

    EXPECT_TRUE(order.fill(5));
    EXPECT_EQ(order.getRemainingQuantity(), 0u);
    EXPECT_EQ(order.getStatus(), OrderStatus::FILLED);
}

TEST(Order, OverfillRejected) {
    Order order("id-6", OrderSide::BUY, 100.0, 10, "TEST", "c");

    // Trying to fill more than remaining should fail
    EXPECT_FALSE(order.fill(11));
    EXPECT_EQ(order.getRemainingQuantity(), 10u);
    EXPECT_EQ(order.getStatus(), OrderStatus::PENDING);
}

TEST(Order, FillAlreadyFilledOrder) {
    Order order("id-7", OrderSide::BUY, 100.0, 10, "TEST", "c");
    EXPECT_TRUE(order.fill(10));
    EXPECT_EQ(order.getStatus(), OrderStatus::FILLED);

    // Cannot fill an already-filled order
    EXPECT_FALSE(order.fill(1));
}

TEST(Order, FillCanceledOrder) {
    Order order("id-8", OrderSide::BUY, 100.0, 10, "TEST", "c");
    order.cancel();
    EXPECT_EQ(order.getStatus(), OrderStatus::CANCELED);

    EXPECT_FALSE(order.fill(5));
}

// ============================================================
// Cancel tests
// ============================================================

TEST(Order, CancelPendingOrder) {
    Order order("id-9", OrderSide::BUY, 100.0, 10, "TEST", "c");

    EXPECT_TRUE(order.cancel());
    EXPECT_EQ(order.getStatus(), OrderStatus::CANCELED);
}

TEST(Order, CancelPartialOrder) {
    Order order("id-10", OrderSide::SELL, 50.0, 20, "TEST", "c");
    order.fill(5);
    EXPECT_EQ(order.getStatus(), OrderStatus::PARTIAL);

    EXPECT_TRUE(order.cancel());
    EXPECT_EQ(order.getStatus(), OrderStatus::CANCELED);
}

TEST(Order, CancelFilledOrderFails) {
    Order order("id-11", OrderSide::BUY, 100.0, 10, "TEST", "c");
    order.fill(10);

    EXPECT_FALSE(order.cancel());
    EXPECT_EQ(order.getStatus(), OrderStatus::FILLED);
}

TEST(Order, CancelAlreadyCanceledFails) {
    Order order("id-12", OrderSide::BUY, 100.0, 10, "TEST", "c");
    order.cancel();

    EXPECT_FALSE(order.cancel());
    EXPECT_EQ(order.getStatus(), OrderStatus::CANCELED);
}

// ============================================================
// Comparison functions
// ============================================================

TEST(Order, PriceAscendingComparison) {
    auto a = std::make_shared<Order>("a", OrderSide::BUY, 100.0, 10, "T", "c");
    auto b = std::make_shared<Order>("b", OrderSide::BUY, 101.0, 10, "T", "c");

    EXPECT_TRUE(Order::comparePriceAscending(a, b));
    EXPECT_FALSE(Order::comparePriceAscending(b, a));
}

TEST(Order, PriceDescendingComparison) {
    auto a = std::make_shared<Order>("a", OrderSide::BUY, 100.0, 10, "T", "c");
    auto b = std::make_shared<Order>("b", OrderSide::BUY, 101.0, 10, "T", "c");

    EXPECT_FALSE(Order::comparePriceDescending(a, b));
    EXPECT_TRUE(Order::comparePriceDescending(b, a));
}

// ============================================================
// Timestamp
// ============================================================

TEST(Order, TimestampIsSet) {
    auto before = std::chrono::system_clock::now();
    Order order("id-ts", OrderSide::BUY, 100.0, 10, "TEST", "c");
    auto after = std::chrono::system_clock::now();

    EXPECT_GE(order.getTimestamp(), before);
    EXPECT_LE(order.getTimestamp(), after);
}
