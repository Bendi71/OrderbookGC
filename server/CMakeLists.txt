cmake_minimum_required(VERSION 3.16)

# Server component
set(SERVER_NAME orderbook_server)

# Collect source files
set(SERVER_SOURCES
    src/main.cpp
    src/server.cpp
    src/session.cpp
    src/order_generator.cpp)

set(SERVER_HEADERS
    include/server.h
    include/session.h
    include/order_generator.h)

# Create the executable
add_executable(${SERVER_NAME} ${SERVER_SOURCES} ${SERVER_HEADERS})

# Set include directories
target_include_directories(${SERVER_NAME} 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against required libraries
target_link_libraries(${SERVER_NAME} 
    PRIVATE 
        orderbook_common 
        ${CMAKE_THREAD_LIBS_INIT}
)

if(WIN32)
    target_link_libraries(orderbook_server PRIVATE ws2_32 mswsock)
endif()

target_compile_definitions(orderbook_server PRIVATE ASIO_STANDALONE)

# Set up installation
install(TARGETS ${SERVER_NAME} 
    RUNTIME DESTINATION bin)

# Set up batch files for running the server
file(WRITE ${CMAKE_BINARY_DIR}/bin/run_orderbook_server.bat 
     "cd /d %~dp0\r\norderbook_server.exe -s AAPL,MSFT,GOOG -g -i 1000")

# Make the batch file executable
file(CHMOD ${CMAKE_BINARY_DIR}/bin/run_orderbook_server.bat
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)