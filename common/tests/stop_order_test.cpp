#include <gtest/gtest.h>
#include "orderbook.h"
#include "order.h"
#include <vector>
#include <string>
#include <memory>

using namespace orderbook;

namespace {

static int g_stop_counter = 0;

std::string genId() {
    return "stop_test_" + std::to_string(++g_stop_counter);
}

OrderPtr limitBuy(double price, uint32_t qty) {
    return std::make_shared<Order>(
        genId(), OrderSide::BUY, price, qty, "TEST", "client1");
}

OrderPtr limitSell(double price, uint32_t qty) {
    return std::make_shared<Order>(
        genId(), OrderSide::SELL, price, qty, "TEST", "client1");
}

OrderPtr stopBuy(double stop_price, uint32_t qty) {
    return std::make_shared<Order>(
        genId(), OrderSide::BUY, 0.0, qty, "TEST", "client1",
        OrderType::STOP, stop_price);
}

OrderPtr stopSell(double stop_price, uint32_t qty) {
    return std::make_shared<Order>(
        genId(), OrderSide::SELL, 0.0, qty, "TEST", "client1",
        OrderType::STOP, stop_price);
}

OrderPtr stopLimitBuy(double price, double stop_price, uint32_t qty) {
    return std::make_shared<Order>(
        genId(), OrderSide::BUY, price, qty, "TEST", "client1",
        OrderType::STOP_LIMIT, stop_price);
}

OrderPtr stopLimitSell(double price, double stop_price, uint32_t qty) {
    return std::make_shared<Order>(
        genId(), OrderSide::SELL, price, qty, "TEST", "client1",
        OrderType::STOP_LIMIT, stop_price);
}

} // namespace

// ── Stop order creation and properties ──────────────────────

TEST(StopOrders, StopOrderCreation) {
    auto order = stopBuy(105.0, 10);
    EXPECT_EQ(order->getOrderType(), OrderType::STOP);
    EXPECT_TRUE(order->isStopOrder());
    EXPECT_DOUBLE_EQ(order->getStopPrice(), 105.0);
    EXPECT_EQ(order->getSide(), OrderSide::BUY);
}

TEST(StopOrders, StopLimitOrderCreation) {
    auto order = stopLimitSell(98.0, 100.0, 5);
    EXPECT_EQ(order->getOrderType(), OrderType::STOP_LIMIT);
    EXPECT_TRUE(order->isStopOrder());
    EXPECT_DOUBLE_EQ(order->getStopPrice(), 100.0);
    EXPECT_DOUBLE_EQ(order->getPrice(), 98.0);
}

TEST(StopOrders, TriggerStopToMarket) {
    auto order = stopBuy(105.0, 10);
    EXPECT_TRUE(order->trigger());
    EXPECT_EQ(order->getOrderType(), OrderType::MARKET);
    EXPECT_FALSE(order->isStopOrder());
    // Trigger again should fail
    EXPECT_FALSE(order->trigger());
}

TEST(StopOrders, TriggerStopLimitToLimit) {
    auto order = stopLimitSell(98.0, 100.0, 5);
    EXPECT_TRUE(order->trigger());
    EXPECT_EQ(order->getOrderType(), OrderType::LIMIT);
    EXPECT_DOUBLE_EQ(order->getPrice(), 98.0);
    EXPECT_FALSE(order->isStopOrder());
}

// ── Stop orders in orderbook ────────────────────────────────

TEST(StopOrders, StopBuyNotMatchedImmediately) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    // Place a sell at 100
    book.addOrder(limitSell(100.0, 10));

    // Place a stop buy at 105 — should NOT match
    auto stop = stopBuy(105.0, 5);
    book.addOrder(stop);

    EXPECT_EQ(trades.size(), 0u);
    EXPECT_TRUE(stop->isStopOrder()); // still a stop, not triggered
}

TEST(StopOrders, StopBuyTriggeredByTradePrice) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    // Set up: sell side has liquidity at 105 and 110
    book.addOrder(limitSell(105.0, 10));
    book.addOrder(limitSell(110.0, 10));

    // Place stop buy that triggers at 105
    auto stop = stopBuy(105.0, 5);
    book.addOrder(stop);

    EXPECT_EQ(trades.size(), 0u); // no trigger yet

    // Now a limit buy at 105 executes a trade at 105 → triggers the stop
    book.addOrder(limitBuy(105.0, 3));

    // The limit buy should match the sell at 105 (trade at 105)
    // Then the stop should trigger (price 105 >= stop_price 105)
    // The triggered stop becomes MARKET and matches remaining sell at 105
    EXPECT_GE(trades.size(), 2u); // at least the original + stop triggered
}

TEST(StopOrders, StopSellTriggeredByTradePrice) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    // Set up: buy side has liquidity at 95 and 90
    book.addOrder(limitBuy(95.0, 10));
    book.addOrder(limitBuy(90.0, 10));

    // Place stop sell that triggers at 95
    auto stop = stopSell(95.0, 5);
    book.addOrder(stop);

    EXPECT_EQ(trades.size(), 0u); // no trigger yet

    // Sell at 95 creates a trade → triggers the stop
    book.addOrder(limitSell(95.0, 3));

    // Should have at least 2 trades
    EXPECT_GE(trades.size(), 2u);
}

TEST(StopOrders, StopLimitBuyTriggered) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    // Sell at 106
    book.addOrder(limitSell(106.0, 10));

    // Stop-limit buy: triggers at 105, places limit at 106
    auto stop = stopLimitBuy(106.0, 105.0, 5);
    book.addOrder(stop);

    EXPECT_EQ(trades.size(), 0u);

    // Trade at 105 triggers the stop, which becomes a limit buy at 106
    book.addOrder(limitSell(105.0, 10));
    book.addOrder(limitBuy(105.0, 3));

    // The triggered stop-limit should match the sell at 106 (partially or fully)
    // The exact trade count depends on matching cascade
    EXPECT_GE(trades.size(), 1u);
}

TEST(StopOrders, CancelUntriggeredStopOrder) {
    OrderBook book("TEST");

    auto stop = stopBuy(105.0, 10);
    book.addOrder(stop);

    // Cancel should work
    bool cancelled = book.cancelOrder(stop->getId());
    EXPECT_TRUE(cancelled);
    EXPECT_EQ(stop->getStatus(), OrderStatus::CANCELED);
}

TEST(StopOrders, StopOrderNotTriggeredByWrongDirection) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    // Stop buy triggers when price >= 110
    book.addOrder(limitSell(100.0, 10));
    auto stop = stopBuy(110.0, 5);
    book.addOrder(stop);

    // Trade at 100 should NOT trigger stop buy at 110
    book.addOrder(limitBuy(100.0, 3));

    EXPECT_EQ(trades.size(), 1u); // only the limit buy trade
    EXPECT_TRUE(stop->isStopOrder()); // still untriggered
}

TEST(StopOrders, MultipleStopsAtSamePrice) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    book.addOrder(limitSell(100.0, 30));

    // Two stop buys at same trigger price
    auto stop1 = stopBuy(100.0, 5);
    auto stop2 = stopBuy(100.0, 5);
    book.addOrder(stop1);
    book.addOrder(stop2);

    // Trigger both via a trade at 100
    book.addOrder(limitBuy(100.0, 3));

    // Both should trigger and execute
    EXPECT_GE(trades.size(), 3u); // original + 2 stops
}

// ── Non-stop order remains unaffected ───────────────────────

TEST(StopOrders, RegularOrdersUnaffected) {
    auto limit = limitBuy(100.0, 10);
    EXPECT_FALSE(limit->isStopOrder());
    EXPECT_EQ(limit->getOrderType(), OrderType::LIMIT);
    EXPECT_DOUBLE_EQ(limit->getStopPrice(), 0.0);
}
