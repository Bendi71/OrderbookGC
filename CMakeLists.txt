cmake_minimum_required(VERSION 3.16)

project(OrderbookSystem VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find external dependencies
find_package(asio QUIET)
if(NOT asio_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-21-0
    )
    FetchContent_MakeAvailable(asio)
    include_directories(${asio_SOURCE_DIR}/asio/include)
    add_compile_definitions(ASIO_STANDALONE)
endif()

# Always add common if it exists (required component)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/common)
    add_subdirectory(common)
    message(STATUS "Building common library")
else()
    message(WARNING "Common directory not found - skipping")
endif()

# Optionally add server if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/server)
    message(STATUS "Building server component")
    add_subdirectory(server)
    
    # Add a custom target to run the server after build
    add_custom_target(run_server
        COMMAND $<TARGET_FILE:orderbook_server>
        DEPENDS orderbook_server
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        COMMENT "Running orderbook server"
    )
else()
    message(STATUS "Server directory not found - skipping server build")
endif()

# Optionally add client if it exists and Qt6 is found
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/client)
    # Qt6 for the client application
    find_package(Qt6 COMPONENTS Widgets QUIET)
    if(Qt6_FOUND)
        message(STATUS "Qt6 found at: ${Qt6_DIR} - building client component")
        # Include directories for Qt
        include_directories(${Qt6Widgets_INCLUDE_DIRS})
        add_subdirectory(client)
        
        # Add a custom target to run the client after build
        add_custom_target(run_client
            COMMAND $<TARGET_FILE:orderbook_client>
            DEPENDS orderbook_client
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
            COMMENT "Running orderbook client"
        )
    else()
        message(STATUS "Qt6 not found, client application will not be built")
        add_subdirectory(client)
        
        # Add a custom target to run the client after build
        add_custom_target(run_client
            COMMAND $<TARGET_FILE:orderbook_client>
            DEPENDS orderbook_client
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
            COMMENT "Running orderbook client"
        )
    endif()
else()
    message(STATUS "Client directory not found - skipping client build")
endif()

# Create a custom target to build documentation (if Doxygen is installed)
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_SOURCE_DIR}/docs/Doxyfile.in)

    # Check if Doxyfile.in exists before configuring
    if(EXISTS ${DOXYGEN_IN})
        set(DOXYGEN_OUT ${CMAKE_BINARY_DIR}/Doxyfile)
        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    else()
        message(STATUS "Doxyfile.in not found, documentation will not be built")
    endif()
endif()