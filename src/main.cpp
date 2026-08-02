#include "order_book.hpp"
#include "order_book_ladder_pool.hpp"

#include <iostream>

const char* event_type_to_string(EventType event_type) {
    switch(event_type){
        case EventType::OrderAccepted:
            return "ACCEPTED";
        case EventType::OrderModified:
            return "MODIFIED";
        case EventType::OrderCancelled:
            return "CANCELLED";
        case EventType::OrderRejected:
            return "REJECTED";
        case EventType::OrderRested:
            return "RESTED";
        case EventType::Trade:
            return "TRADE";
        case EventType::UnfilledMarketOrderCancelled:
            return "UNFILLED_MARKET_ORDER_CANCELLED";
        case EventType::Unknown:
            return "UNKNOWN";
    }

    return "UNKNOWN";
}

const char* request_type_to_string(RequestType type) {
    switch (type) {
        case RequestType::New:
            return "NEW";
        case RequestType::Modify:
            return "MODIFY";
        case RequestType::Cancel:
            return "CANCEL";
        case RequestType::Unknown:
            return "UNKNOWN";
    }

    return "UNKNOWN";
}

const char* order_type_to_string(OrderType type) {
    switch (type) {
        case OrderType::Limit:
            return "LIMIT";
        case OrderType::Market:
            return "MARKET";
        case OrderType::Unknown:
            return "UNKNOWN";
    }

    return "UNKNOWN";
}

const char* side_to_string(Side side) {
    switch (side) {
        case Side::Buy:
            return "BUY";
        case Side::Sell:
            return "SELL";
        case Side::Unknown:
            return "UNKNOWN";
    }

    return "UNKNOWN";
}

const char* reason_to_string(Reason reason) {
    switch(reason){
        case Reason::None:
            return "None";
        case Reason::UnknownOrderType:
            return "UnknownOrderType";
        case Reason::UnknownSide:
            return "UnknownSide";
        case Reason::InvalidOrderId:
            return "InvalidOrderId";
        case Reason::InvalidSymbolId:
            return "InvalidSymbolId";
        case Reason::InvalidLimitPrice:
            return "InvalidLimitPrice";
        case Reason::InvalidQuantity:
            return "InvalidQuantity";
        case Reason::DuplicateActiveOrderId:
            return "DuplicateActiveOrderId";
        case Reason::OrderNotFound:
            return "OrderNotFound";
        case Reason::OrderIdNotFound:
            return "OrderIdNotFound";
    }

    return "None";
}

const char* invariant_violation_to_string(InvariantViolation violation) {
    switch (violation) {
        case InvariantViolation::None:
            return "None";

        case InvariantViolation::EmptyBidPriceLevel:
            return "EmptyBidPriceLevel";

        case InvariantViolation::EmptyAskPriceLevel:
            return "EmptyAskPriceLevel";

        case InvariantViolation::BidOrderWrongSide:
            return "BidOrderWrongSide";

        case InvariantViolation::AskOrderWrongSide:
            return "AskOrderWrongSide";

        case InvariantViolation::OrderPriceMismatch:
            return "OrderPriceMismatch";

        case InvariantViolation::ZeroRemainingQuantity:
            return "ZeroRemainingQuantity";

        case InvariantViolation::OrderMissingFromLookup:
            return "OrderMissingFromLookup";

        case InvariantViolation::LookupSideMismatch:
            return "LookupSideMismatch";

        case InvariantViolation::LookupPriceMismatch:
            return "LookupPriceMismatch";

        case InvariantViolation::LookupIteratorMismatch:
            return "LookupIteratorMismatch";

        case InvariantViolation::LevelQuantityMismatch:
            return "LevelQuantityMismatch";

        case InvariantViolation::LookupSizeMismatch:
            return "LookupSizeMismatch";

        case InvariantViolation::CrossedBook:
            return "CrossedBook";
        
        case InvariantViolation::EmptyLevelHasHead:
            return "EmptyLevelHasHead";
        
        case InvariantViolation::EmptyLevelHasTail:
            return "EmptyLevelHasTail";
    }

    return "UnknownInvariantViolation";
}

void print_events(const std::vector<Event>& events) {
    for(const Event& event : events) {
        std::cout<<"\n"<<event_type_to_string(event.event_type)<<", ";

        if(event.event_type == EventType::OrderAccepted || event.event_type == EventType::OrderModified || event.event_type == EventType::OrderCancelled || event.event_type == EventType::OrderRested) {
            std::cout
            <<"request_type: "<<request_type_to_string(event.request_type)<<", "
            <<"order_id: "<<event.order_id<<", "
            <<"order_type: "<<order_type_to_string(event.order_type)<<", "
            <<"symbol_id: "<<event.symbol_id<<", "
            <<"side: "<<side_to_string(event.side)<<", "
            <<"quantity: "<<event.quantity<<", "
            <<"price: "<<event.price;
        }

        if(event.event_type == EventType::OrderRejected) {
            std::cout
            <<"request_type: "<<request_type_to_string(event.request_type)<<", "
            <<"order_id: "<<event.order_id<<", "
            <<"reason: "<<reason_to_string(event.reason);
        }

        if(event.event_type == EventType::Trade) {
            std::cout
            <<"request_type: "<<request_type_to_string(event.request_type)<<", "
            <<"resting_order_id: "<<event.resting_order_id<<", "
            <<"aggressive_order_id: "<<event.aggressive_order_id<<", "
            <<"order_type: "<<order_type_to_string(event.order_type)<<", "
            <<"symbol_id: "<<event.symbol_id<<", "
            <<"side: "<<side_to_string(event.side)<<", "
            <<"quantity: "<<event.quantity<<", "
            <<"price: "<<event.price;
        }

        if(event.event_type == EventType::UnfilledMarketOrderCancelled) {
            std::cout
            <<"request_type: "<<request_type_to_string(event.request_type)<<", "
            <<"aggressive_order_id: "<<event.aggressive_order_id<<", "
            <<"order_type: "<<order_type_to_string(event.order_type)<<", "
            <<"symbol_id: "<<event.symbol_id<<", "
            <<"side: "<<side_to_string(event.side)<<", "
            <<"quantity: "<<event.quantity<<", "
            <<"price: "<<event.price;
        }
    }
}

template <typename Book>
void print_top_of_book(const Book& book) {
    std::cout << "\n\nTop of book:\n";

    const auto best_bid = book.best_bid();
    const auto best_bid_quantity = book.best_bid_quantity();

    if (best_bid && best_bid_quantity) {
        std::cout
            << "Best bid: " << *best_bid
            << " qty=" << *best_bid_quantity
            << '\n';
    } else {
        std::cout << "Best bid: none\n";
    }

    const auto best_ask = book.best_ask();
    const auto best_ask_quantity = book.best_ask_quantity();

    if (best_ask && best_ask_quantity) {
        std::cout
            << "Best ask: " << *best_ask
            << " qty=" << *best_ask_quantity
            << '\n';
    } else {
        std::cout << "Best ask: none\n";
    }

    std::cout << '\n';
}

template <typename Book>
void run_scenario(Book& book, const char* implementation_name) {
    constexpr SymbolId AAPL = 1;

    std::cout
        << "\n\n========================================\n"
        << "Running implementation: "
        << implementation_name
        << "\n========================================\n";

    std::cout << "\nAdd SELL S1: 40 @ 100.50\n";
    std::vector<Event> events = book.submit(
        NewOrderRequest{
            .order_type = OrderType::Limit,
            .order_id = 1,
            .symbol_id = AAPL,
            .side = Side::Sell,
            .price = 10050,
            .quantity = 40,
        }
    );

    std::cout << "Event size: " << events.size();
    print_events(events);

    std::cout << "\n\nAdd SELL S2: 100 @ 100.75\n";
    print_events(
        book.submit(
            NewOrderRequest{
                .order_type = OrderType::Limit,
                .order_id = 2,
                .symbol_id = AAPL,
                .side = Side::Sell,
                .price = 10075,
                .quantity = 100,
            }
        )
    );

    std::cout << "\n\nAdd BUY B1: 100 @ 100.10\n";
    print_events(
        book.submit(
            NewOrderRequest{
                .order_type = OrderType::Limit,
                .order_id = 3,
                .symbol_id = AAPL,
                .side = Side::Buy,
                .price = 10010,
                .quantity = 100,
            }
        )
    );

    std::cout << "\n\nModify BUY B1: 100 @ 100.00\n";
    print_events(
        book.submit(
            ModifyOrderRequest{
                .order_type = OrderType::Limit,
                .order_id = 3,
                .price = 10000,
                .quantity = 100,
            }
        )
    );

    print_top_of_book(book);

    std::cout << "\nIncoming BUY B2: 120 @ 100.75\n";
    print_events(
        book.submit(
            NewOrderRequest{
                .order_type = OrderType::Limit,
                .order_id = 4,
                .symbol_id = AAPL,
                .side = Side::Buy,
                .price = 10075,
                .quantity = 120,
            }
        )
    );

    print_top_of_book(book);

    std::cout << "\nCancel B1 order_id=3\n";
    print_events(
        book.submit(
            CancelOrderRequest{
                .order_id = 3,
            }
        )
    );

    print_top_of_book(book);

    InvariantViolation violation = InvariantViolation::None;
    const bool invariants_ok = book.check_invariants(violation);

    std::cout
        << "\nInvariants OK? "
        << (invariants_ok ? "yes" : "no")
        << "\nReason: "
        << invariant_violation_to_string(violation)
        << '\n';

    book.debug_print(std::cout);
}

int main() {
    OrderBook map_book(20);

    OrderBookLadderPool ladder_pool_book(
        9000,   // minimum price
        11000,  // maximum price
        1,      // tick size
        20      // expected orders
    );

    run_scenario(map_book, "Map + list order book");
    run_scenario(ladder_pool_book, "Price ladder + object pool");

    return 0;
}
