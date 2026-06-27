#include "order_book.hpp"

#include <algorithm>
#include <iterator>
#include <iostream>

std::vector<Event> OrderBook::submit(const OrderRequest& req) {
    return std::visit(
        [this](const auto& actual_req) {
            return submit(actual_req);
        },
        req
    );
}

std::vector<Event> OrderBook::submit(const NewOrderRequest& req) {
    return handle_new_order(req);
}

std::vector<Event> OrderBook::submit(const ModifyOrderRequest& req) {
    return handle_modify_order(req);
}

std::vector<Event> OrderBook::submit(const CancelOrderRequest& req) {
    return handle_cancel_order(req);
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

bool OrderBook::is_valid_new_order_request(const NewOrderRequest& req, Reason& reason) {
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
 * This function does not check since modify does not change these:
 *      - side
 *      - symbol_id
*/
bool OrderBook::is_valid_modify_order_request(const ModifyOrderRequest& req, Reason& reason) {
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

bool OrderBook::is_valid_cancel_order_request(const CancelOrderRequest& req, Reason& reason) {
    reason = Reason::None;

    if(req.order_id == 0) {
        reason = Reason::InvalidOrderId;
        return false;
    }

    return true;
}

std::vector<Event> OrderBook::handle_new_order(const NewOrderRequest& req) {
    std::vector<Event> events;

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

        return events;
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
                    .event_type = EventType::Trade,
                    .request_type = RequestType::New,
                    .resting_order_id = resting.order_id,
                    .aggressive_order_id = incoming.order_id,
                    .order_type = (is_market)? OrderType::Market : OrderType::Limit,
                    .symbol_id = resting.symbol_id,
                    .side = incoming.side,
                    .quantity = trade_qty,
                    .price = resting.price,
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
                price_level.orders.pop_front();
            } else {
                resting.order_status = OrderStatus::PartiallyFilled;
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
                .event_type = EventType::OrderRested,
                .request_type = RequestType::New,
                .order_id = it->order_id,
                .order_type = OrderType::Limit,
                .symbol_id = it->symbol_id,
                .side = it->side,
                .quantity = it->remaining_quantity,
                .price = it->price,
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
                .event_type = EventType::OrderRested,
                .request_type = RequestType::New,
                .order_id = it->order_id,
                .order_type = OrderType::Limit,
                .symbol_id = it->symbol_id,
                .side = it->side,
                .quantity = it->remaining_quantity,
                .price = it->price,
            }
        );
    }
}

std::vector<Event> OrderBook::handle_cancel_order(const CancelOrderRequest& req) {
    std::vector<Event> events;
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

        return events;
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

        return events;
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

        return events;
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

    return events;
}

std::vector<Event> OrderBook::handle_modify_order(const ModifyOrderRequest& req) {
    std::vector<Event> events;

    Reason reason;
    
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

        return events;
    }

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

        return events;
    }

    // Modify = Cancel + New Order
    auto lookup_it = order_lookup_.find(req.order_id);
    Order older_order = *(lookup_it->second.it);

    CancelOrderRequest cancel_req = CancelOrderRequest{
        .order_id = req.order_id,
    };
    
    std::vector<Event> events_cancel = handle_cancel_order(cancel_req);
    events.insert(events.end(), events_cancel.begin(), events_cancel.end());

    NewOrderRequest new_req = NewOrderRequest{
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

    events.push_back(
        Event{
            .event_type = EventType::OrderModified,
            .request_type = RequestType::Modify,
            .order_id = req.order_id,
            .order_type = req.order_type,
            .symbol_id = older_order.symbol_id,
            .side = older_order.side,
            .quantity = req.quantity,
            .price = req.price,
        }
    );

    return events;

}

bool OrderBook::remove_order(OrderId order_id, Reason& reason, Order* removed_order) {
    auto lookup_it = order_lookup_.find(order_id);

    OrderLocation order_location = lookup_it->second;

    if(order_location.side == Side::Buy) {
        auto level_it = bids_.find(order_location.price);
        if(level_it == bids_.end()) {
            reason = Reason::OrderNotFound;
            return false;
        }

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
        if(level_it == asks_.end()) {
            reason = Reason::OrderNotFound;
            return false;
        }

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

bool OrderBook::check_invariants(InvariantViolation& violation) const {
    uint64_t order_count_in_book = 0;
    violation = InvariantViolation::None;
    
    auto check_bid_book = [&]() -> bool {
        for(const auto& [price, price_level] : bids_) {
            if(price_level.orders.empty()) {
                violation = InvariantViolation::EmptyBidPriceLevel;
                return false;
            }

            Quantity quantity_at_price_level = 0;

            for(auto it = price_level.orders.begin(); it != price_level.orders.end(); it++) {
                const Order& order = *it;
                
                if(order.side != Side::Buy) {
                    violation = InvariantViolation::BidOrderWrongSide;
                    return false;
                }

                if(order.price != price) {
                    violation = InvariantViolation::OrderPriceMismatch;
                    return false;
                }

                if(order.remaining_quantity == 0) {
                    violation = InvariantViolation::ZeroRemainingQuantity;
                    return false;
                }

                auto lookup_it = order_lookup_.find(order.order_id);
                if(lookup_it == order_lookup_.end()) {
                    violation = InvariantViolation::OrderMissingFromLookup;
                    return false;
                }

                if(lookup_it->second.side != Side::Buy) {
                    violation = InvariantViolation::LookupSideMismatch;
                    return false;
                }

                if(lookup_it->second.price != price) {
                    violation = InvariantViolation::LookupPriceMismatch;
                    return false;
                }

                if(lookup_it->second.it != it) {
                    violation = InvariantViolation::LookupIteratorMismatch;
                    return false;
                }

                quantity_at_price_level += order.remaining_quantity;
                order_count_in_book++;
            }

            if(quantity_at_price_level != price_level.total_quantity) {
                violation = InvariantViolation::LevelQuantityMismatch;
                return false;
            }
        }

        return true;
    };

    auto check_ask_book = [&]() -> bool {
        for(const auto& [price, price_level] : asks_) {
            if(price_level.orders.empty()) {
                violation = InvariantViolation::EmptyAskPriceLevel;
                return false;
            }

            Quantity quantity_at_price_level = 0;

            for(auto it = price_level.orders.begin(); it != price_level.orders.end(); it++) {
                const Order& order = *it;
                
                if(order.side != Side::Sell) {
                    violation = InvariantViolation::AskOrderWrongSide;
                    return false;
                }

                if(order.price != price) {
                    violation = InvariantViolation::OrderPriceMismatch;
                    return false;
                }

                if(order.remaining_quantity == 0) {
                    violation = InvariantViolation::ZeroRemainingQuantity;
                    return false;
                }

                auto lookup_it = order_lookup_.find(order.order_id);
                if(lookup_it == order_lookup_.end()) {
                    violation = InvariantViolation::OrderMissingFromLookup;
                    return false;
                }

                if(lookup_it->second.side != Side::Sell) {
                    violation = InvariantViolation::LookupSideMismatch;
                    return false;
                }

                if(lookup_it->second.price != price) {
                    violation = InvariantViolation::LookupPriceMismatch;
                    return false;
                }

                if(lookup_it->second.it != it) {
                    violation = InvariantViolation::LookupIteratorMismatch;
                    return false;
                }

                quantity_at_price_level += order.remaining_quantity;
                order_count_in_book++;
            }

            if(quantity_at_price_level != price_level.total_quantity) {
                violation = InvariantViolation::LevelQuantityMismatch;
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

    if(order_count_in_book != order_lookup_.size()) {
        violation = InvariantViolation::LookupSizeMismatch;
        return false;
    }

    // After matching, book should not be crossed
    if(!bids_.empty() && asks_.empty()) {
        Price best_bid_price = bids_.begin()->first;
        Price best_ask_price = asks_.begin()->first;

        if(best_bid_price >= best_ask_price) {
            violation = InvariantViolation::CrossedBook;
            return false;
        }
    }

    return true;
}

void OrderBook::debug_print(std::ostream& os) const {
    os << "\n\n========== ORDER BOOK ==========\n";
    os << "\nASKS:\n";

    // asks_ is ascending: lowest ask first.
    // For display, print highest ask first, lowest ask last.
    for(auto price_level_it = asks_.rbegin(); price_level_it != asks_.rend(); price_level_it++) {
        Price price = price_level_it->first;
        const PriceLevel& level = price_level_it->second;

        os << "  Price: " << price
           << " | Total Qty: " << level.total_quantity
           << " | Orders: " << level.orders.size()
           << "\n";

        for (const Order& order : level.orders) {
            os << "      order_id=" << order.order_id
               << " qty=" << order.remaining_quantity
               << " price=" << order.price
               << " seq=" << order.sequence_number
               << "\n";
        }
    }

    os << "\nBIDS:\n";

    for(auto level_it = bids_.begin(); level_it != bids_.end(); ++level_it) {
        Price price = level_it->first;
        const PriceLevel& level = level_it->second;

        os << "  Price: " << price
           << " | Total Qty: " << level.total_quantity
           << " | Orders: " << level.orders.size()
           << "\n";

        for (const Order& order : level.orders) {
            os << " order_id=" << order.order_id
               << " qty=" << order.remaining_quantity
               << " price=" << order.price
               << " seq=" << order.sequence_number
               << "\n";
        }
    }

    os << "\nLOOKUP:\n";

    os << "\nLookup size: " << order_lookup_.size() << '\n';

    for(auto order_it = order_lookup_.begin(); order_it != order_lookup_.end(); order_it++) {
        os << " order_id=" << order_it->first
           << " price=" << order_it->second.price
           << " it_order_id=" << order_it->second.it->order_id
           << "\n";
    }

    os << "\n================================\n";
}
