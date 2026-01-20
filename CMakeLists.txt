cmake_minimum_required(VERSION 3.13)
project(redis_clone_starter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# Include root src directory
include_directories(${CMAKE_SOURCE_DIR}/src)

# -------------------------------------------------------------------
# Source file groups
# -------------------------------------------------------------------

file(GLOB_RECURSE CORE_SRC
    src/core/*.cpp
)

file(GLOB_RECURSE SERVER_SRC
    src/server/tcp_server.cpp
)

file(GLOB_RECURSE CLIENT_SRC
    src/client/*.cpp
)

file(GLOB_RECURSE UTIL_SRC
    src/util/*.cpp
)

file(GLOB_RECURSE TEST_SRC
    src/test/*.cpp
)

# -------------------------------------------------------------------
# Core library (shared logic)
# -------------------------------------------------------------------

add_library(redis_core
    ${CORE_SRC}
    ${SERVER_SRC}
    ${UTIL_SRC}
)

target_include_directories(redis_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(redis_core PUBLIC Threads::Threads)

# -------------------------------------------------------------------
# Redis server executable
# -------------------------------------------------------------------

add_executable(redis_server
    src/server/main.cpp
)

target_link_libraries(redis_server PRIVATE redis_core)

# -------------------------------------------------------------------
# Redis client executable
# -------------------------------------------------------------------

add_executable(redis_client
    ${CLIENT_SRC}
)

target_link_libraries(redis_client PRIVATE redis_core)

# -------------------------------------------------------------------
# Test executable (NO server main)
# -------------------------------------------------------------------

# add_executable(redis_tests
#     ${TEST_SRC}
# )

# target_link_libraries(redis_tests PRIVATE redis_core)

