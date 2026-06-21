#pragma once

#include "types.hpp"

// Used for incoming order request
struct OrderRequest {
    MessageType message_type;
    OrderType order_type;

    OrderId order_id;
    SymbolId symbol_id;
    Side side;

    Price price;
    Quantity quantity;
};
