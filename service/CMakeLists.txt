cmake_minimum_required(VERSION 3.10)
project(pc_service)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Try to find nlohmann_json, otherwise assume it's in include/
find_package(nlohmann_json QUIET)

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/pc_service.cpp
    src/db.cpp
    src/utils.cpp
    src/nft_manager.cpp
    src/adguard_manager.cpp
    src/entertainment_manager.cpp
    src/device_manager.cpp
    src/license_manager.cpp
    src/log_monitor.cpp
    src/scheduler.cpp
)

# Executable
add_executable(pc_service ${SOURCES})

# Link libraries
target_link_libraries(pc_service
    ${CURL_LIBRARIES}
    SQLite::SQLite3
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    dl
)

if(nlohmann_json_FOUND)
    target_link_libraries(pc_service nlohmann_json::nlohmann_json)
endif()
