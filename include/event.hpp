#pragma once

#include <string>

#include "types.hpp"

struct Event {
    EventType type = EventType::Unknown;

    // Used for single-order events:
    // Accepted, Cancelled, Modified, Rejected, Rested/AddedToBook
    OrderId order_id = 0;

    // Used for trade events:
    // resting_order_id    = order already sitting in the book
    // aggressive_order_id = incoming order that caused the match
    OrderId resting_order_id = 0;
    OrderId aggressive_order_id = 0;

    SymbolId symbol_id = 0;
    Price price = 0;
    Quantity quantity = 0;
    
    ReasonCode reason_code = ReasonCode::None;
};
