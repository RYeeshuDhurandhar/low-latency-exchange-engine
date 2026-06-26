#pragma once

#include "types.hpp"
#include <variant>

// Used for orders in orderbook
struct Order {
    OrderId order_id = 0;
    SymbolId symbol_id = 0;
    Side side = Side::Unknown;
    Price price = 0;
    Quantity original_quantity = 0;
    Quantity remaining_quantity = 0;
    SequenceNumber sequence_number = 0;
    OrderStatus order_status = OrderStatus::Unknown;
};

// Used for incoming order request
struct NewOrderRequest {
    OrderType order_type = OrderType::Unknown;

    OrderId order_id = 0;
    SymbolId symbol_id = 0;
    Side side = Side::Unknown;

    Price price = 0;
    Quantity quantity = 0;
};

struct ModifyOrderRequest {
    OrderType order_type = OrderType::Unknown;
    OrderId order_id = 0;

    Price price = 0;
    Quantity quantity = 0;
};


struct CancelOrderRequest {
    OrderId order_id = 0;
};

// Common wrapper type for mixed request streams/logs/replay
using OrderRequest = std::variant<
    NewOrderRequest,
    ModifyOrderRequest,
    CancelOrderRequest
>;
