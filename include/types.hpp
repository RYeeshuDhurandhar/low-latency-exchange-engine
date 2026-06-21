#pragma once

#include <cstdint>

using OrderId = uint64_t;
using SymbolId = uint32_t;
using Price = uint64_t;
using Quantity = uint32_t;
using SequenceNumber = uint64_t;

enum class Side: uint8_t {
    Buy,
    Sell
};

enum class OrderType: uint8_t {
    Limit,
    Market
};

enum class MessageType: uint8_t {
    New,
    Cancel,
    Modify
};

enum class OrderStatus: uint8_t {
    New,
    PartiallyFilled,
    Filled,
    Canceled,
    Rejected
};

enum class EventType: uint8_t {
    OrderAccepted,
    OrderRejected,
    OrderCanceled,
    OrderModified,
    Trade,
    BookUpdated
};
