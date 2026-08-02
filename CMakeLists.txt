cmake_minimum_required(VERSION 3.20)

project(low_latency_exchange_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(exchange_engine
    src/main.cpp
    src/order_book.cpp
    src/order_book_ladder_pool.cpp
)

target_include_directories(exchange_engine PRIVATE include)

target_compile_options(exchange_engine PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(exchange_benchmark
    benchmark/benchmark_main.cpp
    benchmark/benchmark_runner.cpp
    benchmark/workload_generator.cpp
    benchmark/allocation_counter.cpp

    src/order_book.cpp
    src/order_book_ladder_pool.cpp
)

target_include_directories(exchange_benchmark PRIVATE
    include
    benchmark
)

target_compile_features(exchange_benchmark PRIVATE cxx_std_20)

target_compile_options(exchange_benchmark PRIVATE
    -O3
    -DNDEBUG
)