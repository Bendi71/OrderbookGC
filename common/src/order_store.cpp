#include "order_store.h"
#include "password_hash.h"
#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace orderbook {

// ── helpers ─────────────────────────────────────────────────

static std::string sideToString(OrderSide s) {
    return s == OrderSide::BUY ? "BUY" : "SELL";
}

static OrderSide stringToSide(const std::string& s) {
    return s == "BUY" ? OrderSide::BUY : OrderSide::SELL;
}

static std::string typeToString(OrderType t) {
    switch (t) {
        case OrderType::MARKET:     return "MARKET";
        case OrderType::STOP:       return "STOP";
        case OrderType::STOP_LIMIT: return "STOP_LIMIT";
        default:                    return "LIMIT";
    }
}

static OrderType stringToType(const std::string& s) {
    if (s == "MARKET")     return OrderType::MARKET;
    if (s == "STOP")       return OrderType::STOP;
    if (s == "STOP_LIMIT") return OrderType::STOP_LIMIT;
    return OrderType::LIMIT;
}

static std::string statusToString(OrderStatus st) {
    switch (st) {
        case OrderStatus::PENDING:  return "PENDING";
        case OrderStatus::PARTIAL:  return "PARTIAL";
        case OrderStatus::FILLED:   return "FILLED";
        case OrderStatus::CANCELED: return "CANCELED";
        case OrderStatus::REJECTED: return "REJECTED";
        default:                    return "PENDING";
    }
}

static OrderStatus stringToStatus(const std::string& s) {
    if (s == "PARTIAL")  return OrderStatus::PARTIAL;
    if (s == "FILLED")   return OrderStatus::FILLED;
    if (s == "CANCELED") return OrderStatus::CANCELED;
    if (s == "REJECTED") return OrderStatus::REJECTED;
    return OrderStatus::PENDING;
}

static std::string nowIso() {
    auto tp = std::chrono::system_clock::now();
    auto t  = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%FT%T") << "Z";
    return ss.str();
}

// ── lifecycle ───────────────────────────────────────────────

OrderStore::OrderStore(const std::string& db_path)
    : db_path_(db_path) {}

OrderStore::~OrderStore() {
    close();
}

bool OrderStore::open() {
    if (db_) return true;
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "OrderStore: cannot open DB " << db_path_
                  << ": " << sqlite3_errmsg(db_) << std::endl;
        db_ = nullptr;
        return false;
    }
    // Enable WAL mode for better concurrent performance
    executeSQL("PRAGMA journal_mode=WAL;");
    executeSQL("PRAGMA synchronous=NORMAL;");
    return createSchema();
}

void OrderStore::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool OrderStore::executeSQL(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "OrderStore SQL error: " << (err ? err : "unknown") << std::endl;
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool OrderStore::createSchema() {
    const char* schema = R"SQL(
        CREATE TABLE IF NOT EXISTS orders (
            id                TEXT PRIMARY KEY,
            client_id         TEXT NOT NULL,
            symbol            TEXT NOT NULL,
            side              TEXT NOT NULL,
            order_type        TEXT NOT NULL DEFAULT 'LIMIT',
            price             REAL NOT NULL,
            stop_price        REAL NOT NULL DEFAULT 0.0,
            quantity          INTEGER NOT NULL,
            remaining_quantity INTEGER NOT NULL,
            status            TEXT NOT NULL DEFAULT 'PENDING',
            created_at        TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_orders_symbol_status
            ON orders(symbol, status);

        CREATE TABLE IF NOT EXISTS trades (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            buy_order_id    TEXT NOT NULL,
            sell_order_id   TEXT NOT NULL,
            symbol          TEXT NOT NULL,
            price           REAL NOT NULL,
            quantity         INTEGER NOT NULL,
            traded_at       TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_trades_symbol
            ON trades(symbol, traded_at DESC);

        CREATE TABLE IF NOT EXISTS users (
            username    TEXT PRIMARY KEY,
            password    TEXT NOT NULL,
            permissions TEXT NOT NULL DEFAULT 'trade,view'
        );
    )SQL";
    return executeSQL(schema);
}

// ── Order persistence ───────────────────────────────────────

bool OrderStore::saveOrder(const OrderPtr& order) {
    if (!db_ || !order) return false;

    const char* sql =
        "INSERT OR REPLACE INTO orders "
        "(id, client_id, symbol, side, order_type, price, stop_price, "
        " quantity, remaining_quantity, status, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, order->getId().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, order->getClientId().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, order->getSymbol().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, sideToString(order->getSide()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, typeToString(order->getOrderType()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 6, order->getPrice());
    sqlite3_bind_double(stmt, 7, order->getStopPrice());
    sqlite3_bind_int(stmt, 8, order->getQuantity());
    sqlite3_bind_int(stmt, 9, order->getRemainingQuantity());
    sqlite3_bind_text(stmt, 10, statusToString(order->getStatus()).c_str(), -1, SQLITE_TRANSIENT);
    std::string ts = nowIso();
    sqlite3_bind_text(stmt, 11, ts.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool OrderStore::updateOrder(const OrderPtr& order) {
    if (!db_ || !order) return false;

    const char* sql =
        "UPDATE orders SET remaining_quantity=?, status=?, order_type=? WHERE id=?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, order->getRemainingQuantity());
    sqlite3_bind_text(stmt, 2, statusToString(order->getStatus()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, typeToString(order->getOrderType()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, order->getId().c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::vector<OrderPtr> OrderStore::loadActiveOrders(const std::string& symbol) {
    std::vector<OrderPtr> result;
    if (!db_) return result;

    const char* sql =
        "SELECT id, client_id, symbol, side, order_type, price, stop_price, "
        "       quantity, remaining_quantity, status "
        "FROM orders WHERE symbol=? AND status IN ('PENDING','PARTIAL') "
        "ORDER BY created_at ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    sqlite3_bind_text(stmt, 1, symbol.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string id      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string cid     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string sym     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        OrderSide side      = stringToSide(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
        OrderType otype     = stringToType(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
        double price        = sqlite3_column_double(stmt, 5);
        double stop_price   = sqlite3_column_double(stmt, 6);
        uint32_t qty        = static_cast<uint32_t>(sqlite3_column_int(stmt, 7));
        uint32_t rem_qty    = static_cast<uint32_t>(sqlite3_column_int(stmt, 8));
        // Build order with remaining qty - create with remaining qty as the quantity
        auto order = std::make_shared<Order>(id, side, price, rem_qty, sym, cid, otype, stop_price);
        // Mark as partial if qty differs
        if (rem_qty < qty) {
            order->fill(0); // trigger PARTIAL status via a no-op fill
        }
        result.push_back(order);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<OrderPtr> OrderStore::loadAllActiveOrders() {
    std::vector<OrderPtr> result;
    if (!db_) return result;

    const char* sql =
        "SELECT id, client_id, symbol, side, order_type, price, stop_price, "
        "       quantity, remaining_quantity, status "
        "FROM orders WHERE status IN ('PENDING','PARTIAL') "
        "ORDER BY created_at ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string id      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string cid     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string sym     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        OrderSide side      = stringToSide(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
        OrderType otype     = stringToType(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
        double price        = sqlite3_column_double(stmt, 5);
        double stop_price   = sqlite3_column_double(stmt, 6);
        uint32_t qty        = static_cast<uint32_t>(sqlite3_column_int(stmt, 7));
        uint32_t rem_qty    = static_cast<uint32_t>(sqlite3_column_int(stmt, 8));

        auto order = std::make_shared<Order>(id, side, price, rem_qty, sym, cid, otype, stop_price);
        if (rem_qty < qty) {
            order->fill(0);
        }
        result.push_back(order);
    }

    sqlite3_finalize(stmt);
    return result;
}

// ── Trade persistence ───────────────────────────────────────

bool OrderStore::saveTrade(const TradeRecord& trade) {
    if (!db_) return false;

    const char* sql =
        "INSERT INTO trades (buy_order_id, sell_order_id, symbol, price, quantity, traded_at) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, trade.buy_order_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, trade.sell_order_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, trade.symbol.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, trade.price);
    sqlite3_bind_int(stmt, 5, trade.quantity);
    std::string ts = trade.timestamp.empty() ? nowIso() : trade.timestamp;
    sqlite3_bind_text(stmt, 6, ts.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::vector<OrderStore::TradeRecord> OrderStore::loadRecentTrades(
        const std::string& symbol, int limit) {
    std::vector<TradeRecord> result;
    if (!db_) return result;

    const char* sql =
        "SELECT buy_order_id, sell_order_id, symbol, price, quantity, traded_at "
        "FROM trades WHERE symbol=? ORDER BY traded_at DESC LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    sqlite3_bind_text(stmt, 1, symbol.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TradeRecord tr;
        tr.buy_order_id  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        tr.sell_order_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        tr.symbol        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        tr.price         = sqlite3_column_double(stmt, 3);
        tr.quantity       = static_cast<uint32_t>(sqlite3_column_int(stmt, 4));
        tr.timestamp     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        result.push_back(tr);
    }

    sqlite3_finalize(stmt);
    return result;
}

// ── User management ─────────────────────────────────────────

bool OrderStore::saveUser(const UserRecord& user) {
    if (!db_) return false;

    const char* sql =
        "INSERT OR REPLACE INTO users (username, password, permissions) VALUES (?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    // Hash the password before storing
    std::string hashed = user.password.empty()
        ? std::string()  // Guest accounts have no password
        : password_hash::hashPassword(user.password);

    sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hashed.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, user.permissions.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool OrderStore::getUser(const std::string& username, UserRecord& out) {
    if (!db_) return false;

    const char* sql = "SELECT username, password, permissions FROM users WHERE username=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out.username    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        out.password    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        out.permissions = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        found = true;
    }

    sqlite3_finalize(stmt);
    return found;
}

bool OrderStore::authenticateUser(const std::string& username, const std::string& password) {
    UserRecord user;
    if (!getUser(username, user)) return false;
    // Verify password against stored hash (supports sha256:salt:hash and legacy plaintext)
    if (user.password.empty()) {
        // Guest account: only allow empty password
        return password.empty();
    }
    return password_hash::verifyPassword(password, user.password);
}

void OrderStore::seedDefaultUsers() {
    if (!db_) return;

    // Check if users table is empty
    const char* sql = "SELECT COUNT(*) FROM users;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (count == 0) {
        // Seed default users
        saveUser({"admin", "admin", "trade,view,admin"});
        saveUser({"trader", "trader", "trade,view"});
        saveUser({"viewer", "viewer", "view"});
        saveUser({"guest", "", "trade,view"});  // No password for guest
        std::cout << "OrderStore: seeded default users (admin/trader/viewer/guest)" << std::endl;
    }
}

} // namespace orderbook
