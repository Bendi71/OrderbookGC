cmake_minimum_required(VERSION 3.16)

set(LIBRARY_NAME orderbook_common)

# Collect source files
set(SOURCES
    src/order.cpp
    src/orderbook.cpp
    src/message.cpp
)

set(HEADERS
    include/order.h
    include/orderbook.h
    include/message.h
)

# Create the library
add_library(${LIBRARY_NAME} ${SOURCES} ${HEADERS})

# Set include directories
target_include_directories(${LIBRARY_NAME}
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Add compile definitions
target_compile_definitions(${LIBRARY_NAME} PRIVATE ASIO_STANDALONE)

# Add networking libraries if needed
find_package(Threads REQUIRED)
target_link_libraries(${LIBRARY_NAME} PRIVATE Threads::Threads)

# Set up installation
install(TARGETS ${LIBRARY_NAME} EXPORT ${LIBRARY_NAME}Config
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT ${LIBRARY_NAME}Config DESTINATION share/${LIBRARY_NAME}/cmake)

# Set up export
export(TARGETS ${LIBRARY_NAME} FILE ${LIBRARY_NAME}Config.cmake)