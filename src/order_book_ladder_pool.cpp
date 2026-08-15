#include "ladder_pool_order_book.hpp"

#include <cstddef>
#include <algorithm>
#include <iostream>
#include <type_traits>
#include <stdexcept>
#include <variant>
#include <utility>

LadderPoolOrderBook::LadderPoolOrderBook(
    Price min_price, 
    Price max_price, 
    Price tick_size, 
    std::size_t expected_orders
) 
  : min_price_(min_price), 
    max_price_(max_price), 
    tick_size_(tick_size),
    level_count_(0) {

    if(tick_size_ == 0) {
        throw std::invalid_argument("tick_size must be non-zero");
    }

    if (max_price_ < min_price_) {
        throw std::invalid_argument("max_price must be >= min_price");
    }

    const Price price_span = max_price_ - min_price_;

    if (price_span % tick_size_ != 0) {
        throw std::invalid_argument("price range must be divisible by tick_size");
    }

    level_count_ = static_cast<PriceLevelIndex>(price_span / tick_size_) + 1;

    bid_levels_.resize(level_count_);
    ask_levels_.resize(level_count_);

    if(expected_orders > 0) {
        nodes_.reserve(expected_orders);
        order_lookup_.reserve(expected_orders);
    }
}

std::vector<Event> LadderPoolOrderBook::submit(const OrderRequest& req) {
    std::vector<Event> events;
    events.reserve(4);          // Generally enough for common requests

    std::visit(
        [this, &events](const auto& actual_req) {
            using T = std::decay_t<decltype(actual_req)>;

            if constexpr (std::is_same_v<T, NewOrderRequest>) {
                handle_new_order(actual_req, events);
            } else if constexpr (std::is_same_v<T, ModifyOrderRequest>) {
                handle_modify_order(actual_req, events);
            } else if constexpr (std::is_same_v<T, CancelOrderRequest>) {
                handle_cancel_order(actual_req, events);
            }
        },
        req
    );

    return events;
}

std::vector<Event> LadderPoolOrderBook::submit(const NewOrderRequest& req) {
    std::vector<Event> events;
    events.reserve(4);

    handle_new_order(req, events);

    return events;
}

std::vector<Event> LadderPoolOrderBook::submit(const ModifyOrderRequest& req) {
    std::vector<Event> events;
    events.reserve(4);

    handle_modify_order(req, events);

    return events;
}

std::vector<Event> LadderPoolOrderBook::submit(const CancelOrderRequest& req) {
    std::vector<Event> events;
    events.reserve(4);

    handle_cancel_order(req, events);

    return events;
}

std::optional<Price> LadderPoolOrderBook::best_bid() const {
    if(best_bid_level_index_ == invalid_level) {
        return std::nullopt;
    }

    return level_index_to_price(best_bid_level_index_);
}

std::optional<Price> LadderPoolOrderBook::best_ask() const {
    if(best_ask_level_index_ == invalid_level) {
        return std::nullopt;
    }

    return level_index_to_price(best_ask_level_index_);
}

std::optional<Quantity> LadderPoolOrderBook::best_bid_quantity() const {
    if(best_bid_level_index_ == invalid_level) {
        return std::nullopt;
    }

    return bid_levels_[best_bid_level_index_].total_quantity;
}

std::optional<Quantity> LadderPoolOrderBook::best_ask_quantity() const {
    if(best_ask_level_index_ == invalid_level) {
        return std::nullopt;
    }

    return ask_levels_[best_ask_level_index_].total_quantity;
}

bool LadderPoolOrderBook::contains_order(OrderId order_id) const {
    return order_lookup_.find(order_id) != order_lookup_.end();
}

bool LadderPoolOrderBook::price_to_level_index(Price price, PriceLevelIndex& level_index) const {
    if (price < min_price_ || price > max_price_) {
        return false;
    }

    const Price offset = price - min_price_;
    if (offset % tick_size_ != 0) {
        return false;
    }
    level_index = static_cast<PriceLevelIndex>(offset / tick_size_);

    return level_index < level_count_;
}

Price LadderPoolOrderBook::level_index_to_price(PriceLevelIndex level_index) const {
    if (level_index >= level_count_) {
        throw std::out_of_range("price-level index is out of range");
    }

    return min_price_ + static_cast<Price>(level_index) * tick_size_;
}

LadderPoolOrderBook::NodeIndex LadderPoolOrderBook::allocate_node(Order&& order) {
    if(free_head_ != invalid_node) {
        const NodeIndex node_index = free_head_;

        OrderNode& node = nodes_[node_index];

        free_head_ = node.next_free;

        node.order = std::move(order);      // actual move happens here!
        node.prev = invalid_node;
        node.next = invalid_node;
        node.next_free = invalid_node;
        node.in_use = true;

        return node_index;
    }

    const NodeIndex node_index = nodes_.size();

    nodes_.push_back(
        OrderNode{
            .order = std::move(order),      // OR actual move happens here!
            .prev = invalid_node,
            .next = invalid_node,
            .next_free = invalid_node,
            .in_use = true,
        }
    );

    return node_index;
}

void LadderPoolOrderBook::free_node(NodeIndex node_index) {
    OrderNode& node = nodes_[node_index];

    node.prev = invalid_node;
    node.next = invalid_node;
    node.next_free = free_head_;
    node.in_use = false;
    
    free_head_ = node_index;
}

void LadderPoolOrderBook::push_back_level(PriceLevel& level, NodeIndex node_index) {
    OrderNode& node = nodes_[node_index];
    node.prev = level.tail;
    node.next = invalid_node;

    if(level.tail != invalid_node) {
        nodes_[level.tail].next = node_index;
    } else {
        level.head = node_index;
    }

    level.tail = node_index;
}

void LadderPoolOrderBook::unlink_from_level(PriceLevel& level, NodeIndex node_index) {
    OrderNode& node = nodes_[node_index];

    if(node.prev != invalid_node) {
        nodes_[node.prev].next = node.next;
    } else {
        level.head = node.next;
    }

    if(node.next != invalid_node) {
        nodes_[node.next].prev = node.prev;
    } else {
        level.tail = node.prev;
    }

    node.prev = invalid_node;
    node.next = invalid_node;
}

void LadderPoolOrderBook::refresh_best_bid_from(PriceLevelIndex start_level_index) {
    if(level_count_ == 0) {
        best_bid_level_index_ = invalid_level;
        return;
    }

    if(start_level_index >= level_count_) {
        start_level_index = level_count_ - 1;
    }

    for(PriceLevelIndex level_index = start_level_index;; level_index--) {
        if(!bid_levels_[level_index].empty()) {
            best_bid_level_index_ = level_index;
            return;
        }

        if (level_index == 0) {
            break;
        }
    }

    best_bid_level_index_ = invalid_level;
}

void LadderPoolOrderBook::refresh_best_ask_from(PriceLevelIndex start_level_index) {
    if (start_level_index >= level_count_) {
        best_ask_level_index_ = invalid_level;
        return;
    }

    for(PriceLevelIndex level_index = start_level_index; level_index < level_count_; level_index++) {
        if(!ask_levels_[level_index].empty()) {
            best_ask_level_index_ = level_index;
            return;
        }
    }

    best_ask_level_index_ = invalid_level;
}

bool LadderPoolOrderBook::is_valid_new_order_request(const NewOrderRequest& req, Reason& reason) {
    reason = Reason::None;

    if(req.order_type == OrderType::Unknown) {
        reason = Reason::UnknownOrderType;
        return false;
    }

    if(req.order_id == 0) {
        reason = Reason::InvalidOrderId;
        return false;
    }

    if(req.symbol_id == 0) {
        reason = Reason::InvalidSymbolId;
        return false;
    }

    if(req.side == Side::Unknown) {
        reason = Reason::UnknownSide;
        return false;
    }

    if(req.order_type == OrderType::Limit && req.price == 0) {
        reason = Reason::InvalidLimitPrice;
        return false;
    }

    if(req.quantity == 0) {
        reason = Reason::InvalidQuantity;
        return false;
    }

    return true;
}

/*
 * In modify order, req contains:
 *      - message_type
 *      - order_type
 *      - order_id
 *      - quantity
 *      - price (only if limit order, else no need)
 * 
 * This function does not check the following since modify does not change them:
 *      - side
 *      - symbol_id
*/
bool LadderPoolOrderBook::is_valid_modify_order_request(const ModifyOrderRequest& req, Reason& reason) {
    reason = Reason::None;

    if(req.order_type == OrderType::Unknown) {
        reason = Reason::UnknownOrderType;
        return false;
    }

    if(req.order_id == 0) {
        reason = Reason::InvalidOrderId;
        return false;
    }

    if(req.order_type == OrderType::Limit && req.price == 0) {
        reason = Reason::InvalidLimitPrice;
        return false;
    }

    if(req.quantity == 0) {
        reason = Reason::InvalidQuantity;
        return false;
    }

    return true;
}

bool LadderPoolOrderBook::is_valid_cancel_order_request(const CancelOrderRequest& req, Reason& reason) {
    reason = Reason::None;

    if(req.order_id == 0) {
        reason = Reason::InvalidOrderId;
        return false;
    }

    return true;
}

void LadderPoolOrderBook::handle_new_order(const NewOrderRequest& req, std::vector<Event>& events) {
    Reason reason;
    if(!is_valid_new_order_request(req, reason)) {
        events.push_back(
            Event{
                .event_type = EventType::OrderRejected,
                .request_type = RequestType::New,
                .order_id = req.order_id,
                .order_type = req.order_type,
                .symbol_id = req.symbol_id,
                .side = req.side,
                .quantity = req.quantity,
                .price = req.price,
                .reason = reason,
            }
        );

        return;
    }

    if(req.order_type == OrderType::Limit) {
        PriceLevelIndex level_index = invalid_level;

        if(!price_to_level_index(req.price, level_index)) {
            events.push_back(
                Event{
                    .event_type = EventType::OrderRejected,
                    .request_type = RequestType::New,
                    .order_id = req.order_id,
                    .order_type = req.order_type,
                    .symbol_id = req.symbol_id,
                    .side = req.side,
                    .quantity = req.quantity,
                    .price = req.price,
                    .reason = Reason::InvalidLimitPrice,
                }
            );
            
            return;
        }

    }

    if(contains_order(req.order_id)) {
        events.push_back(
            Event{
                .event_type = EventType::OrderRejected,
                .request_type = RequestType::New,
                .order_id = req.order_id,
                .order_type = req.order_type,
                .symbol_id = req.symbol_id,
                .side = req.side,
                .quantity = req.quantity,
                .price = req.price,
                .reason = Reason::DuplicateActiveOrderId,
            }
        );

        return;
    }

    Order incoming = Order{
        .order_id = req.order_id,
        .symbol_id = req.symbol_id,
        .side = req.side,
        .price = req.price,
        .original_quantity = req.quantity,
        .remaining_quantity = req.quantity,
        .sequence_number = next_sequence_number_++,
        .order_status = OrderStatus::New,
    };

    events.push_back(
        Event{
            .event_type = EventType::OrderAccepted,
            .request_type = RequestType::New,
            .order_id = incoming.order_id,
            .order_type = req.order_type,
            .symbol_id = incoming.symbol_id,
            .side = incoming.side,
            .quantity = incoming.original_quantity,
            .price = incoming.price,
        }
    );

    const bool is_market = req.order_type == OrderType::Market;

    if(incoming.side == Side::Buy) {
        match_buy(incoming, events, is_market);
    } else {
        match_sell(incoming, events, is_market);
    }

    if(incoming.remaining_quantity > 0) {
        if(!is_market) {
            add_resting_order(std::move(incoming), events);
        } else {
            events.push_back(
                Event{
                    .event_type = EventType::UnfilledMarketOrderCancelled,
                    .request_type = RequestType::New,
                    .aggressive_order_id = incoming.order_id,
                    .order_type = req.order_type,
                    .symbol_id = incoming.symbol_id,
                    .side = incoming.side,
                    .quantity = incoming.remaining_quantity,
                    .price = incoming.price,
                }
            );
        }
    }

    return;
}

void LadderPoolOrderBook::match_buy(Order& incoming, std::vector<Event>& events, bool is_market) {
    while(incoming.remaining_quantity > 0 && best_ask_level_index_ != invalid_level) {
        const PriceLevelIndex ask_level_index = best_ask_level_index_;
        const Price best_ask_price = level_index_to_price(ask_level_index);

        // Limit buy can only match if ask price <= buy limit
        if(!is_market && best_ask_price > incoming.price) {
            break;
        }

        PriceLevel& price_level = ask_levels_[ask_level_index];

        while(incoming.remaining_quantity > 0 && !price_level.empty()) {
            const NodeIndex resting_index = price_level.head;
            Order& resting = nodes_[resting_index].order;
            Quantity trade_qty = std::min(resting.remaining_quantity, incoming.remaining_quantity);

            incoming.remaining_quantity -= trade_qty;
            resting.remaining_quantity -= trade_qty;
            price_level.total_quantity -= trade_qty;

            events.push_back(
                Event{
                    .event_type = EventType::Trade,
                    .request_type = RequestType::New,
                    .resting_order_id = resting.order_id,
                    .aggressive_order_id = incoming.order_id,
                    .order_type = (is_market) ? OrderType::Market : OrderType::Limit,
                    .symbol_id = resting.symbol_id,
                    .side = incoming.side,
                    .quantity = trade_qty,
                    .price = resting.price,
                }
            );

            if(resting.remaining_quantity == 0) {
                order_lookup_.erase(resting.order_id);
                unlink_from_level(price_level, resting_index);
                free_node(resting_index);
            } else {
                resting.order_status = OrderStatus::PartiallyFilled;
            }
        }

        if(price_level.empty()) {
            refresh_best_ask_from(ask_level_index);
        }
    }
}

void LadderPoolOrderBook::match_sell(Order& incoming, std::vector<Event>& events, bool is_market) {
    while(incoming.remaining_quantity > 0 && best_bid_level_index_ != invalid_level) {
        const PriceLevelIndex bid_level_index = best_bid_level_index_;
        const Price best_bid_price = level_index_to_price(bid_level_index);

        // Limit sell can only match if bid price >= sell limit
        if(!is_market && best_bid_price < incoming.price) {
            break;
        }

        PriceLevel& price_level = bid_levels_[bid_level_index];

        while(incoming.remaining_quantity > 0 && !price_level.empty()) {
            const NodeIndex resting_index = price_level.head;
            Order& resting = nodes_[resting_index].order;
            Quantity trade_qty = std::min(resting.remaining_quantity, incoming.remaining_quantity);

            incoming.remaining_quantity -= trade_qty;
            resting.remaining_quantity -= trade_qty;
            price_level.total_quantity -= trade_qty;

            events.push_back(
                Event{
                    .event_type = EventType::Trade,
                    .request_type = RequestType::New,
                    .resting_order_id = resting.order_id,
                    .aggressive_order_id = incoming.order_id,
                    .order_type = (is_market) ? OrderType::Market : OrderType::Limit,
                    .symbol_id = resting.symbol_id,
                    .side = incoming.side,
                    .quantity = trade_qty,
                    .price = resting.price,
                }
            );

            if(resting.remaining_quantity == 0) {
                order_lookup_.erase(resting.order_id);
                unlink_from_level(price_level, resting_index);
                free_node(resting_index);
            } else {
                resting.order_status = OrderStatus::PartiallyFilled;
            }
        }

        if(price_level.empty()) {
            refresh_best_bid_from(bid_level_index);
        }
    }
}

void LadderPoolOrderBook::add_resting_order(Order&& order, std::vector<Event>& events) {
    PriceLevelIndex level_index = invalid_level;

    if(!price_to_level_index(order.price, level_index)) {
        events.push_back(
            Event{
                .event_type = EventType::OrderRejected,
                .request_type = RequestType::New,
                .order_id = order.order_id,
                .order_type = OrderType::Limit,
                .symbol_id = order.symbol_id,
                .side = order.side,
                .quantity = order.remaining_quantity,
                .price = order.price,
                .reason = Reason::InvalidLimitPrice,
            }
        );
        return;
    }

    order.order_status = OrderStatus::Resting;

    const OrderId order_id = order.order_id;
    const SymbolId symbol_id = order.symbol_id;
    const Side side = order.side;
    const Quantity remaining_quantity = order.remaining_quantity;
    const Price price = order.price;

    const NodeIndex node_index = allocate_node(std::move(order));     // Note: order is not used after this since it is moved and may no longer contain the old value

    if(side == Side::Buy) {
        PriceLevel& price_level = bid_levels_[level_index];

        push_back_level(price_level, node_index);
        price_level.total_quantity += remaining_quantity;

        // Update best_bid_level_index_
        if(best_bid_level_index_ == invalid_level || level_index > best_bid_level_index_) {
            best_bid_level_index_ = level_index;
        }
    } else {
        PriceLevel& price_level = ask_levels_[level_index];

        push_back_level(price_level, node_index);
        price_level.total_quantity += remaining_quantity;

        // Update best_ask_level_index_
        if(best_ask_level_index_ == invalid_level || level_index < best_ask_level_index_) {
            best_ask_level_index_ = level_index;
        }
    }

    // Construct key/value directly. Avoid creating default OrderLocation first and then assigning. Direct
    order_lookup_.emplace(
        order_id,
        node_index
    );

    events.push_back(
        Event{
            .event_type = EventType::OrderRested,
            .request_type = RequestType::New,
            .order_id = order_id,
            .order_type = OrderType::Limit,
            .symbol_id = symbol_id,
            .side = side,
            .quantity = remaining_quantity,
            .price = price,
        }
    );
}

void LadderPoolOrderBook::handle_cancel_order(const CancelOrderRequest& req, std::vector<Event>& events) {
    Order removed_order;
    Reason reason;

    if(!is_valid_cancel_order_request(req, reason)) {
        events.push_back(
            Event{
                .event_type = EventType::OrderRejected,
                .request_type = RequestType::Cancel,
                .order_id = req.order_id,
                .reason = reason,
            }
        );

        return;
    }

    if(!contains_order(req.order_id)) {
        events.push_back(
            Event{
                .event_type = EventType::OrderRejected,
                .request_type = RequestType::Cancel,
                .order_id = req.order_id,
                .reason = Reason::OrderIdNotFound,
            }
        );

        return;
    }

    if(!remove_order(req.order_id, reason, &removed_order)) {
        events.push_back(
            Event{
                .event_type = EventType::OrderRejected,
                .request_type = RequestType::Cancel,
                .order_id = req.order_id,
                .reason = reason,
            }
        );

        return;
    }

    events.push_back(
        Event{
            .event_type = EventType::OrderCancelled,
            .request_type = RequestType::Cancel,
            .order_id = removed_order.order_id,
            .order_type = OrderType::Limit,
            .symbol_id = removed_order.symbol_id,
            .side = removed_order.side,
            .quantity = removed_order.remaining_quantity,
            .price = removed_order.price,
        }
    );

    return;
}

void LadderPoolOrderBook::handle_modify_order(const ModifyOrderRequest& req, std::vector<Event>& events) {
    Reason reason;
    if(!is_valid_modify_order_request(req, reason)) {
        events.push_back(
            Event{
                .event_type = EventType::OrderRejected,
                .request_type = RequestType::Modify,
                .order_id = req.order_id,
                .order_type = req.order_type,
                .quantity = req.quantity,
                .price = req.price,
                .reason = reason,
            }
        );

        return;
    }

    if (req.order_type == OrderType::Limit) {
        PriceLevelIndex level_index = invalid_node;
        if (!price_to_level_index(req.price, level_index)) {
            events.push_back(
                Event{
                    .event_type = EventType::OrderRejected,
                    .request_type = RequestType::Modify,
                    .order_id = req.order_id,
                    .order_type = req.order_type,
                    .quantity = req.quantity,
                    .price = req.price,
                    .reason = Reason::InvalidLimitPrice,
                }
            );
            return;
        }
    }
    
    if(!contains_order(req.order_id)) {
        events.push_back(
            Event{
                .event_type = EventType::OrderRejected,
                .request_type = RequestType::Modify,
                .order_id = req.order_id,
                .order_type = req.order_type,
                .quantity = req.quantity,
                .price = req.price,
                .reason = Reason::OrderIdNotFound,
            }
        );

        return;
    }

    // Modify = Cancel + New Order
    auto lookup_it = order_lookup_.find(req.order_id);

    if (lookup_it == order_lookup_.end()) {
        events.push_back(
            Event{
                .event_type = EventType::OrderRejected,
                .request_type = RequestType::Modify,
                .order_id = req.order_id,
                .order_type = req.order_type,
                .quantity = req.quantity,
                .price = req.price,
                .reason = Reason::OrderIdNotFound,
            }
        );
        return;
    }

    const NodeIndex old_node_index = lookup_it->second;
    const Order& old_order = nodes_[old_node_index].order;

    // const Order& old_order = nodes_[order_lookup_[req.order_id]].order;
    const SymbolId old_symbol_id = old_order.symbol_id;
    const Side old_side = old_order.side;

    CancelOrderRequest cancel_req = CancelOrderRequest{
        .order_id = req.order_id,
    };

    handle_cancel_order(cancel_req, events);

    NewOrderRequest new_req = NewOrderRequest{
        .order_type = req.order_type,
        .order_id = req.order_id,

        // Modify should not change symbol or side 
        .symbol_id = old_symbol_id,
        .side = old_side,

        .price = req.price,
        .quantity = req.quantity,
    };


    handle_new_order(new_req, events);

    events.push_back(
        Event{
            .event_type = EventType::OrderModified,
            .request_type = RequestType::Modify,
            .order_id = req.order_id,
            .order_type = req.order_type,
            .symbol_id = old_symbol_id,
            .side = old_side,
            .quantity = req.quantity,
            .price = req.price,
        }
    );
}

bool LadderPoolOrderBook::remove_order(OrderId order_id, Reason& reason, Order* removed_order) {

    auto lookup_it = order_lookup_.find(order_id);
    if(lookup_it == order_lookup_.end()) {
        reason = Reason::OrderNotFound;
        return false;
    }

    NodeIndex node_index = lookup_it->second;
    OrderNode& node = nodes_[node_index];
    if (!node.in_use) {
        reason = Reason::OrderNotFound;
        return false;
    }
    
    Order& order = node.order;
    PriceLevelIndex level_index = invalid_level;
    if(!price_to_level_index(order.price, level_index)) {
        reason = Reason::InvalidLimitPrice;
        return false;
    }

    if (removed_order != nullptr) {
        *removed_order = order;
    }

    if(order.side == Side::Buy) {
        PriceLevel& price_level = bid_levels_[level_index];

        price_level.total_quantity -= order.remaining_quantity;
        unlink_from_level(price_level, node_index);
        if(price_level.empty() && best_bid_level_index_ == level_index) {
            refresh_best_bid_from(level_index);
        }
    } else {
        PriceLevel& price_level = ask_levels_[level_index];

        price_level.total_quantity -= order.remaining_quantity;
        unlink_from_level(price_level, node_index);
        
        if(price_level.empty() && best_ask_level_index_ == level_index) {
            refresh_best_ask_from(level_index);
        }
    }

    order_lookup_.erase(lookup_it);
    free_node(node_index);

    return true;
}

bool LadderPoolOrderBook::check_invariants(InvariantViolation& violation) const {
    violation = InvariantViolation::None;

    std::size_t order_count_in_levels = 0;

    auto check_side = [&](const std::vector<PriceLevel>& levels, Side side) -> bool {
            for (PriceLevelIndex price_index = 0; price_index < levels.size(); ++price_index) {
                const PriceLevel& level = levels[price_index];

                if (level.empty()) {
                    if (level.tail != invalid_node) {
                        violation = InvariantViolation::EmptyLevelHasTail;
                        return false;
                    }

                    if (level.total_quantity != 0) {
                        violation = InvariantViolation::LevelQuantityMismatch;
                        return false;
                    }

                    continue;
                }

                if (level.tail == invalid_node) {
                    violation = InvariantViolation::LookupIteratorMismatch;
                    return false;
                }

                Quantity computed_quantity = 0;
                NodeIndex prev = invalid_node;

                for (NodeIndex node_index = level.head;
                     node_index != invalid_node;
                     node_index = nodes_[node_index].next) {
                    if (node_index >= nodes_.size()) {
                        violation = InvariantViolation::LookupIteratorMismatch;
                        return false;
                    }

                    const OrderNode& node = nodes_[node_index];

                    if (!node.in_use) {
                        violation = InvariantViolation::LookupIteratorMismatch;
                        return false;
                    }

                    if (node.prev != prev) {
                        violation = InvariantViolation::LookupIteratorMismatch;
                        return false;
                    }

                    const Order& order = node.order;

                    if (order.side != side) {
                        violation = side == Side::Buy
                            ? InvariantViolation::BidOrderWrongSide
                            : InvariantViolation::AskOrderWrongSide;
                        return false;
                    }

                    if (order.price != level_index_to_price(price_index)) {
                        violation = InvariantViolation::OrderPriceMismatch;
                        return false;
                    }

                    if (order.remaining_quantity == 0) {
                        violation = InvariantViolation::ZeroRemainingQuantity;
                        return false;
                    }

                    auto lookup_it = order_lookup_.find(order.order_id);
                    if (lookup_it == order_lookup_.end()) {
                        violation = InvariantViolation::OrderMissingFromLookup;
                        return false;
                    }

                    if (lookup_it->second != node_index) {
                        violation = InvariantViolation::LookupIteratorMismatch;
                        return false;
                    }

                    computed_quantity += order.remaining_quantity;
                    ++order_count_in_levels;
                    prev = node_index;
                }

                if (prev != level.tail) {
                    violation = InvariantViolation::LookupIteratorMismatch;
                    return false;
                }

                if (computed_quantity != level.total_quantity) {
                    violation = InvariantViolation::LevelQuantityMismatch;
                    return false;
                }
            }

            return true;
        };

    if (!check_side(bid_levels_, Side::Buy)) {
        return false;
    }

    if (!check_side(ask_levels_, Side::Sell)) {
        return false;
    }

    PriceLevelIndex computed_best_bid = invalid_level;
    for (PriceLevelIndex i = bid_levels_.size(); i > 0; --i) {
        PriceLevelIndex level_index = i - 1;
        if (!bid_levels_[level_index].empty()) {
            computed_best_bid = level_index;
            break;
        }
    }

    PriceLevelIndex computed_best_ask = invalid_level;
    for (PriceLevelIndex i = 0; i < ask_levels_.size(); ++i) {
        if (!ask_levels_[i].empty()) {
            computed_best_ask = i;
            break;
        }
    }

    if (computed_best_bid != best_bid_level_index_) {
        violation = InvariantViolation::LookupIteratorMismatch;
        return false;
    }

    if (computed_best_ask != best_ask_level_index_) {
        violation = InvariantViolation::LookupIteratorMismatch;
        return false;
    }

    if (order_count_in_levels != order_lookup_.size()) {
        violation = InvariantViolation::LookupSizeMismatch;
        return false;
    }

    if (best_bid_level_index_ != invalid_level && best_ask_level_index_ != invalid_level) {
        if (level_index_to_price(best_bid_level_index_) >= level_index_to_price(best_ask_level_index_)) {
            violation = InvariantViolation::CrossedBook;
            return false;
        }
    }

    return true;
}

void LadderPoolOrderBook::debug_print(std::ostream& os) const {
    os << "=== Ladder Pool Order Book ===\n";

    os << "ASKS:\n";
    for (PriceLevelIndex i = 0; i < ask_levels_.size(); ++i) {
        const PriceLevel& level = ask_levels_[i];
        if (level.empty()) {
            continue;
        }

        os << "price=" << level_index_to_price(i)
           << " total_qty=" << level.total_quantity
           << " orders=[";

        for (NodeIndex node_index = level.head;
             node_index != invalid_node;
             node_index = nodes_[node_index].next) {
            const Order& order = nodes_[node_index].order;
            os << "{id=" << order.order_id
               << ", qty=" << order.remaining_quantity
               << "} ";
        }

        os << "]\n";
    }

    os << "BIDS:\n";
    for (PriceLevelIndex i = bid_levels_.size(); i > 0; --i) {
        const PriceLevelIndex price_index = i - 1;
        const PriceLevel& level = bid_levels_[price_index];

        if (level.empty()) {
            continue;
        }

        os << "price=" << level_index_to_price(price_index)
           << " total_qty=" << level.total_quantity
           << " orders=[";

        for (NodeIndex node_index = level.head;
             node_index != invalid_node;
             node_index = nodes_[node_index].next) {
            const Order& order = nodes_[node_index].order;
            os << "{id=" << order.order_id
               << ", qty=" << order.remaining_quantity
               << "} ";
        }

        os << "]\n";
    }
}
