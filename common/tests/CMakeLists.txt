cmake_minimum_required(VERSION 3.16)

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# Prevent GTest from overriding parent project settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- Orderbook matching tests (migrated from raw asserts) ---
add_executable(orderbook_matching_test
    orderbook_matching_test.cpp
)
target_link_libraries(orderbook_matching_test
    PRIVATE orderbook_common GTest::gtest_main
)
target_include_directories(orderbook_matching_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
set_target_properties(orderbook_matching_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_test(NAME OrderbookMatchingTest COMMAND orderbook_matching_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Cancel tests ---
add_executable(orderbook_cancel_test
    orderbook_cancel_test.cpp
)
target_link_libraries(orderbook_cancel_test
    PRIVATE orderbook_common GTest::gtest_main
)
target_include_directories(orderbook_cancel_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
set_target_properties(orderbook_cancel_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_test(NAME OrderbookCancelTest COMMAND orderbook_cancel_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Edge case tests ---
add_executable(orderbook_edge_test
    orderbook_edge_test.cpp
)
target_link_libraries(orderbook_edge_test
    PRIVATE orderbook_common GTest::gtest_main
)
target_include_directories(orderbook_edge_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
set_target_properties(orderbook_edge_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_test(NAME OrderbookEdgeTest COMMAND orderbook_edge_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Message serialization tests ---
add_executable(message_test
    message_test.cpp
)
target_link_libraries(message_test
    PRIVATE orderbook_common GTest::gtest_main
)
target_include_directories(message_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
set_target_properties(message_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_test(NAME MessageTest COMMAND message_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Order unit tests ---
add_executable(order_test
    order_test.cpp
)
target_link_libraries(order_test
    PRIVATE orderbook_common GTest::gtest_main
)
target_include_directories(order_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
set_target_properties(order_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_test(NAME OrderTest COMMAND order_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Market order tests ---
add_executable(market_order_test
    market_order_test.cpp
)
target_link_libraries(market_order_test
    PRIVATE orderbook_common GTest::gtest_main
)
target_include_directories(market_order_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
set_target_properties(market_order_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_test(NAME MarketOrderTest COMMAND market_order_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
