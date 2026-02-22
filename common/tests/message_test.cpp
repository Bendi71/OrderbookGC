#include <gtest/gtest.h>
#include "message.h"
#include <cmath>

using namespace orderbook;

// ============================================================
// MessageFrame tests
// ============================================================

TEST(MessageFrame, FrameAndExtractLength) {
    std::string msg = "type=ORDER_SUBMIT\nclient_id=c1\n";
    auto framed = MessageFrame::frameMessage(msg);

    ASSERT_GE(framed.size(), MessageFrame::HEADER_SIZE);

    uint32_t len = MessageFrame::extractLength(framed.data());
    EXPECT_EQ(len, static_cast<uint32_t>(msg.size()));

    // Payload bytes match the original message
    std::string payload(framed.begin() + MessageFrame::HEADER_SIZE, framed.end());
    EXPECT_EQ(payload, msg);
}

TEST(MessageFrame, EmptyMessage) {
    auto framed = MessageFrame::frameMessage("");
    uint32_t len = MessageFrame::extractLength(framed.data());
    EXPECT_EQ(len, 0u);
}

// ============================================================
// OrderSubmitMessage round-trip
// ============================================================

TEST(MessageSerialization, OrderSubmitRoundTrip) {
    OrderSubmitMessage orig;
    orig.client_id = "test_client_42";
    orig.symbol = "AAPL";
    orig.side = OrderSide::BUY;
    orig.price = 155.50;
    orig.quantity = 100;

    std::string serialized = orig.serialize();
    auto parsed = parseMessage(serialized);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->getType(), MessageType::ORDER_SUBMIT);

    auto* msg = dynamic_cast<OrderSubmitMessage*>(parsed.get());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->client_id, "test_client_42");
    EXPECT_EQ(msg->symbol, "AAPL");
    EXPECT_EQ(msg->side, OrderSide::BUY);
    EXPECT_NEAR(msg->price, 155.50, 0.01);
    EXPECT_EQ(msg->quantity, 100u);
}

TEST(MessageSerialization, OrderSubmitSell) {
    OrderSubmitMessage orig;
    orig.client_id = "c2";
    orig.symbol = "MSFT";
    orig.side = OrderSide::SELL;
    orig.price = 300.25;
    orig.quantity = 50;

    auto parsed = parseMessage(orig.serialize());
    auto* msg = dynamic_cast<OrderSubmitMessage*>(parsed.get());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->side, OrderSide::SELL);
    EXPECT_EQ(msg->symbol, "MSFT");
}

// ============================================================
// OrderCancelMessage round-trip
// ============================================================

TEST(MessageSerialization, OrderCancelRoundTrip) {
    OrderCancelMessage orig("order-uuid-123", "client-abc");

    auto parsed = parseMessage(orig.serialize());
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->getType(), MessageType::ORDER_CANCEL);

    auto* msg = dynamic_cast<OrderCancelMessage*>(parsed.get());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->order_id, "order-uuid-123");
    EXPECT_EQ(msg->client_id, "client-abc");
}

// ============================================================
// SnapshotRequestMessage round-trip
// ============================================================

TEST(MessageSerialization, SnapshotRequestRoundTrip) {
    SnapshotRequestMessage orig("TSLA");

    auto parsed = parseMessage(orig.serialize());
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->getType(), MessageType::SNAPSHOT_REQUEST);

    auto* msg = dynamic_cast<SnapshotRequestMessage*>(parsed.get());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->symbol, "TSLA");
}

// ============================================================
// OrderStatusMessage round-trip
// ============================================================

TEST(MessageSerialization, OrderStatusRoundTrip) {
    OrderStatusMessage orig;
    orig.order_id = "uuid-789";
    orig.client_id = "client-xyz";
    orig.symbol = "GOOG";
    orig.side = OrderSide::SELL;
    orig.price = 2800.00;
    orig.quantity = 10;
    orig.filled_quantity = 3;
    orig.status = OrderStatus::PARTIAL;
    orig.order_timestamp = std::chrono::system_clock::now();

    auto parsed = parseMessage(orig.serialize());
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->getType(), MessageType::ORDER_STATUS);

    auto* msg = dynamic_cast<OrderStatusMessage*>(parsed.get());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->order_id, "uuid-789");
    EXPECT_EQ(msg->client_id, "client-xyz");
    EXPECT_EQ(msg->symbol, "GOOG");
    EXPECT_EQ(msg->side, OrderSide::SELL);
    EXPECT_NEAR(msg->price, 2800.00, 0.01);
    EXPECT_EQ(msg->quantity, 10u);
    EXPECT_EQ(msg->filled_quantity, 3u);
    EXPECT_EQ(msg->status, OrderStatus::PARTIAL);
}

TEST(MessageSerialization, OrderStatusAllStatuses) {
    for (auto status : {OrderStatus::PENDING, OrderStatus::PARTIAL,
                        OrderStatus::FILLED, OrderStatus::CANCELED,
                        OrderStatus::REJECTED}) {
        OrderStatusMessage orig;
        orig.order_id = "id";
        orig.client_id = "c";
        orig.symbol = "X";
        orig.side = OrderSide::BUY;
        orig.price = 1.0;
        orig.quantity = 1;
        orig.filled_quantity = 0;
        orig.status = status;
        orig.order_timestamp = std::chrono::system_clock::now();

        auto parsed = parseMessage(orig.serialize());
        auto* msg = dynamic_cast<OrderStatusMessage*>(parsed.get());
        ASSERT_NE(msg, nullptr);
        EXPECT_EQ(msg->status, status);
    }
}

// ============================================================
// OrderbookSnapshotMessage round-trip
// ============================================================

TEST(MessageSerialization, SnapshotRoundTrip) {
    OrderbookSnapshotMessage orig;
    orig.symbol = "AAPL";
    orig.bids = {{101.0, 50, 3}, {100.5, 20, 1}};
    orig.asks = {{101.5, 30, 2}, {102.0, 15, 1}};

    auto parsed = parseMessage(orig.serialize());
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->getType(), MessageType::ORDERBOOK_SNAPSHOT);

    auto* msg = dynamic_cast<OrderbookSnapshotMessage*>(parsed.get());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->symbol, "AAPL");

    ASSERT_EQ(msg->bids.size(), 2u);
    EXPECT_NEAR(msg->bids[0].price, 101.0, 0.01);
    EXPECT_EQ(msg->bids[0].quantity, 50u);
    EXPECT_EQ(msg->bids[0].order_count, 3);
    EXPECT_NEAR(msg->bids[1].price, 100.5, 0.01);

    ASSERT_EQ(msg->asks.size(), 2u);
    EXPECT_NEAR(msg->asks[0].price, 101.5, 0.01);
    EXPECT_EQ(msg->asks[0].quantity, 30u);
}

TEST(MessageSerialization, SnapshotEmptyBook) {
    OrderbookSnapshotMessage orig;
    orig.symbol = "EMPTY";

    auto parsed = parseMessage(orig.serialize());
    auto* msg = dynamic_cast<OrderbookSnapshotMessage*>(parsed.get());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->symbol, "EMPTY");
    EXPECT_TRUE(msg->bids.empty());
    EXPECT_TRUE(msg->asks.empty());
}

// ============================================================
// TradeNotificationMessage round-trip
// ============================================================

TEST(MessageSerialization, TradeNotificationRoundTrip) {
    TradeNotificationMessage orig;
    orig.buy_order_id = "buy-uuid-1";
    orig.sell_order_id = "sell-uuid-2";
    orig.symbol = "TSLA";
    orig.price = 250.75;
    orig.quantity = 42;
    orig.trade_timestamp = std::chrono::system_clock::now();

    auto parsed = parseMessage(orig.serialize());
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->getType(), MessageType::TRADE_NOTIFICATION);

    auto* msg = dynamic_cast<TradeNotificationMessage*>(parsed.get());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->buy_order_id, "buy-uuid-1");
    EXPECT_EQ(msg->sell_order_id, "sell-uuid-2");
    EXPECT_EQ(msg->symbol, "TSLA");
    EXPECT_NEAR(msg->price, 250.75, 0.01);
    EXPECT_EQ(msg->quantity, 42u);
}

// ============================================================
// ErrorMessage round-trip
// ============================================================

TEST(MessageSerialization, ErrorMessageRoundTrip) {
    ErrorMessage orig("INVALID_SYMBOL", "Symbol XYZ not found");

    auto parsed = parseMessage(orig.serialize());
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->getType(), MessageType::ERROR_MSG);

    auto* msg = dynamic_cast<ErrorMessage*>(parsed.get());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->error_code, "INVALID_SYMBOL");
    EXPECT_EQ(msg->description, "Symbol XYZ not found");
}

// ============================================================
// Parse edge cases
// ============================================================

TEST(MessageParse, EmptyString) {
    auto parsed = parseMessage("");
    ASSERT_NE(parsed, nullptr);
    // Empty message should produce an error
    EXPECT_EQ(parsed->getType(), MessageType::ERROR_MSG);
}

TEST(MessageParse, UnknownMessageType) {
    auto parsed = parseMessage("type=UNKNOWN_TYPE\nfoo=bar\n");
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->getType(), MessageType::ERROR_MSG);
}

TEST(MessageParse, MissingTypeValue) {
    auto parsed = parseMessage("not_a_type_line\n");
    ASSERT_NE(parsed, nullptr);
    // Without a valid type= prefix, extractString returns "" and the parser
    // falls through to the error branch
    EXPECT_EQ(parsed->getType(), MessageType::ERROR_MSG);
}

// ============================================================
// Time conversion
// ============================================================

TEST(TimeConversion, IsoRoundTrip) {
    auto now = std::chrono::system_clock::now();
    // Truncate to milliseconds for comparison
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    auto truncated = std::chrono::system_clock::time_point(ms);

    std::string iso = timePointToIsoString(truncated);
    auto parsed = isoStringToTimePoint(iso);

    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        parsed - truncated).count();
    EXPECT_LE(std::abs(diff), 1);  // Within 1ms tolerance
}

// ============================================================
// MessageType conversion
// ============================================================

TEST(MessageTypeConversion, StringRoundTrip) {
    std::vector<MessageType> types = {
        MessageType::ORDER_SUBMIT,
        MessageType::ORDER_CANCEL,
        MessageType::ORDER_STATUS,
        MessageType::ORDERBOOK_SNAPSHOT,
        MessageType::TRADE_NOTIFICATION,
        MessageType::ERROR_MSG,
        MessageType::SNAPSHOT_REQUEST
    };

    for (auto type : types) {
        std::string str = messageTypeToString(type);
        MessageType parsed = messageTypeFromString(str);
        EXPECT_EQ(parsed, type) << "Failed for type: " << str;
    }
}

TEST(MessageTypeConversion, UnknownStringThrows) {
    EXPECT_THROW(messageTypeFromString("INVALID"), std::runtime_error);
}
