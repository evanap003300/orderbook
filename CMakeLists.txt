cmake_minimum_required(VERSION 3.17)

project(orderbook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(order_matching 
                src/main.cpp 
                src/itch.cpp 
                src/matching_engine.cpp
                src/orderbook.cpp
                )
target_include_directories(order_matching PRIVATE include)
target_compile_options(order_matching PRIVATE -O3 -Wall -Wextra -march=native)