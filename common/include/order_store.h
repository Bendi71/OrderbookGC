#pragma once

#include "order.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

// Forward-declare sqlite3 to avoid leaking the header
struct sqlite3;

namespace orderbook {

/// Lightweight SQLite-backed persistence for orders, trades and users.
/// Designed as a foundation to evolve into a richer database layer later.
class OrderStore {
public:
    explicit OrderStore(const std::string& db_path = "orderbook.db");
    ~OrderStore();

    // Non-copyable
    OrderStore(const OrderStore&) = delete;
    OrderStore& operator=(const OrderStore&) = delete;

    /// Open (or create) the database and ensure schema exists.
    bool open();

    /// Close the database.
    void close();

    // ── Order persistence ──────────────────────────────────

    /// Persist a new order (INSERT).
    bool saveOrder(const OrderPtr& order);

    /// Update an existing order's status / remaining qty.
    bool updateOrder(const OrderPtr& order);

    /// Load all active orders (PENDING / PARTIAL) for a given symbol.
    std::vector<OrderPtr> loadActiveOrders(const std::string& symbol);

    /// Load all active orders across all symbols.
    std::vector<OrderPtr> loadAllActiveOrders();

    // ── Trade persistence ──────────────────────────────────

    struct TradeRecord {
        std::string buy_order_id;
        std::string sell_order_id;
        std::string symbol;
        double price;
        uint32_t quantity;
        std::string timestamp;
    };

    /// Persist a trade.
    bool saveTrade(const TradeRecord& trade);

    /// Load recent trades for a symbol (newest first).
    std::vector<TradeRecord> loadRecentTrades(const std::string& symbol, int limit = 100);

    // ── User management (simple auth) ──────────────────────

    struct UserRecord {
        std::string username;
        std::string password;        // plain text for now — hash later
        std::string permissions;     // comma-separated: "trade,view,admin"
    };

    /// Add or update a user.
    bool saveUser(const UserRecord& user);

    /// Look up a user by username.  Returns true if found.
    bool getUser(const std::string& username, UserRecord& out);

    /// Validate credentials.  Returns true on match.
    bool authenticateUser(const std::string& username, const std::string& password);

    /// Seed default users if the table is empty.
    void seedDefaultUsers();

private:
    bool executeSQL(const std::string& sql);
    bool createSchema();

    std::string db_path_;
    sqlite3* db_ = nullptr;
};

} // namespace orderbook
