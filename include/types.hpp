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

enum class OrderStatus: uint8_t {
    Unknown = 0,
    New,
    PartiallyFilled,
    Resting,
};

enum class EventType: uint8_t {
    Unknown = 0,                    // default event type for debugging
    OrderAccepted,                  // new order accepted by engine
    OrderRejected,                  // invalid request / duplicate / unknown cancel
    OrderCancelled,                 // order removed from book
    OrderModified,                  // modify request accepted
    OrderRested,                    // remaining limit order inserted into book
    UnfilledMarketOrderCancelled,   // remaining market order cancelled
    Trade,                          // two orders matched
};

enum class RequestType: uint8_t {
    Unknown = 0,
    New,
    Modify,
    Cancel
};

enum class Reason: uint8_t {
    None = 0,
    UnknownOrderType,
    UnknownSide,
    InvalidOrderId,
    InvalidSymbolId,
    InvalidLimitPrice,
    InvalidQuantity,
    DuplicateActiveOrderId,
    OrderNotFound,
    OrderIdNotFound,
};

enum class InvariantViolation {
    None = 0,

    EmptyBidPriceLevel,
    EmptyAskPriceLevel,

    BidOrderWrongSide,
    AskOrderWrongSide,

    OrderPriceMismatch,
    ZeroRemainingQuantity,

    OrderMissingFromLookup,
    LookupSideMismatch,
    LookupPriceMismatch,
    LookupIteratorMismatch,

    LevelQuantityMismatch,
    LookupSizeMismatch,

    CrossedBook
};
