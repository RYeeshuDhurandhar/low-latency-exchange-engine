#pragma once

#include <functional>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

#include "types.hpp"
#include "order.hpp"
#include "event.hpp"

class OrderBook {
    private:
        struct PriceLevel{
            std::list<Order> orders;
            Quantity total_quantity = 0;
        };
        
        struct OrderLocation {
            Side side;
            Price price;
            std::list<Order>::iterator it;
        };

        using AskBook = std::map<Price, PriceLevel>;
        using BidBook = std::map<Price, PriceLevel, std::greater<Price>>;
        using OrderLookup = std::unordered_map<OrderId, OrderLocation>;

    private:
        AskBook asks_;
        BidBook bids_;
        OrderLookup order_lookup_;
        SequenceNumber next_sequence_number_ = 1;
};
