#pragma once

#include "types.hpp"

// Used for orders in orderbook
struct Order {
    OrderId order_id;
    SymbolId symbol_id;
    Side side;
    Price price;
    Quantity original_quantity;
    Quantity remaining_quantity;
    SequenceNumber sequence_number;
    OrderStatus order_status;
};
