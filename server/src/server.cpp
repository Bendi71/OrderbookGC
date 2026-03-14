#include "server.h"
#include "uuid.h"
#include <iostream>

namespace orderbook {
Server::Server(asio::io_context& io_context, unsigned short port)
    : io_context_(io_context)
    , acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    , snapshot_timer_(io_context)
{
}

void Server::start() {
    std::cout << "Server starting on port " << acceptor_.local_endpoint().port() << std::endl;
    
    // Initialize persistence if not already done
    if (!order_store_) {
        initPersistence();
    }
    
    // Start accepting connections
    acceptConnection();
    
    // Start the snapshot timer
    startSnapshotTimer(snapshot_interval_ms_);

    if (order_generator_) {
        order_generator_->start();
    }
}

void Server::stop() {
    // Stop accepting connections
    acceptor_.close();
    
    // Close all sessions
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& pair : sessions_) {
            pair.second->close();
        }
        sessions_.clear();
    }
    
    // Cancel the snapshot timer
    snapshot_timer_.cancel();

    if (order_generator_) {
        order_generator_->stop();
    }
}

bool Server::initPersistence() {
    order_store_ = std::make_unique<OrderStore>(db_path_);
    if (!order_store_->open()) {
        std::cerr << "Failed to open order store at: " << db_path_ << std::endl;
        order_store_.reset();
        return false;
    }
    
    // Seed default users for auth
    order_store_->seedDefaultUsers();
    
    std::cout << "Persistence initialized: " << db_path_ << std::endl;
    
    // Restore active orders from DB into orderbooks
    auto active_orders = order_store_->loadAllActiveOrders();
    if (!active_orders.empty()) {
        std::cout << "Restoring " << active_orders.size() << " active orders from DB..." << std::endl;
        for (const auto& order : active_orders) {
            auto orderbook = getOrderbook(order->getSymbol());
            if (orderbook) {
                orderbook->addOrder(order);
            }
        }
        std::cout << "Order restoration complete." << std::endl;
    }
    
    return true;
}

void Server::configureOrderGenerator(const OrderGenerator::Config& config) {
    if (order_generator_) {
        // Stop the generator first if it's running
        order_generator_->stop();
        // Apply the new configuration
        order_generator_->setConfig(config);
    } else {
        // Create a new generator with the specified config
        order_generator_ = std::make_shared<OrderGenerator>(config);
    }

    // Set up the order callback — post addOrder to io_context so all matching
    // and callbacks run on the ASIO thread (thread-safe for session writes).
    order_generator_->setOrderCallback([this](const OrderPtr& order) {
        asio::post(io_context_, [this, order]() {
            auto orderbook = getOrderbook(order->getSymbol());
            if (orderbook) {
                orderbook->addOrder(order);
                order_count_.fetch_add(1, std::memory_order_relaxed);
            }
        });
    });

    // Cancel callback — orderbook is internally mutex-protected and generator
    // orders have no session owner, so onOrderUpdated() returns early.
    auto gen_symbol = config.symbol;
    order_generator_->setCancelCallback([this, gen_symbol](const std::string& order_id) -> bool {
        auto orderbook = getOrderbook(gen_symbol);
        if (orderbook) {
            return orderbook->cancelOrder(order_id);
        }
        return false;
    });

    // The generator will be started in the start() method
}

bool Server::createOrderbook(const std::string& symbol) {
    // Default tick size if not specified
    return createOrderbook(symbol, 0.1);
}

bool Server::createOrderbook(const std::string& symbol, double tick_size) {
    std::lock_guard<std::mutex> lock(orderbooks_mutex_);
    
    // Check if the orderbook already exists
    if (orderbooks_.find(symbol) != orderbooks_.end()) {
        return false;
    }
    
    auto orderbook = std::make_shared<OrderBook>(symbol, tick_size);
    
    // Set up callbacks for order updates and trade notifications
    orderbook->setOrderCallback([this](const OrderPtr& order) {
        onOrderUpdated(order);
    });
    
    orderbook->setTradeCallback([this](const Trade& trade) {
        onTradeExecuted(trade);
    });
    
    // Add the orderbook to the map
    orderbooks_[symbol] = orderbook;
    
    std::cout << "Created orderbook for symbol: " << symbol 
    << " with tick size: " << tick_size << std::endl;
    
    return true;
}

std::shared_ptr<OrderBook> Server::getOrderbook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(orderbooks_mutex_);
    
    auto it = orderbooks_.find(symbol);
    if (it != orderbooks_.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::vector<std::string> Server::getOrderbookSymbols() const {
    std::lock_guard<std::mutex> lock(orderbooks_mutex_);
    
    std::vector<std::string> symbols;
    for (const auto& pair : orderbooks_) {
        symbols.push_back(pair.first); // increasing vector size
    }
    
    return symbols;
}

void Server::startSnapshotTimer(int interval_ms) {
    snapshot_interval_ms_ = interval_ms;
    
    // Set up the timer to periodically send orderbook snapshots
    snapshot_timer_.expires_after(std::chrono::milliseconds(interval_ms));
    snapshot_timer_.async_wait([this](std::error_code ec) {
        if (!ec) {
            // Send orderbook snapshots to clients
            sendOrderbookSnapshots();
            
            // Restart the timer
            startSnapshotTimer(snapshot_interval_ms_);
        } else if (ec != asio::error::operation_aborted) {
            std::cerr << "Snapshot timer error: " << ec.message() << std::endl;
        }
    });
}

bool Server::setOrderbookTickSize(const std::string& symbol, double tick_size) {
    if (tick_size <= 0.0) {
        std::cerr << "Invalid tick size: " << tick_size << " for symbol: " << symbol << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(orderbooks_mutex_);
    
    auto it = orderbooks_.find(symbol);
    if (it == orderbooks_.end()) {
        std::cerr << "Orderbook not found for symbol: " << symbol << std::endl;
        return false;
    }
    
    it->second->setTickSize(tick_size);
    std::cout << "Updated tick size to " << tick_size << " for symbol: " << symbol << std::endl;
    return true;
}

void Server::acceptConnection() {
    // Check if acceptor is still open before starting new accept
    if (!acceptor_.is_open()) {
        return;
    }
    
    acceptor_.async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                std::string endpoint_str;
                try {
                    auto endpoint = socket.remote_endpoint();
                    endpoint_str = endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
                    std::cout << "New connection from: " << endpoint_str << std::endl;
                } catch (const std::exception& e) {
                    endpoint_str = "[unknown endpoint]";
                    std::cout << "New connection from unknown endpoint: " << e.what() << std::endl;
                }

                try {
                    // Set TCP_NODELAY option to disable Nagle's algorithm
                    asio::ip::tcp::no_delay nodelay_option(true);
                    socket.set_option(nodelay_option);

                    // Set keepalive to detect dead connections
                    asio::socket_base::keep_alive keepalive_option(true);
                    socket.set_option(keepalive_option);
                    
                    std::cout << "SERVER DEBUG: Configured socket options for " << endpoint_str << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "SERVER WARNING: Failed to set some socket options: " << e.what() << std::endl;
                }
                
                // Create a new session
                auto session = std::make_shared<Session>(
                    std::move(socket),
                    [this](const MessagePtr& message, SessionPtr session) {
                        handleMessage(message, session);
                    }
                );
                
                // Generate a unique client ID
                std::string client_id = orderbook::generateUuid();
                session->setClientId(client_id);
                
                // Set disconnect callback so the session is cleaned up on close
                session->setDisconnectCallback([this](SessionPtr s) {
                    std::cout << "Session disconnected: " << s->getClientId() << std::endl;
                    removeSession(s);
                });
                
                // Add the session to the map of active sessions
                {
                    std::lock_guard<std::mutex> lock(sessions_mutex_);
                    sessions_[client_id] = session;
                }
                
                // Start the session
                session->start();
            } else {
                // Only log error if it's not due to shutdown
                if (ec != asio::error::operation_aborted) {
                    std::cerr << "SERVER ERROR: Failed to accept connection: " << ec.message() << std::endl;
                }
            }
            
            // Accept the next connection only if acceptor is still open
            if (acceptor_.is_open()) {
                acceptConnection();
            }
        });
}

void Server::handleMessage(const MessagePtr& message, SessionPtr session) {
    if (!message) {
        return;
    }
    
    // Login messages are always allowed (even before auth)
    if (auto login = std::dynamic_pointer_cast<LoginMessage>(message)) {
        handleLogin(login, session);
        return;
    }
    
    // Register messages are always allowed (even before auth)
    if (auto reg = std::dynamic_pointer_cast<RegisterMessage>(message)) {
        handleRegister(reg, session);
        return;
    }
    
    // Snapshot requests only need "view" permission
    if (auto snapshot_request = std::dynamic_pointer_cast<SnapshotRequestMessage>(message)) {
        if (!requireAuth(session, "view")) return;
        handleSnapshotRequest(snapshot_request, session);
        return;
    }
    
    // Handle different message types (require "trade" permission)
    if (auto order_submit = std::dynamic_pointer_cast<OrderSubmitMessage>(message)) {
        if (!requireAuth(session, "trade")) return;
        handleOrderSubmit(order_submit, session);
    }
    else if (auto order_cancel = std::dynamic_pointer_cast<OrderCancelMessage>(message)) {
        if (!requireAuth(session, "trade")) return;
        handleOrderCancel(order_cancel, session);
    }
    else if (auto order_status = std::dynamic_pointer_cast<OrderStatusMessage>(message)) {
        if (!requireAuth(session, "view")) return;
        handleOrderStatus(order_status, session);
    }
    else {
        // Unknown message type
        std::cerr << "Unknown message type received" << std::endl;
        session->sendMessage(std::make_shared<ErrorMessage>("UNKNOWN_MESSAGE", "Unknown message type"));
    }
}

void Server::handleOrderSubmit(const std::shared_ptr<OrderSubmitMessage>& message, SessionPtr session) {
    // Get the orderbook for the symbol
    auto orderbook = getOrderbook(message->symbol);
    if (!orderbook) {
        std::cerr << "Orderbook not found for symbol: " << message->symbol << std::endl;
        session->sendMessage(std::make_shared<ErrorMessage>(
            "INVALID_SYMBOL", 
            "Orderbook not found for symbol: " + message->symbol));
        return;
    }

    double original_price = message->price;
    if (!orderbook->isValidPrice(message->price)) {
        message->price = orderbook->roundToTickSize(message->price);
    }
    
    // Subscribe the client to the orderbook updates if not already subscribed
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        subscriptions_[session->getClientId()].insert(message->symbol);
    }
    
    // Market orders use price 0 as a sentinel — the matching engine ignores it
    double order_price = (message->order_type == OrderType::MARKET || 
                          message->order_type == OrderType::STOP) ? 0.0 : message->price;

    // Create a new order
    auto order = std::make_shared<Order>(
        orderbook::generateUuid(),
        message->side,
        order_price,
        message->quantity,
        message->symbol,
        message->client_id,
        message->order_type,
        message->stop_price
    );
    
    // Persist the order
    if (order_store_) {
        order_store_->saveOrder(order);
    }
    
    // Add the order to the orderbook
    orderbook->addOrder(order);
    
    // Remember the owner of this order
    {
        std::lock_guard<std::mutex> lock(order_owners_mutex_);
        order_owners_[order->getId()] = session->getClientId();
    }
    
    // Send an order status message to the client
    auto status_message = std::make_shared<OrderStatusMessage>();
    status_message->order_id = order->getId();
    status_message->client_id = order->getClientId();
    status_message->symbol = order->getSymbol();
    status_message->side = order->getSide();
    status_message->order_type = order->getOrderType();
    status_message->price = order->getPrice();
    status_message->stop_price = order->getStopPrice();
    status_message->quantity = order->getQuantity();
    status_message->filled_quantity = order->getQuantity() - order->getRemainingQuantity();
    status_message->status = order->getStatus();
    status_message->order_timestamp = order->getTimestamp();
    
    session->sendMessage(status_message);
}

void Server::handleOrderCancel(const std::shared_ptr<OrderCancelMessage>& message, SessionPtr session) {
    // Check if the client owns the order — compare against the session's
    // server-assigned client ID, not the message's client_id field, because
    // ownership is recorded with session->getClientId() in handleOrderSubmit.
    {
        std::lock_guard<std::mutex> lock(order_owners_mutex_);
        auto it = order_owners_.find(message->order_id);
        if (it == order_owners_.end() || it->second != session->getClientId()) {
            std::cerr << "Client " << session->getClientId() << " (msg: " << message->client_id
                      << ") tried to cancel order " << message->order_id
                      << " which they don't own" << std::endl;
            session->sendMessage(std::make_shared<ErrorMessage>(
                "INVALID_ORDER", 
                "Cannot cancel order: not found or not owned by this client"));
            return;
        }
    }
    
    // Find the orderbook for this order
    bool order_cancelled = false;
    
    {
        std::lock_guard<std::mutex> lock(orderbooks_mutex_);
        for (auto& pair : orderbooks_) {
            auto& orderbook = pair.second;
            if (orderbook->cancelOrder(message->order_id)) {
                order_cancelled = true;
                break;
            }
        }
    }
    
    if (!order_cancelled) {
        std::cerr << "Order not found for cancellation: " << message->order_id << std::endl;
        session->sendMessage(std::make_shared<ErrorMessage>(
            "INVALID_ORDER", 
            "Order not found for cancellation"));
    }
}

void Server::handleSnapshotRequest(const std::shared_ptr<SnapshotRequestMessage>& message, SessionPtr session) {
    std::cout << "Handling snapshot request for symbol: " << message->symbol << std::endl;
    
    // Add the client to the subscription list for this symbol
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        subscriptions_[session->getClientId()].insert(message->symbol);
    }
    
    // Get the orderbook for this symbol
    auto orderbook = getOrderbook(message->symbol);
    
    // Create the orderbook if it doesn't exist
    if (!orderbook) {
        std::cout << "Creating new orderbook for symbol: " << message->symbol << std::endl;
        if (!createOrderbook(message->symbol)) {
            std::cerr << "Failed to create orderbook for symbol: " << message->symbol << std::endl;
            session->sendMessage(std::make_shared<ErrorMessage>(
                "ORDERBOOK_CREATION_FAILED", 
                "Failed to create orderbook for symbol: " + message->symbol));
            return;
        }
        orderbook = getOrderbook(message->symbol);
    }
    
    // Create and send a snapshot message immediately
    auto snapshot_message = std::make_shared<OrderbookSnapshotMessage>();
    snapshot_message->symbol = message->symbol;
    
    // Get the top N levels of bids and asks
    constexpr int max_levels = 10;
    
    // Add bid levels
    auto bids = orderbook->getBidLevels(max_levels);
    for (const auto& level : bids) {
        snapshot_message->bids.push_back(level);
    }
    
    // Add ask levels
    auto asks = orderbook->getAskLevels(max_levels);
    for (const auto& level : asks) {
        snapshot_message->asks.push_back(level);
    }
    
    std::cout << "Sending orderbook snapshot for symbol: " << message->symbol 
              << " (bids: " << snapshot_message->bids.size() 
              << ", asks: " << snapshot_message->asks.size() << ")" << std::endl;
              
    // Send the snapshot
    session->sendMessage(snapshot_message);
}

void Server::handleOrderStatus(const std::shared_ptr<OrderStatusMessage>& message, SessionPtr session) {
    // Check if the client owns the order — compare against the session's
    // server-assigned client ID, not the message's client_id field.
    bool is_owner = false;
    {
        std::lock_guard<std::mutex> lock(order_owners_mutex_);
        auto it = order_owners_.find(message->order_id);
        if (it != order_owners_.end() && it->second == session->getClientId()) {
            is_owner = true;
        }
    }
    
    if (!is_owner) {
        std::cerr << "Client " << session->getClientId() << " (msg: " << message->client_id
                  << ") requested status for order " 
                  << message->order_id << " which they don't own" << std::endl;
        session->sendMessage(std::make_shared<ErrorMessage>(
            "INVALID_ORDER", 
            "Cannot get order status: not found or not owned by this client"));
        return;
    }
    
    // Find the order in the orderbooks
    OrderPtr found_order;
    std::string found_symbol;
    
    {
        std::lock_guard<std::mutex> lock(orderbooks_mutex_);
        for (const auto& pair : orderbooks_) {
            auto& symbol = pair.first;
            auto& orderbook = pair.second;
            
            OrderPtr order = orderbook->getOrder(message->order_id);
            if (order) {
                found_order = order;
                found_symbol = symbol;
                break;
            }
        }
    }
    
    if (!found_order) {
        // Order might have been completely filled or cancelled
        std::cerr << "Order not found for status request: " << message->order_id << std::endl;
        session->sendMessage(std::make_shared<ErrorMessage>(
            "INVALID_ORDER", 
            "Order not found for status request"));
        return;
    }
    
    // Send back the order status
    auto status_message = std::make_shared<OrderStatusMessage>();
    status_message->order_id = found_order->getId();
    status_message->client_id = found_order->getClientId();
    status_message->symbol = found_order->getSymbol();
    status_message->side = found_order->getSide();
    status_message->order_type = found_order->getOrderType();
    status_message->price = found_order->getPrice();
    status_message->stop_price = found_order->getStopPrice();
    status_message->quantity = found_order->getQuantity();
    status_message->filled_quantity = found_order->getQuantity() - found_order->getRemainingQuantity();
    status_message->status = found_order->getStatus();
    status_message->order_timestamp = found_order->getTimestamp();
    
    session->sendMessage(status_message);
}

void Server::handleLogin(const std::shared_ptr<LoginMessage>& message, SessionPtr session) {
    auto response = std::make_shared<LoginResponseMessage>();
    response->username = message->username;
    
    if (!order_store_) {
        // No persistence — allow all logins
        session->setAuthenticated(true);
        session->setUsername(message->username);
        session->setSessionToken(orderbook::generateUuid());
        session->setPermissions("trade,view");
        
        response->success = true;
        response->session_token = session->getSessionToken();
    } else if (message->username == "guest" || 
               order_store_->authenticateUser(message->username, message->password)) {
        OrderStore::UserRecord user;
        if (order_store_->getUser(message->username, user)) {
            session->setPermissions(user.permissions);
        } else {
            session->setPermissions("trade,view");
        }
        
        session->setAuthenticated(true);
        session->setUsername(message->username);
        session->setSessionToken(orderbook::generateUuid());
        
        response->success = true;
        response->session_token = session->getSessionToken();
        std::cout << "User '" << message->username << "' authenticated (session " 
                  << session->getClientId() << ")" << std::endl;
    } else {
        response->success = false;
        response->error_message = "Invalid credentials";
        std::cerr << "Authentication failed for user '" << message->username 
                  << "' (session " << session->getClientId() << ")" << std::endl;
    }
    
    session->sendMessage(response);
}

void Server::handleRegister(const std::shared_ptr<RegisterMessage>& message, SessionPtr session) {
    auto response = std::make_shared<RegisterResponseMessage>();
    response->username = message->username;
    
    // Validate input
    if (message->username.empty()) {
        response->success = false;
        response->error_message = "Username cannot be empty";
        session->sendMessage(response);
        return;
    }
    if (message->password.empty()) {
        response->success = false;
        response->error_message = "Password cannot be empty";
        session->sendMessage(response);
        return;
    }
    if (message->username.size() > 64 || message->password.size() > 128) {
        response->success = false;
        response->error_message = "Username or password too long";
        session->sendMessage(response);
        return;
    }
    
    if (!order_store_) {
        response->success = false;
        response->error_message = "Registration unavailable (no persistence)";
        session->sendMessage(response);
        return;
    }
    
    // Check if user already exists
    OrderStore::UserRecord existing;
    if (order_store_->getUser(message->username, existing)) {
        response->success = false;
        response->error_message = "Username already taken";
        session->sendMessage(response);
        return;
    }
    
    // Create user with default permissions
    OrderStore::UserRecord newUser;
    newUser.username = message->username;
    newUser.password = message->password;  // OrderStore will hash it
    newUser.permissions = "trade,view";
    
    if (order_store_->saveUser(newUser)) {
        response->success = true;
        std::cout << "User '" << message->username << "' registered successfully (session "
                  << session->getClientId() << ")" << std::endl;
    } else {
        response->success = false;
        response->error_message = "Failed to create account";
        std::cerr << "Registration failed for user '" << message->username
                  << "' (session " << session->getClientId() << ")" << std::endl;
    }
    
    session->sendMessage(response);
}

bool Server::requireAuth(SessionPtr session, const std::string& permission) {
    // If auth is not required, auto-authenticate with full permissions
    if (!auth_required_) {
        if (!session->isAuthenticated()) {
            session->setAuthenticated(true);
            session->setPermissions("trade,view,admin");
            session->setUsername("auto");
        }
        return true;
    }
    
    if (!session->isAuthenticated()) {
        session->sendMessage(std::make_shared<ErrorMessage>(
            "AUTH_REQUIRED", "Login required before this operation"));
        return false;
    }
    
    if (!session->hasPermission(permission)) {
        session->sendMessage(std::make_shared<ErrorMessage>(
            "PERMISSION_DENIED", "Missing permission: " + permission));
        return false;
    }
    
    return true;
}

void Server::onOrderUpdated(const OrderPtr& order) {
    if (!order) {
        return;
    }
    
    // Persist order update
    if (order_store_) {
        order_store_->updateOrder(order);
    }
    
    // Find the client that owns this order
    std::string client_id;
    {
        std::lock_guard<std::mutex> lock(order_owners_mutex_);
        auto it = order_owners_.find(order->getId());
        if (it != order_owners_.end()) {
            client_id = it->second;
            // Clean up filled/cancelled orders from ownership map
            if (order->getStatus() == OrderStatus::FILLED ||
                order->getStatus() == OrderStatus::CANCELED) {
                order_owners_.erase(it);
            }
        }
    }
    
    if (client_id.empty()) {
        return;
    }
    
    // Find the session for this client (O(1) lookup)
    SessionPtr client_session;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(client_id);
        if (it != sessions_.end()) {
            client_session = it->second;
        }
    }
    
    if (!client_session) {
        return;
    }
    
    // Send an order status update to the client
    auto status_message = std::make_shared<OrderStatusMessage>();
    status_message->order_id = order->getId();
    status_message->client_id = order->getClientId();
    status_message->symbol = order->getSymbol();
    status_message->side = order->getSide();
    status_message->order_type = order->getOrderType();
    status_message->price = order->getPrice();
    status_message->stop_price = order->getStopPrice();
    status_message->quantity = order->getQuantity();
    status_message->filled_quantity = order->getQuantity() - order->getRemainingQuantity();
    status_message->status = order->getStatus();
    status_message->order_timestamp = order->getTimestamp();
    
    client_session->sendMessage(status_message);
}

void Server::onTradeExecuted(const Trade& trade) {
    trade_count_.fetch_add(1, std::memory_order_relaxed);

    if (order_generator_) {
        order_generator_->updateLastPrice(trade.price);
    }

    // Persist the trade
    if (order_store_) {
        OrderStore::TradeRecord tr;
        tr.buy_order_id = trade.buy_order_id;
        tr.sell_order_id = trade.sell_order_id;
        tr.symbol = trade.symbol;
        tr.price = trade.price;
        tr.quantity = trade.quantity;
        order_store_->saveTrade(tr);
    }

    // Build trade notification once, broadcast to all subscribers of this symbol.
    // This is standard market-data behaviour: all participants see all trades.
    auto trade_message = std::make_shared<TradeNotificationMessage>();
    trade_message->buy_order_id = trade.buy_order_id;
    trade_message->sell_order_id = trade.sell_order_id;
    trade_message->symbol = trade.symbol;
    trade_message->price = trade.price;
    trade_message->quantity = trade.quantity;
    trade_message->trade_timestamp = trade.timestamp;

    // Collect sessions subscribed to this symbol
    std::vector<SessionPtr> recipients;
    {
        std::lock_guard<std::mutex> sub_lock(subscriptions_mutex_);
        std::lock_guard<std::mutex> ses_lock(sessions_mutex_);
        for (const auto& [client_id, syms] : subscriptions_) {
            if (syms.count(trade.symbol)) {
                auto it = sessions_.find(client_id);
                if (it != sessions_.end()) {
                    recipients.push_back(it->second);
                }
            }
        }
    }

    for (const auto& session : recipients) {
        session->sendMessage(trade_message);
    }
}

void Server::sendOrderbookSnapshots() {
    // Step 1: Collect snapshot data while holding only orderbooks_mutex_
    // This avoids holding orderbooks_mutex_ while acquiring sessions_mutex_ (lock ordering fix)
    struct SnapshotData {
        std::string symbol;
        std::shared_ptr<OrderbookSnapshotMessage> message;
    };
    std::vector<SnapshotData> snapshots;
    
    {
        std::lock_guard<std::mutex> orderbooks_lock(orderbooks_mutex_);
        for (const auto& pair : orderbooks_) {
            const std::string& symbol = pair.first;
            const auto& orderbook = pair.second;
            
            auto snapshot_message = std::make_shared<OrderbookSnapshotMessage>();
            snapshot_message->symbol = symbol;
            
            constexpr int max_levels = 10;
            snapshot_message->bids = orderbook->getBidLevels(max_levels);
            snapshot_message->asks = orderbook->getAskLevels(max_levels);
            
            snapshots.push_back({symbol, std::move(snapshot_message)});
        }
    }
    
    // Step 2: Distribute snapshots to subscribed clients
    std::lock_guard<std::mutex> subscriptions_lock(subscriptions_mutex_);
    std::lock_guard<std::mutex> sessions_lock(sessions_mutex_);
    
    for (const auto& snap : snapshots) {
        for (const auto& sub_pair : subscriptions_) {
            const std::string& client_id = sub_pair.first;
            const std::set<std::string>& subscribed_symbols = sub_pair.second;
            
            if (subscribed_symbols.count(snap.symbol)) {
                auto it = sessions_.find(client_id);
                if (it != sessions_.end()) {
                    it->second->sendMessage(snap.message);
                }
            }
        }
    }
}

void Server::removeSession(const SessionPtr& session) {
    std::string client_id = session->getClientId();
    
    // Remove from sessions
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(client_id);
    }
    
    // Remove subscriptions
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        subscriptions_.erase(client_id);
    }
    
    // Remove order ownership entries for this client
    {
        std::lock_guard<std::mutex> lock(order_owners_mutex_);
        for (auto it = order_owners_.begin(); it != order_owners_.end(); ) {
            if (it->second == client_id) {
                it = order_owners_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

}