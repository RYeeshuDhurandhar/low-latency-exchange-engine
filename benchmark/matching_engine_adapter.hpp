#pragma once

#include <memory>

#include "benchmark_common.hpp"
#include "matching_engine.hpp"
#include "map_order_book.hpp"
#include "ladder_pool_order_book.hpp"

class MapMatchingEngineAdapter final : public IBenchmarkBook {
    public:
        explicit MapMatchingEngineAdapter(std::size_t expected_orders) : expected_orders_(expected_orders) {
            reset();
        }

        void reset() override {
            engine_ = std::make_unique<MatchingEngine<MapOrderBook>>(expected_orders_);
        }

        void process(const BenchRequest& req, BenchmarkEventSink& sink) override {
            OrderRequest real_request = to_order_request(req);
            std::vector<Event> events = engine_->process(std::move(real_request));
            for (const Event& event : events) {
                sink.on_event(event);
            }
        }

        const char* name() const override {
            return "map_engine";
        }

    private:
        std::size_t expected_orders_ = 0;
        std::unique_ptr<MatchingEngine<MapOrderBook>> engine_;
};

class LadderPoolMatchingEngineAdapter final : public IBenchmarkBook {
public:
    LadderPoolMatchingEngineAdapter(
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
        engine_ = std::make_unique<MatchingEngine<LadderPoolOrderBook>>(
            min_price_,
            max_price_,
            tick_size_,
            expected_orders_
        );
    }

    void process(const BenchRequest& req, BenchmarkEventSink& sink) override {
        OrderRequest real_request = to_order_request(req);

        std::vector<Event> events = engine_->process(std::move(real_request));

        for (const Event& event : events) {
            sink.on_event(event);
        }
    }

    const char* name() const override {
        return "ladder_pool_engine";
    }

private:
    Price min_price_ = 0;
    Price max_price_ = 0;
    Price tick_size_ = 1;
    std::size_t expected_orders_ = 0;

    std::unique_ptr<MatchingEngine<LadderPoolOrderBook>> engine_;
};

inline std::unique_ptr<IBenchmarkBook> make_benchmark_target(const std::string& impl, Price min_price, Price max_price, Price tick_size, std::size_t expected_orders) {
    if (impl == "map") {
        auto book = std::make_unique<MapMatchingEngineAdapter>(expected_orders);
        return book;
    }

    if (impl == "ladder_pool") {
        auto book = std::make_unique<LadderPoolMatchingEngineAdapter>(
            min_price,
            max_price,
            tick_size,
            expected_orders
        );

        book->reset();
        return book;
    }

    throw std::invalid_argument("unknown implementation: " + impl);
}
