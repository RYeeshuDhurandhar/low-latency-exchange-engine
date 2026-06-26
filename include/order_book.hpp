#pragma once

#include <functional>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>
#include <optional>

#include "types.hpp"
#include "order.hpp"
#include "event.hpp"

class OrderBook {
    public:
        // Explicitly ask compiler to generate default constructor
        OrderBook() = default;

        // Disable copy constructor and copy assignment
        // Reason: copied order_lookup_ might contain iterators pointing into the old book’s lists, not the copied book’s lists, which is unsafe
        OrderBook(const OrderBook&) = delete;
        OrderBook& operator = (const OrderBook&) = delete;

        std::vector<Event> submit(const OrderRequest& req);

        // Can not modify OrderBook object, i.e., data structures of this class (asks_, bids_, order_lookup_, next_sequence_number_) 
        std::optional<Price> best_bid() const;
        std::optional<Price> best_ask() const;

        std::optional<Quantity> best_bid_quantity() const;
        std::optional<Quantity> best_ask_quantity() const;

        bool contains_order(OrderId order_id) const;

        // Debugging/testing helper
        bool check_invariants() const;

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

    private:
        static bool is_valid_new_order_request(const OrderRequest& req, ReasonCode& reason_code);
        static bool is_valid_modify_order_request(const OrderRequest& req, ReasonCode& reason_code);

        std::vector<Event> handle_new_order(const OrderRequest& req);
        std::vector<Event> handle_cancel_order(OrderId order_id);
        std::vector<Event> handle_modify_order(const OrderRequest& req);
        // Use pointer instead of reference since reference can't be null
        // removed_order: optional, needed for modify order, does not need for cancel order
        bool remove_order(OrderId order_id, Order* removed_order = nullptr);

        void match_buy(Order& incoming, std::vector<Event>& events, bool is_market);
        void match_sell(Order& incoming, std::vector<Event>& events, bool is_market);

        void add_resting_order(Order&& order, std::vector<Event>& events);
};
