/**
 * Market order tests — verifies that market orders match at the best
 * available price, sweep multiple levels, and never rest in the book.
 */

#include <gtest/gtest.h>
#include "orderbook.h"
#include "message.h"
#include <vector>
#include <memory>

using namespace orderbook;

namespace {

static int g_mkt_counter = 0;

std::string mktId() {
    return "mkt_test_" + std::to_string(++g_mkt_counter);
}

OrderPtr limitBuy(double price, uint32_t qty) {
    return std::make_shared<Order>(mktId(), OrderSide::BUY, price, qty, "TEST", "client1");
}

OrderPtr limitSell(double price, uint32_t qty) {
    return std::make_shared<Order>(mktId(), OrderSide::SELL, price, qty, "TEST", "client1");
}

OrderPtr marketBuy(uint32_t qty) {
    return std::make_shared<Order>(mktId(), OrderSide::BUY, 0.0, qty, "TEST", "client1", OrderType::MARKET);
}

OrderPtr marketSell(uint32_t qty) {
    return std::make_shared<Order>(mktId(), OrderSide::SELL, 0.0, qty, "TEST", "client1", OrderType::MARKET);
}

} // namespace

// Market buy fills at best ask price
TEST(MarketOrder, BuyFillsAtBestAsk) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    book.addOrder(limitSell(100.0, 10));
    book.addOrder(limitSell(101.0, 20));

    auto mkt = marketBuy(5);
    book.addOrder(mkt);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_DOUBLE_EQ(trades[0].price, 100.0);
    EXPECT_EQ(trades[0].quantity, 5u);
    EXPECT_EQ(mkt->getStatus(), OrderStatus::FILLED);
}

// Market sell fills at best bid price
TEST(MarketOrder, SellFillsAtBestBid) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    book.addOrder(limitBuy(100.0, 10));
    book.addOrder(limitBuy(99.0, 20));

    auto mkt = marketSell(5);
    book.addOrder(mkt);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_DOUBLE_EQ(trades[0].price, 100.0);
    EXPECT_EQ(trades[0].quantity, 5u);
    EXPECT_EQ(mkt->getStatus(), OrderStatus::FILLED);
}

// Market buy sweeps multiple ask levels
TEST(MarketOrder, BuySweepsMultipleLevels) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    book.addOrder(limitSell(100.0, 5));
    book.addOrder(limitSell(101.0, 5));
    book.addOrder(limitSell(102.0, 5));

    auto mkt = marketBuy(12);
    book.addOrder(mkt);

    ASSERT_EQ(trades.size(), 3u);
    EXPECT_DOUBLE_EQ(trades[0].price, 100.0);
    EXPECT_EQ(trades[0].quantity, 5u);
    EXPECT_DOUBLE_EQ(trades[1].price, 101.0);
    EXPECT_EQ(trades[1].quantity, 5u);
    EXPECT_DOUBLE_EQ(trades[2].price, 102.0);
    EXPECT_EQ(trades[2].quantity, 2u);

    EXPECT_EQ(mkt->getStatus(), OrderStatus::FILLED);
}

// Market sell sweeps multiple bid levels
TEST(MarketOrder, SellSweepsMultipleLevels) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    book.addOrder(limitBuy(102.0, 5));
    book.addOrder(limitBuy(101.0, 5));
    book.addOrder(limitBuy(100.0, 5));

    auto mkt = marketSell(12);
    book.addOrder(mkt);

    ASSERT_EQ(trades.size(), 3u);
    // Should match highest bid first
    EXPECT_DOUBLE_EQ(trades[0].price, 102.0);
    EXPECT_EQ(trades[0].quantity, 5u);
    EXPECT_DOUBLE_EQ(trades[1].price, 101.0);
    EXPECT_EQ(trades[1].quantity, 5u);
    EXPECT_DOUBLE_EQ(trades[2].price, 100.0);
    EXPECT_EQ(trades[2].quantity, 2u);

    EXPECT_EQ(mkt->getStatus(), OrderStatus::FILLED);
}

// Market order on empty book: unfilled remainder is canceled, not resting
TEST(MarketOrder, EmptyBookCancelsRemainder) {
    OrderBook book("TEST");

    auto mkt = marketBuy(10);
    book.addOrder(mkt);

    // Order should be canceled (not resting)
    EXPECT_EQ(mkt->getStatus(), OrderStatus::CANCELED);
    EXPECT_EQ(mkt->getRemainingQuantity(), 10u);

    // Book should be empty
    EXPECT_TRUE(book.getBidLevels().empty());
    EXPECT_TRUE(book.getAskLevels().empty());
}

// Partial fill + cancel remainder when liquidity is insufficient
TEST(MarketOrder, PartialFillCancelsRemainder) {
    OrderBook book("TEST");
    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) { trades.push_back(t); });

    book.addOrder(limitSell(100.0, 3));

    auto mkt = marketBuy(10);
    book.addOrder(mkt);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 3u);
    // Remainder is canceled, not left in the book
    EXPECT_EQ(mkt->getStatus(), OrderStatus::CANCELED);
    EXPECT_EQ(mkt->getRemainingQuantity(), 7u);
    EXPECT_TRUE(book.getBidLevels().empty());
}

// Market order does not rest in the book
TEST(MarketOrder, DoesNotRestInBook) {
    OrderBook book("TEST");

    auto mkt_buy = marketBuy(10);
    book.addOrder(mkt_buy);

    auto mkt_sell = marketSell(10);
    book.addOrder(mkt_sell);

    EXPECT_TRUE(book.getBidLevels().empty());
    EXPECT_TRUE(book.getAskLevels().empty());
}

// Market order preserves order_type through creation
TEST(MarketOrder, OrderTypeIsPreserved) {
    auto limit = limitBuy(100.0, 10);
    EXPECT_EQ(limit->getOrderType(), OrderType::LIMIT);

    auto mkt = marketBuy(10);
    EXPECT_EQ(mkt->getOrderType(), OrderType::MARKET);
}

// OrderSubmitMessage round-trip with order_type
TEST(MarketOrder, MessageSerializationRoundTrip) {
    OrderSubmitMessage msg;
    msg.client_id = "test_client";
    msg.symbol = "AAPL";
    msg.side = OrderSide::BUY;
    msg.order_type = OrderType::MARKET;
    msg.price = 0.0;
    msg.quantity = 50;

    std::string serialized = msg.serialize();
    EXPECT_NE(serialized.find("order_type=MARKET"), std::string::npos);

    auto parsed = parseMessage(serialized);
    ASSERT_NE(parsed, nullptr);
    auto submit = std::dynamic_pointer_cast<OrderSubmitMessage>(parsed);
    ASSERT_NE(submit, nullptr);
    EXPECT_EQ(submit->order_type, OrderType::MARKET);
    EXPECT_EQ(submit->side, OrderSide::BUY);
    EXPECT_EQ(submit->quantity, 50u);
}

// OrderStatusMessage round-trip with order_type
TEST(MarketOrder, StatusMessageSerializationRoundTrip) {
    OrderStatusMessage msg;
    msg.order_id = "test-uuid-1234";
    msg.client_id = "test_client";
    msg.symbol = "AAPL";
    msg.side = OrderSide::SELL;
    msg.order_type = OrderType::MARKET;
    msg.price = 0.0;
    msg.quantity = 30;
    msg.filled_quantity = 30;
    msg.status = OrderStatus::FILLED;
    msg.order_timestamp = std::chrono::system_clock::now();

    std::string serialized = msg.serialize();
    EXPECT_NE(serialized.find("order_type=MARKET"), std::string::npos);

    auto parsed = parseMessage(serialized);
    ASSERT_NE(parsed, nullptr);
    auto status = std::dynamic_pointer_cast<OrderStatusMessage>(parsed);
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->order_type, OrderType::MARKET);
    EXPECT_EQ(status->status, OrderStatus::FILLED);
}

// Default order_type is LIMIT when field is missing
TEST(MarketOrder, DefaultOrderTypeIsLimit) {
    std::string raw = "type=ORDER_SUBMIT\nclient_id=c1\nsymbol=TEST\nside=BUY\nprice=100.00\nquantity=10";
    auto parsed = parseMessage(raw);
    auto submit = std::dynamic_pointer_cast<OrderSubmitMessage>(parsed);
    ASSERT_NE(submit, nullptr);
    EXPECT_EQ(submit->order_type, OrderType::LIMIT);
}
