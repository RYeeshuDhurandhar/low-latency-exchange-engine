#pragma once

#include <cstdint>
#include <string>

#include "types.hpp"

enum class BenchOpType : uint8_t {
    Unknown,
    NewLimit,
    NewMarket,
    Cancel,
    Modify,
};

struct BenchRequest {
    BenchOpType bench_op_type = BenchOpType::Unknown;

    OrderId order_id = 0;
    SymbolId symbol_id = 0;

    Side side = Side::Unknown;
    OrderType order_type = OrderType::Unknown;

    Price price = 0;
    Quantity quantity = 0;
};

struct WorkloadConfig {
    std::string name;

    std::uint64_t num_ops = 1'000'000;
    std::int64_t seed = 1;

    SymbolId symbol_id = 1;

    Price max_price = 11000;
    Price mid_price = 10000;
    Price min_price = 90000;
    Price tick_size = 1;
    
    Quantity max_quantity = 100;
    Quantity min_quantity = 1;
};

struct BenchmarkResult {
    std::string impl_name;
    std::string workload_name;

    std::uint64_t operations = 0;
    std::uint64_t total_ns = 0;

    double throughput_ops_per_sec = 0.0;

    std::int64_t p50_ns = 0;
    std::int64_t p90_ns = 0;
    std::int64_t p99_ns = 0;
    std::int64_t max_ns = 0;

    std::int64_t alloc_count = 0;
    std::int64_t alloc_bytes = 0;

    std::int64_t trades = 0;
    std::int64_t accepted = 0;
    std::int64_t rejected = 0;
    std::int64_t cancelled = 0;
    std::int64_t modified = 0;
    std::int64_t rested = 0;
};
