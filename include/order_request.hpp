#pragma once

#include "types.hpp"

// Used for incoming order request
struct OrderRequest {
    MessageType message_type = MessageType::Unknown;
    OrderType order_type = OrderType::Unknown;

    OrderId order_id = 0;
    SymbolId symbol_id = 0;
    Side side = Side::Unknown;

    Price price = 0;
    Quantity quantity = 0;
};
