
#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <variant>

#include "map_order_book.hpp"
#include "ladder_pool_order_book.hpp"
#include "benchmark_common.hpp"

class MapOrderBookAdapter final : public IBenchmarkBook {
    public:
        explicit MapOrderBookAdapter(std::size_t expected_orders) : expected_orders_(expected_orders) {
            reset();
        }

        void reset() override {
            book_ = std::make_unique<MapOrderBook>(expected_orders_);
        }

        void process(const BenchRequest& req, BenchmarkEventSink& sink) override {
            OrderRequest real_request = to_order_request(req);
            std::vector<Event> events = book_->submit(real_request);
            for (const Event& event : events) {
                sink.on_event(event);
            }
        }

        const char* name() const override {
            return "map";
        }

    private:
        std::size_t expected_orders_ = 0;
        std::unique_ptr<MapOrderBook> book_;
};

class LadderPoolOrderBookAdapter final : public IBenchmarkBook {
public:
    LadderPoolOrderBookAdapter(
        Price min_price,
        Price max_price,
        Price tick_size,
        std::size_t expected_orders
    )
        : min_price_(min_price),
          max_price_(max_price),
          tick_size_(tick_size),
          expected_orders_(expected_orders) {}

    void reset() override {
        book_ = std::make_unique<LadderPoolOrderBook>(
            min_price_,
            max_price_,
            tick_size_,
            expected_orders_
        );
    }

    void process(const BenchRequest& req, BenchmarkEventSink& sink) override {
        OrderRequest real_request = to_order_request(req);

        std::vector<Event> events = book_->submit(real_request);

        for (const Event& event : events) {
            sink.on_event(event);
        }
    }

    const char* name() const override {
        return "ladder_pool";
    }

private:
    Price min_price_ = 0;
    Price max_price_ = 0;
    Price tick_size_ = 1;
    std::size_t expected_orders_ = 0;

    std::unique_ptr<LadderPoolOrderBook> book_;
};
