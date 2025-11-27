cmake_minimum_required(VERSION 3.12)

project(tsn_udp_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable extra warnin
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()


# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Define the executable
add_executable(tsn_udp_demo
    src/main.cpp
)

