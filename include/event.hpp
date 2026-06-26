#pragma once

#include <string>

#include "types.hpp"

struct Event {
    EventType event_type = EventType::Unknown;
    RequestType request_type = RequestType::Unknown;

    // Used for single-order events:
    // Accepted, Cancelled, Modified, Rejected, Rested/AddedToBook
    OrderId order_id = 0;

    // Used for trade events:
    // resting_order_id    = order already sitting in the book
    // aggressive_order_id = incoming order that caused the match
    OrderId resting_order_id = 0;
    OrderId aggressive_order_id = 0;
    
    OrderType order_type = OrderType::Unknown;

    SymbolId symbol_id = 0;
    Side side = Side::Unknown;
    Quantity quantity = 0;
    Price price = 0;
    
    ReasonCode reason_code = ReasonCode::None;
};
