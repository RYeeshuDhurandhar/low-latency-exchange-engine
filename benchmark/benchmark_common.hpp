#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <variant>

#include "benchmark_event_sink.hpp"
#include "benchmark_types.hpp"
#include "order.hpp"

inline const char* benchmark_layer_to_string(BenchmarkLayer layer) {
    switch (layer) {
        case BenchmarkLayer::Book:
            return "book";

        case BenchmarkLayer::Engine:
            return "engine";
    }

    return "unknown";
}

inline BenchmarkLayer parse_benchmark_layer(const std::string& value) {
    if (value == "book") {
        return BenchmarkLayer::Book;
    }

    if (value == "engine") {
        return BenchmarkLayer::Engine;
    }

    throw std::invalid_argument(
        "unknown benchmark layer: " + value
    );
}

class IBenchmarkBook {
public:
    virtual ~IBenchmarkBook() = default;

    virtual void reset() = 0;
    virtual void process(const BenchRequest& req, BenchmarkEventSink& sink) = 0;
    virtual const char* name() const = 0;
};

// Convert benchmark request to real order request
inline OrderRequest to_order_request(const BenchRequest& req) {
    switch (req.bench_op_type) {
        case BenchOpType::NewLimit:
        case BenchOpType::NewMarket: {
            NewOrderRequest new_req;

            new_req.order_id = req.order_id;
            new_req.order_type = (req.bench_op_type == BenchOpType::NewMarket) ? OrderType::Market : OrderType::Limit;
            new_req.price = req.price;
            new_req.quantity = req.quantity;
            new_req.side = req.side;
            new_req.symbol_id = req.symbol_id;

            return OrderRequest{std::in_place_type<NewOrderRequest>, new_req};
        }

        case BenchOpType::Modify: {
            ModifyOrderRequest modify_req;

            modify_req.order_id = req.order_id;
            modify_req.order_type = req.order_type;
            modify_req.price = req.price;
            modify_req.quantity = req.quantity;

            return OrderRequest{std::in_place_type<ModifyOrderRequest>, modify_req};
        }

        case BenchOpType::Cancel: {
            CancelOrderRequest cancel_req;

            cancel_req.order_id = req.order_id;

            return OrderRequest{std::in_place_type<CancelOrderRequest>, cancel_req};
        }

        case BenchOpType::Unknown: {
            return OrderRequest{std::in_place_type<CancelOrderRequest>, CancelOrderRequest{}};
        }
    }

    return OrderRequest{std::in_place_type<CancelOrderRequest>, CancelOrderRequest{}};
}
