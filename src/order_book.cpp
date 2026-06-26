#include "order_book.hpp"

#include <algorithm>
#include <iterator>

std::vector<Event> OrderBook::submit(const OrderRequest& req) {
    switch (req.message_type) {
        case MessageType::New:
            return handle_new_order(req);

        case MessageType::Cancel:
            return handle_cancel_order(req.order_id);

        case MessageType::Modify:
            return handle_modify_order(req);
        
        default:
            std::vector<Event> events;
            events.push_back(
                Event{
                    .type = EventType::OrderRejected,
                    .aggressive_order_id = req.order_id,
                    .symbol_id = req.symbol_id,
                    .price = req.price,
                    .quantity = req.quantity,
                    .reason_code = ReasonCode::InvalidMessageType,
                }
            );
    }

    return {};
}

std::optional<Price> OrderBook::best_bid() const {
    if(bids_.empty()) {
        return std::nullopt;
    }

    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
    if(asks_.empty()) {
        return std::nullopt;
    }

    return asks_.begin()->first;
}

std::optional<Quantity> OrderBook::best_bid_quantity() const {
    if(bids_.empty()) {
        return std::nullopt;
    }

    return bids_.begin()->second.total_quantity;
}

std::optional<Quantity> OrderBook::best_ask_quantity() const {
    if(asks_.empty()) {
        return std::nullopt;
    }

    return asks_.begin()->second.total_quantity;
}

bool OrderBook::contains_order(OrderId order_id) const {
    return order_lookup_.find(order_id) != order_lookup_.end();
}

bool OrderBook::is_valid_new_order_request(const OrderRequest& req, ReasonCode& reason_code) {
    reason_code = ReasonCode::None;

    if(req.message_type == MessageType::Unknown) {
        reason_code = ReasonCode::UnknownMessageType;
        return false;
    }

    if(req.order_type == OrderType::Unknown) {
        reason_code = ReasonCode::UnknownOrderType;
        return false;
    }

    if(req.order_id == 0) {
        reason_code = ReasonCode::InvalidOrderId;
        return false;
    }

    if(req.symbol_id == 0) {
        reason_code = ReasonCode::InvalidSymbolId;
        return false;
    }

    if(req.side == Side::Unknown) {
        reason_code = ReasonCode::UnknownSide;
        return false;
    }

    if(req.order_type == OrderType::Limit && req.price == 0) {
        reason_code = ReasonCode::InvalidLimitPrice;
        return false;
    }

    if(req.quantity == 0) {
        reason_code = ReasonCode::InvalidQuantity;
        return false;
    }

    return true;
}

/*
 * In modify order, req contains:
 *      - message_type
 *      - order_id
 *      - quantity
 *      - order_type
 *      - price (only if limit order, else no need)
 * 
 * This function does not check since modify does not change these:
 *      - side
 *      - symbol_id
*/
bool OrderBook::is_valid_modify_order_request(const OrderRequest& req, ReasonCode& reason_code) {
    reason_code = ReasonCode::None;

    if(req.message_type != MessageType::Modify) {
        reason_code = ReasonCode::NotAModifyOrder;
        return false;
    }

    if(req.order_type == OrderType::Unknown) {
        reason_code = ReasonCode::UnknownOrderType;
        return false;
    }

    if(req.order_id == 0) {
        reason_code = ReasonCode::InvalidOrderId;
        return false;
    }

    if(req.order_type == OrderType::Limit && req.price == 0) {
        reason_code = ReasonCode::InvalidLimitPrice;
        return false;
    }

    if(req.quantity == 0) {
        reason_code = ReasonCode::InvalidQuantity;
        return false;
    }

    return true;
}

std::vector<Event> OrderBook::handle_new_order(const OrderRequest& req) {
    std::vector<Event> events;

    ReasonCode reason_code;
    if(!is_valid_new_order_request(req, reason_code)) {
        events.push_back(
            Event{
                .type = EventType::OrderRejected,
                .order_id = req.order_id,
                .symbol_id = req.symbol_id,
                .price = req.price,
                .quantity = req.quantity,
                .reason_code = reason_code,
            }
        );

        return events;
    }

    if(contains_order(req.order_id)) {
        events.push_back(
            Event{
                .type = EventType::OrderRejected,
                .order_id = req.order_id,
                .symbol_id = req.symbol_id,
                .price = req.price,
                .quantity = req.quantity,
                .reason_code = ReasonCode::DuplicateActiveOrderId,
            }
        );

        return events;
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
            .type = EventType::OrderAccepted,
            .order_id = incoming.order_id,
            .symbol_id = incoming.symbol_id,
            .price = incoming.price,
            .quantity = incoming.original_quantity,
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
                    .type = EventType::UnfilledMarketOrderCancelled,
                    .aggressive_order_id = incoming.order_id,
                    .symbol_id = incoming.symbol_id,
                    .quantity = incoming.remaining_quantity,
                    .price = incoming.price,
                }
            );
        }
    }

    return events;
}

void OrderBook::match_buy(Order& incoming, std::vector<Event>& events, bool is_market) {
    while(incoming.remaining_quantity > 0 && !asks_.empty()) {
        auto best_ask_it = asks_.begin();
        Price best_ask_price = best_ask_it->first;

        // Limit buy can only match if ask price <= buy limit
        if(!is_market && best_ask_price > incoming.price) {
            break;
        }

        PriceLevel& price_level = best_ask_it->second;

        while(incoming.remaining_quantity > 0 && !price_level.orders.empty()) {
            Order& resting = price_level.orders.front();
            Quantity trade_qty = std::min(resting.remaining_quantity, incoming.remaining_quantity);

            incoming.remaining_quantity -= trade_qty;
            resting.remaining_quantity -= trade_qty;
            price_level.total_quantity -= trade_qty;

            events.push_back(
                Event{
                    .type = EventType::Trade,
                    .resting_order_id = resting.order_id,
                    .aggressive_order_id = incoming.order_id,
                    .symbol_id = resting.symbol_id,
                    .price = resting.price,
                    .quantity = trade_qty,
                }
            );

            if(resting.remaining_quantity == 0) {
                order_lookup_.erase(resting.order_id);
                price_level.orders.pop_front();
            }
        }

        if(price_level.orders.empty()) asks_.erase(best_ask_it);
    }
}


void OrderBook::match_sell(Order& incoming, std::vector<Event>& events, bool is_market) {
    while(incoming.remaining_quantity > 0 && !bids_.empty()) {
        auto best_bid_it = bids_.begin();
        Price best_bid_price = best_bid_it->first;

        // Limit sell can only match if bid price >= sell limit
        if(!is_market && best_bid_price < incoming.price) {
            break;
        }

        PriceLevel& price_level = best_bid_it->second;

        while(incoming.remaining_quantity > 0 && !price_level.orders.empty()) {
            Order& resting = price_level.orders.front();
            Quantity trade_qty = std::min(resting.remaining_quantity, incoming.remaining_quantity);

            incoming.remaining_quantity -= trade_qty;
            resting.remaining_quantity -= trade_qty;
            price_level.total_quantity -= trade_qty;

            events.push_back(
                Event{
                    .type = EventType::Trade,
                    .order_id = incoming.order_id,
                    .resting_order_id = resting.order_id,
                    .aggressive_order_id = incoming.order_id,
                    .symbol_id = resting.symbol_id,
                    .price = resting.price,
                    .quantity = trade_qty,
                }
            );

            if(resting.remaining_quantity == 0) {
                order_lookup_.erase(resting.order_id);
                price_level.orders.pop_front();
            }
        }

        if(price_level.orders.empty()) bids_.erase(best_bid_it);
    }
}

void OrderBook::add_resting_order(Order&& order, std::vector<Event>& events) {
    order.order_status = OrderStatus::Resting;

    if(order.side == Side::Buy) {
        PriceLevel& price_level = bids_[order.price];

        price_level.orders.push_back(std::move(order));
        auto it = std::prev(price_level.orders.end());

        price_level.total_quantity += it->remaining_quantity;

        order_lookup_[it->order_id] = OrderLocation{
            .side = it->side,
            .price = it->price,
            .it = it,
        };

        events.push_back(
            Event{
                .type = EventType::OrderRested,
                .order_id = it->order_id,
                .symbol_id = it->symbol_id,
                .price = it->price,
                .quantity = it->remaining_quantity,
            }
        );
    } else {
        PriceLevel& price_level = asks_[order.price];

        price_level.orders.push_back(std::move(order));
        auto it = std::prev(price_level.orders.end());

        price_level.total_quantity += it->remaining_quantity;

        order_lookup_[it->order_id] = OrderLocation{
            .side = it->side,
            .price = it->price,
            .it = it,
        };

        events.push_back(
            Event{
                .type = EventType::OrderRested,
                .order_id = it->order_id,
                .symbol_id = it->symbol_id,
                .price = it->price,
                .quantity = it->remaining_quantity,
            }
        );
    }
}

std::vector<Event> OrderBook::handle_cancel_order(OrderId order_id) {
    std::vector<Event> events;
    Order removed_order;

    if(!remove_order(order_id, &removed_order)) {
        events.push_back(
            Event{
                .type = EventType::OrderNotFound,
                .order_id = order_id,
            }
        );

        return events;
    }

    events.push_back(
        Event{
            .type = EventType::OrderCancelled,
            .order_id = removed_order.order_id,
            .symbol_id = removed_order.symbol_id,
            .price = removed_order.price,
            .quantity = removed_order.remaining_quantity,
        }
    );

    return events;
}

std::vector<Event> OrderBook::handle_modify_order(const OrderRequest& req) {
    std::vector<Event> events;

    ReasonCode reason_code;
    
    if(!contains_order(req.order_id)) {
        events.push_back(
            Event{
                .type = EventType::OrderRejected,
                .order_id = req.order_id,
                .symbol_id = req.symbol_id,
                .price = req.price,
                .quantity = req.quantity,
                .reason_code = ReasonCode::OrderIdNotFound,
            }
        );

        return events;
    }

    if(!is_valid_modify_order_request(req, reason_code)) {
        events.push_back(
            Event{
                .type = EventType::OrderRejected,
                .order_id = req.order_id,
                .symbol_id = req.symbol_id,
                .price = req.price,
                .quantity = req.quantity,
                .reason_code = reason_code,
            }
        );

        return events;
    }

    // Modify = Cancel + New Order
    auto lookup_it = order_lookup_.find(req.order_id);
    Order older_order = *(lookup_it->second.it);

    std::vector<Event> events_cancel = handle_cancel_order(req.order_id);
    events.insert(events.end(), events_cancel.begin(), events_cancel.end());

    OrderRequest new_req = OrderRequest{
        .message_type = MessageType::New,
        .order_type = req.order_type,
        .order_id = req.order_id,

        // Modify should not change symbol or side 
        .symbol_id = older_order.symbol_id,
        .side = older_order.side,

        .price = req.price,
        .quantity = req.quantity,
    };


    std::vector<Event> events_new = handle_new_order(new_req);
    events.insert(events.end(), events_new.begin(), events_new.end());

    return events;

}

bool OrderBook::remove_order(OrderId order_id, Order* removed_order) {
    auto lookup_it = order_lookup_.find(order_id);
    if(lookup_it == order_lookup_.end()) {
        return false;
    }

    OrderLocation order_location = lookup_it->second;

    if(order_location.side == Side::Buy) {
        auto level_it = bids_.find(order_location.price);
        if(level_it == bids_.end()) return false;

        PriceLevel* price_level = &level_it->second;
        
        if(removed_order != nullptr) {
            *removed_order = *(order_location.it);
        }

        price_level->total_quantity -= order_location.it->remaining_quantity;
        price_level->orders.erase(order_location.it);
        if(price_level->orders.empty()) {
            bids_.erase(level_it);
        }
    } else {
        auto level_it = asks_.find(order_location.price);
        if(level_it == asks_.end()) return false;

        PriceLevel* price_level = &level_it->second;
        
        if(removed_order != nullptr) {
            *removed_order = *(order_location.it);
        }

        price_level->total_quantity -= order_location.it->remaining_quantity;
        price_level->orders.erase(order_location.it);
        if(price_level->orders.empty()) {
            asks_.erase(level_it);
        }
    }

    order_lookup_.erase(lookup_it);
    return true;
}

bool OrderBook::check_invariants() const {
    uint64_t order_count_in_book = 0;
    
    auto check_bid_book = [&]() -> bool {
        for(const auto& [price, price_level] : bids_) {
            if(price_level.orders.empty()) {
                return false;
            }

            Quantity quantity_at_price_level = 0;

            for(auto it = price_level.orders.begin(); it != price_level.orders.end(); it++) {
                const Order& order = *it;
                
                if(order.side != Side::Buy) {
                    return false;
                }

                if(order.price != price) {
                    return false;
                }

                if(order.remaining_quantity == 0) {
                    return false;
                }

                auto lookup_it = order_lookup_.find(order.order_id);
                if(lookup_it == order_lookup_.end()) {
                    return false;
                }

                if(lookup_it->second.side != Side::Buy || lookup_it->second.price != price || lookup_it->second.it != it) {
                    return false;
                }

                quantity_at_price_level += order.remaining_quantity;
                order_count_in_book++;
            }

            if(quantity_at_price_level != price_level.total_quantity) {
                return false;
            }
        }

        return true;
    };

    auto check_ask_book = [&]() -> bool {
        for(const auto& [price, price_level] : asks_) {
            if(price_level.orders.empty()) {
                return false;
            }

            Quantity quantity_at_price_level = 0;

            for(auto it = price_level.orders.begin(); it != price_level.orders.end(); it++) {
                const Order& order = *it;
                
                if(order.side != Side::Sell) {
                    return false;
                }

                if(order.price != price) {
                    return false;
                }

                if(order.remaining_quantity == 0) {
                    return false;
                }

                auto lookup_it = order_lookup_.find(order.order_id);
                if(lookup_it == order_lookup_.end()) {
                    return false;
                }

                if(lookup_it->second.side != Side::Sell || lookup_it->second.price != price || lookup_it->second.it != it) {
                    return false;
                }

                quantity_at_price_level += order.remaining_quantity;
                order_count_in_book++;
            }

            if(quantity_at_price_level != price_level.total_quantity) {
                return false;
            }
        }

        return true;
    };

    if(!check_bid_book()) {
        return false;
    }

    if(!check_ask_book()) {
        return false;
    }

    if(order_count_in_book = order_lookup_.size()) {
        return false;
    }

    // After matching, book should not be crossed
    if(!bids_.empty() && asks_.empty()) {
        Price best_bid_price = bids_.begin()->first;
        Price best_ask_price = asks_.begin()->first;

        if(best_bid_price >= best_ask_price) {
            return false;
        }
    }

    return true;
}
