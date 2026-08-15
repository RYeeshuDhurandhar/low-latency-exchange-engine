#pragma once

#include "benchmark_common.hpp"
#include "order_book_adapter.hpp"
#include "matching_engine_adapter.hpp"

inline std::unique_ptr<IBenchmarkBook> make_benchmark_target(
    BenchmarkLayer layer,
    const std::string& book_type,
    Price min_price,
    Price max_price,
    Price tick_size,
    std::size_t expected_orders
) {
    if (layer == BenchmarkLayer::Book) {
        if (book_type == "map") {
            return std::make_unique<MapOrderBookAdapter>(
                expected_orders
            );
        }

        if (book_type == "ladder_pool") {
            return std::make_unique<LadderPoolOrderBookAdapter>(
                min_price,
                max_price,
                tick_size,
                expected_orders
            );
        }
    }

    if (layer == BenchmarkLayer::Engine) {
        if (book_type == "map") {
            return std::make_unique<MapMatchingEngineAdapter>(
                expected_orders
            );
        }

        if (book_type == "ladder_pool") {
            return std::make_unique<LadderPoolMatchingEngineAdapter>(
                min_price,
                max_price,
                tick_size,
                expected_orders
            );
        }
    }

    throw std::invalid_argument(
        "unsupported layer/implementation combination"
    );
}
