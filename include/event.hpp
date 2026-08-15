#pragma once

#include "types.hpp"

struct Event {
    EventType event_type = EventType::Unknown;
    RequestType request_type = RequestType::Unknown;

    // Input request sequence number assigned by MatchingEngine.
    // All output events caused by the same input request get the same value.
    SequenceNumber input_sequence_number = 0;

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
    
    Reason reason = Reason::None;

    // Only used by EventType::BookUpdated
    bool has_best_bid = false;
    Price best_bid = 0;
    Quantity best_bid_quantity = 0;

    bool has_best_ask = false;
    Price best_ask = 0;
    Quantity best_ask_quantity = 0;
};
