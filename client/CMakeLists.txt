cmake_minimum_required(VERSION 3.16)

set(TARGET_NAME orderbook_client)

# Find required Qt packages with better error messages
find_package(Qt6 COMPONENTS Widgets REQUIRED)
if(NOT Qt6_FOUND)
    message(FATAL_ERROR "Qt6 not found. Please set Qt6_DIR to your Qt6 installation's cmake directory.")
endif()

# Print Qt version information for debugging
message(STATUS "Found Qt6 version: ${Qt6_VERSION}")
message(STATUS "Using Qt6 from: ${Qt6_DIR}")
message(STATUS "Qt6Widgets includes: ${Qt6Widgets_INCLUDE_DIRS}")

# Set Qt specific CMake flags
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Collect source files
set(SOURCES
    src/client.cpp
    src/test_client.cpp
)

set(HEADERS
    include/client.h
)

# Create the executable
add_executable(${TARGET_NAME} ${SOURCES} ${HEADERS})

# Set include directories
target_include_directories(${TARGET_NAME}
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/common/include  # Add explicit path to common headers
)

# Link with the common library
target_link_libraries(${TARGET_NAME} PRIVATE orderbook_common)

# Link with Qt
target_link_libraries(${TARGET_NAME} PRIVATE Qt6::Widgets)

# Find and link with required libraries
find_package(Threads REQUIRED)
target_link_libraries(${TARGET_NAME} PRIVATE Threads::Threads)

# Add Windows socket libraries for ASIO
if(WIN32)
    target_link_libraries(${TARGET_NAME} PRIVATE ws2_32 mswsock)
endif()

# Add compile definitions
target_compile_definitions(${TARGET_NAME} PRIVATE ASIO_STANDALONE)
if(WIN32)
    target_compile_definitions(${TARGET_NAME} PRIVATE WIN32)
endif()

# Set up installation
install(TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION bin
)

# Copy required Qt DLLs for Windows
# if(WIN32)
#     # Safe way to find windeployqt
#     set(WINDEPLOYQT_EXECUTABLE "${Qt6_DIR}/../../../bin/windeployqt.exe")
#     if(NOT EXISTS ${WINDEPLOYQT_EXECUTABLE})
#         message(WARNING "windeployqt.exe not found at expected location: ${WINDEPLOYQT_EXECUTABLE}")
#         # Try to find it in the PATH
#         find_program(WINDEPLOYQT_EXECUTABLE windeployqt)
#     endif()

#     if(EXISTS ${WINDEPLOYQT_EXECUTABLE})
#         message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE}")
#         add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
#             COMMAND ${CMAKE_COMMAND} -E echo "Deploying Qt dependencies..."
#             COMMAND ${WINDEPLOYQT_EXECUTABLE} --no-compiler-runtime $<TARGET_FILE:${TARGET_NAME}>
#             COMMENT "Deploying Qt..."
#         )
#     else()
#         message(WARNING "windeployqt.exe not found, Qt dependencies will not be deployed")
#     endif()
# endif()
