cmake_minimum_required(VERSION 3.17)
include(GoogleTest)
include(FetchContent)
project(orderbook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

FetchContent_MakeAvailable(googletest)

# Main matching engine binary. file-mode default; --udp enables the UDP path.
add_executable(order_matching
                src/main.cpp
                src/itch.cpp
                src/matching_engine.cpp
                src/engine_udp.cpp
                src/feed_handler.cpp
                src/orderbook.cpp
                )

target_include_directories(order_matching PRIVATE include)
target_compile_options(order_matching PRIVATE -O3 -g -Wall -Wextra -march=native)
target_link_libraries(order_matching PRIVATE Threads::Threads)

# Replay sender: pushes the ITCH file out over UDP as MoldUDP64 packets.
add_executable(replay
                src/replay.cpp
                )
target_include_directories(replay PRIVATE include)
target_compile_options(replay PRIVATE -O3 -g -Wall -Wextra -march=native)

enable_testing()
add_executable(run_tests
                tests/test.cpp
                src/itch.cpp
                src/matching_engine.cpp
                src/engine_udp.cpp
                src/feed_handler.cpp
                src/orderbook.cpp
                )

target_include_directories(run_tests PRIVATE include)
target_link_libraries(run_tests GTest::gtest_main Threads::Threads)
target_compile_options(run_tests PRIVATE -O3 -Wall -Wextra -march=native)

gtest_discover_tests(run_tests)
