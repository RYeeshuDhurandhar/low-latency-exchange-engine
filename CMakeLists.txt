cmake_minimum_required(VERSION 3.20)

project(low_latency_exchange_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(exchange_engine
    src/main.cpp
)

target_include_directories(exchange_engine PRIVATE include)
