#pragma once

#include <type_traits>
#include <utility>
#include <vector>

#include "event.hpp"
#include "order.hpp"
#include "types.hpp"

template <typename Book>
class MatchingEngine {
    public:
        template <typename... Args>
        explicit MatchingEngine(Args&&... args) : book_(std::forward<Args>(args)...) {}

        MatchingEngine(const MatchingEngine&) = delete;
        MatchingEngine& operator=(const MatchingEngine&) = delete;


        std::vector<Event> process(OrderRequest req) {
            const SequenceNumber input_sequence_number = next_input_sequence_number++;
            set_input_sequence_number(req, input_sequence_number);

            std::vector<Event> events = book_.submit(req);

            for(Event& event : events) {
                event.input_sequence_number = input_sequence_number;
            }

            if(book_changed(events)) {
                append_book_update_event(req, input_sequence_number, events);
            }

            return events;
        }

        std::vector<Event> process(NewOrderRequest req) {
            return process(
                OrderRequest{
                    std::in_place_type<NewOrderRequest>,
                    std::move(req)      // constructing the variant may copy the NewOrderRequest, therefore, use move
                }
            );
        }

        std::vector<Event> process(ModifyOrderRequest req) {
            return process(
                OrderRequest{
                    std::in_place_type<ModifyOrderRequest>,
                    std::move(req)
                }
            );
        }

        std::vector<Event> process(CancelOrderRequest req) {
            return process(
                OrderRequest{
                    std::in_place_type<CancelOrderRequest>,
                    std::move(req)
                }
            );
        }

        std::optional<Price> best_bid() const {
            return book_.best_bid();
        }

        std::optional<Price> best_ask() const {
            return book_.best_ask();
        }

        std::optional<Quantity> best_bid_quantity() const {
            return book_.best_bid_quantity();
        }

        std::optional<Quantity> best_ask_quantity() const {
            return book_.best_ask_quantity();
        }

        bool check_invariants(InvariantViolation& violation) const {
            return book_.check_invariants(violation);
        }

        void debug_print(std::ostream& os) const {
            book_.debug_print(os);
        }
    
    private:
        static void set_input_sequence_number(OrderRequest& req, SequenceNumber input_sequence_number) {
            std::visit(
                [input_sequence_number](auto& actual_req) {
                    actual_req.input_sequence_number = input_sequence_number;
                },
                req
            );
        }

        static bool event_changes_book(EventType event_type) {
            switch(event_type) {
                case EventType::Trade:
                case EventType::OrderModified:
                case EventType::OrderCancelled:
                case EventType::OrderRested:
                    return true;
                
                default:
                    return false;
            }

        }

        static bool book_changed(const std::vector<Event>& events) {
            for(const Event& event : events) {
                if(event_changes_book(event.event_type)) {
                    return true;
                }
            }

            return false;
        }

        static SymbolId infer_symbol_id(const OrderRequest& req, const std::vector<Event>& events) {
            for(const Event& event : events) {
                if(event.symbol_id != 0) {
                    return event.symbol_id;
                }
            }

            return std::visit(
                [](const auto& actual_req) -> SymbolId {
                    using T = std::decay_t<decltype(actual_req)>;

                    if constexpr (std::is_same_v<T, NewOrderRequest>) {
                        return actual_req.symbol_id;
                    } else {
                        return 0;
                    }
                },
                req
            );
        }

        void append_book_update_event(const OrderRequest& req, SequenceNumber input_sequence_number, std::vector<Event>& events) const {
            const auto best_bid_price = book_.best_bid();
            const auto best_bid_quantity = book_.best_bid_quantity();

            const auto best_ask_price = book_.best_ask();
            const auto best_ask_quantity = book_.best_ask_quantity();

            events.push_back(
                Event{
                    .event_type = EventType::BookUpdated,
                    .request_type = RequestType::Unknown,
                    .input_sequence_number = input_sequence_number,

                    .order_id = 0,
                    .resting_order_id = 0,
                    .aggressive_order_id = 0,

                    .order_type = OrderType::Unknown,
                    .symbol_id = infer_symbol_id(req, events),
                    .side = Side::Unknown,
                    .quantity = 0,
                    .price = 0,

                    .reason = Reason::None,

                    .has_best_bid = best_bid_price.has_value(),
                    .best_bid = best_bid_price.value_or(0),
                    .best_bid_quantity = best_bid_quantity.value_or(0),

                    .has_best_ask = best_ask_price.has_value(),
                    .best_ask = best_ask_price.value_or(0),
                    .best_ask_quantity = best_ask_quantity.value_or(0),
                }
            );
        }

    private:
        Book book_;
        SequenceNumber next_input_sequence_number = 1;
};
