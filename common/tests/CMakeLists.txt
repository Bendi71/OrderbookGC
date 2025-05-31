cmake_minimum_required(VERSION 3.16)

# Add the executable
add_executable(orderbook_matching_test 
    orderbook_matching_test.cpp
)

# Link against the orderbook library
target_link_libraries(orderbook_matching_test
    PRIVATE
    orderbook_common
)

# Include directories
target_include_directories(orderbook_matching_test
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Set output directory
set_target_properties(orderbook_matching_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Add test
add_test(
    NAME OrderbookMatchingTest
    COMMAND orderbook_matching_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
