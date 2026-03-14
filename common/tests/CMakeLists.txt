cmake_minimum_required(VERSION 3.16)

# Fetch Google Test
include(FetchContent)

# Some existing build folders can contain a partially populated googletest tree
# (missing googletest/CMakeLists.txt).
set(_gtest_src_dir "${CMAKE_BINARY_DIR}/_deps/googletest-src")
set(_gtest_expected_cmakelists "${_gtest_src_dir}/googletest/CMakeLists.txt")

if(FETCHCONTENT_FULLY_DISCONNECTED)
    if(NOT EXISTS "${_gtest_expected_cmakelists}")
        set(_gtest_alt_src_dir "${CMAKE_SOURCE_DIR}/build2/_deps/googletest-src")
        if(EXISTS "${_gtest_alt_src_dir}/googletest/CMakeLists.txt")
            message(STATUS "Using offline googletest source from ${_gtest_alt_src_dir}")
            set(FETCHCONTENT_SOURCE_DIR_GOOGLETEST "${_gtest_alt_src_dir}" CACHE PATH "" FORCE)
        else()
            message(FATAL_ERROR
                "FETCHCONTENT_FULLY_DISCONNECTED is ON but googletest sources are not populated. "
                "Configure once with network access/disconnected OFF, or provide a populated "
                "FETCHCONTENT_SOURCE_DIR_GOOGLETEST."
            )
        endif()
    endif()
elseif(EXISTS "${_gtest_src_dir}" AND NOT EXISTS "${_gtest_expected_cmakelists}")
    # In connected mode we can clear stale folders and let FetchContent re-download.
    message(STATUS "Detected stale googletest cache in ${_gtest_src_dir}; repopulating")
    file(REMOVE_RECURSE
        "${CMAKE_BINARY_DIR}/_deps/googletest-src"
        "${CMAKE_BINARY_DIR}/_deps/googletest-build"
        "${CMAKE_BINARY_DIR}/_deps/googletest-subbuild"
    )
endif()

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    SOURCE_SUBDIR googletest
)
# Required when using SOURCE_SUBDIR=googletest without the repository root
# CMakeLists that normally defines GOOGLETEST_VERSION.
if(NOT DEFINED GOOGLETEST_VERSION)
    set(GOOGLETEST_VERSION 1.14.0)
endif()
# Prevent GTest from overriding parent project settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Resolve the gtest_main target name provided by FetchContent/system package.
set(GTEST_MAIN_TARGET "")
if(TARGET GTest::gtest_main)
    set(GTEST_MAIN_TARGET GTest::gtest_main)
elseif(TARGET gtest_main)
    set(GTEST_MAIN_TARGET gtest_main)
else()
    find_package(GTest QUIET CONFIG)
    if(TARGET GTest::gtest_main)
        set(GTEST_MAIN_TARGET GTest::gtest_main)
    elseif(TARGET gtest_main)
        set(GTEST_MAIN_TARGET gtest_main)
    else()
        message(FATAL_ERROR "GoogleTest target not found. FetchContent and find_package(GTest) both failed.")
    endif()
endif()
# --- Orderbook matching tests (migrated from raw asserts) ---
add_executable(orderbook_matching_test
    orderbook_matching_test.cpp
)
target_link_libraries(orderbook_matching_test
    PRIVATE orderbook_common ${GTEST_MAIN_TARGET}
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
    PRIVATE orderbook_common ${GTEST_MAIN_TARGET}
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
    PRIVATE orderbook_common ${GTEST_MAIN_TARGET}
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
    PRIVATE orderbook_common ${GTEST_MAIN_TARGET}
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
    PRIVATE orderbook_common ${GTEST_MAIN_TARGET}
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
    PRIVATE orderbook_common ${GTEST_MAIN_TARGET}
)
target_include_directories(market_order_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
set_target_properties(market_order_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_test(NAME MarketOrderTest COMMAND market_order_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Stop order tests ---
add_executable(stop_order_test
    stop_order_test.cpp
)
target_link_libraries(stop_order_test
    PRIVATE orderbook_common ${GTEST_MAIN_TARGET}
)
target_include_directories(stop_order_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
set_target_properties(stop_order_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_test(NAME StopOrderTest COMMAND stop_order_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
