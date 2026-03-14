cmake_minimum_required(VERSION 3.16)

set(LIBRARY_NAME orderbook_common)

# Enable C language for SQLite amalgamation
enable_language(C)

# Enable testing support
option(BUILD_TESTS "Build tests for orderbook common library" ON)

# ── SQLite dependency ───────────────────────────────────────
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    message(STATUS "System SQLite3 not found — fetching amalgamation via FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        sqlite3_src
        URL https://www.sqlite.org/2024/sqlite-amalgamation-3450000.zip
        DOWNLOAD_EXTRACT_TIMESTAMP ON
        TLS_VERIFY FALSE
    )
    FetchContent_MakeAvailable(sqlite3_src)
    add_library(sqlite3 STATIC ${sqlite3_src_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3 PUBLIC
        $<BUILD_INTERFACE:${sqlite3_src_SOURCE_DIR}>
    )
    # Alias so consumers can link with the same name
    add_library(SQLite::SQLite3 ALIAS sqlite3)
else()
    message(STATUS "Found system SQLite3: ${SQLite3_VERSION}")
endif()

# Collect source files
set(SOURCES
    src/order.cpp
    src/orderbook.cpp
    src/message.cpp
    src/uuid.cpp
    src/order_store.cpp
)

set(HEADERS
    include/order.h
    include/orderbook.h
    include/message.h
    include/uuid.h
    include/order_store.h
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

find_package(Threads REQUIRED)
target_link_libraries(${LIBRARY_NAME} PRIVATE Threads::Threads)
target_link_libraries(${LIBRARY_NAME} PRIVATE SQLite::SQLite3)
# Expose SQLite headers for order_store.h (build only)
if(NOT SQLite3_FOUND)
    target_include_directories(${LIBRARY_NAME} PRIVATE ${sqlite3_src_SOURCE_DIR})
endif()

# Set up installation
if(NOT SQLite3_FOUND)
    install(TARGETS ${LIBRARY_NAME} sqlite3 EXPORT ${LIBRARY_NAME}Config
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
    )
else()
    install(TARGETS ${LIBRARY_NAME} EXPORT ${LIBRARY_NAME}Config
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
    )
endif()
install(DIRECTORY include/ DESTINATION include)
install(EXPORT ${LIBRARY_NAME}Config DESTINATION share/${LIBRARY_NAME}/cmake)

# Set up export
if(NOT SQLite3_FOUND)
    export(TARGETS ${LIBRARY_NAME} sqlite3 FILE ${LIBRARY_NAME}Config.cmake)
else()
    export(TARGETS ${LIBRARY_NAME} FILE ${LIBRARY_NAME}Config.cmake)
endif()

# Add tests if enabled
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()