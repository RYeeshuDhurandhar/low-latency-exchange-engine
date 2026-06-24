#pragma once

#include "types.hpp"

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
