#pragma once

#include <cstdint>

using OrderId = uint64_t;
using SymbolId = uint32_t;
using Price = uint64_t;
using Quantity = uint32_t;
using SequenceNumber = uint64_t;

enum class Side: uint8_t {
    Unknown = 0,
    Buy,
    Sell
};

enum class OrderType: uint8_t {
    Unknown = 0,
    Limit,
    Market
};

enum class MessageType: uint8_t {
    Unknown = 0,
    New,
    Cancel,
    Modify
};

enum class OrderStatus: uint8_t {
    Unknown = 0,
    New,
    PartiallyFilled,
    Resting,
    Filled,
    Cancelled,
    Rejected
};

enum class EventType: uint8_t {
    Unknown = 0,                    // default event type for debugging
    OrderAccepted,                  // new order accepted by engine
    OrderRejected,                  // invalid request / duplicate / unknown cancel
    OrderCancelled,                 // order removed from book
    OrderModified,                  // modify request accepted
    OrderRested,                    // remaining limit order inserted into book
    UnfilledMarketOrderCancelled,   // remaining market order cancelled
    OrderNotFound,
    Trade,                          // two orders matched
    BookUpdated                     // price level / best bid / best ask changed
};

enum class ReasonCode: uint8_t {
    None = 0,
    UnknownMessageType = 1,
    UnknownOrderType = 2,
    UnknownSide = 3,
    InvalidOrderId = 4,
    InvalidSymbolId = 5,
    InvalidLimitPrice = 6,
    InvalidQuantity = 7,
    DuplicateActiveOrderId = 8,
    OrderIdNotFound = 9,
    InvalidMessageType = 10,
    NotAModifyOrder = 11,
};
