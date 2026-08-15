#pragma once

#include <cstddef>
#include <iosfwd>
#include <limits>
#include <unordered_map>
#include <vector>
#include <optional>

#include "types.hpp"
#include "order.hpp"
#include "event.hpp"

class LadderPoolOrderBook {
    public:
        explicit LadderPoolOrderBook(
            Price min_price = 9000, 
            Price max_price = 11000, 
            Price tick_size = 1, 
            std::size_t expected_orders = 0
        );

        // Disable copy constructor and copy assignment
        LadderPoolOrderBook(const LadderPoolOrderBook&) = delete;
        LadderPoolOrderBook& operator = (const LadderPoolOrderBook&) = delete;

        std::vector<Event> submit(const OrderRequest& req);
        std::vector<Event> submit(const NewOrderRequest& req);
        std::vector<Event> submit(const ModifyOrderRequest& req);
        std::vector<Event> submit(const CancelOrderRequest& req);

        // Constant member functions: can not modify LadderPoolOrderBook object, i.e., data structures of this class (asks_, bids_, order_lookup_, next_sequence_number_) 
        std::optional<Price> best_bid() const;
        std::optional<Price> best_ask() const;

        std::optional<Quantity> best_bid_quantity() const;
        std::optional<Quantity> best_ask_quantity() const;

        bool contains_order(OrderId order_id) const;

        // Debugging/testing helper
        bool check_invariants(InvariantViolation& violation) const;
        void debug_print(std::ostream& os) const;

    private:
        using NodeIndex = std::size_t;
        using PriceLevelIndex = std::size_t;
        static constexpr NodeIndex invalid_node = std::numeric_limits<NodeIndex>::max();
        static constexpr PriceLevelIndex invalid_level = std::numeric_limits<PriceLevelIndex>::max();

        struct PriceLevel{
            NodeIndex head = invalid_node;
            NodeIndex tail = invalid_node;
            Quantity total_quantity = 0;

            bool empty() const {
                return head == invalid_node;
            }
        };

        struct OrderNode {
            Order order;

            NodeIndex prev = invalid_node;
            NodeIndex next = invalid_node;
            
            NodeIndex next_free = invalid_node;
            bool in_use = false;  
        };

    private:
        Price min_price_;
        Price max_price_;
        Price tick_size_;
        PriceLevelIndex level_count_;

        std::vector<PriceLevel> bid_levels_;
        std::vector<PriceLevel> ask_levels_;

        std::vector<OrderNode> nodes_;                      // pool storage
        NodeIndex free_head_ = invalid_node;                        // head of free list

        std::unordered_map<OrderId, NodeIndex> order_lookup_;

        PriceLevelIndex best_bid_level_index_ = invalid_level;
        PriceLevelIndex best_ask_level_index_ = invalid_level;

        SequenceNumber next_sequence_number_ = 1;

    private:
        static bool is_valid_new_order_request(const NewOrderRequest& req, Reason& reason);
        static bool is_valid_modify_order_request(const ModifyOrderRequest& req, Reason& reason);
        static bool is_valid_cancel_order_request(const CancelOrderRequest& req, Reason& reason);

        void handle_new_order(const NewOrderRequest& req, std::vector<Event>& events);
        void handle_modify_order(const ModifyOrderRequest& req, std::vector<Event>& events);
        void handle_cancel_order(const CancelOrderRequest& req, std::vector<Event>& events);
        // Use pointer instead of reference since reference can't be null
        // removed_order is optional. When provided, it receives a copy of the order before the node is released
        bool remove_order(OrderId order_id, Reason& reason, Order* removed_order = nullptr);

        void match_buy(Order& incoming, std::vector<Event>& events, bool is_market);
        void match_sell(Order& incoming, std::vector<Event>& events, bool is_market);

        void add_resting_order(Order&& order, std::vector<Event>& events);
    
    private:
        bool price_to_level_index(Price price, PriceLevelIndex& level_index) const;
        Price level_index_to_price(PriceLevelIndex level_index) const;

        NodeIndex allocate_node(Order&& order);
        void free_node(NodeIndex node_index);

        void push_back_level(PriceLevel& level, NodeIndex node_index);
        void unlink_from_level(PriceLevel& level, NodeIndex node_index);

        void refresh_best_bid_from(PriceLevelIndex start_level_index);
        void refresh_best_ask_from(PriceLevelIndex start_level_index);
};
