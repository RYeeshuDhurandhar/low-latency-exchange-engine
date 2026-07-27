#include "workload_generator.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>


namespace {

class ActiveOrderIds {
    public:
        bool empty() const {
            return ids_.empty();
        }

        void add(OrderId id) {
            ids_.push_back(id);
        }

        // Used for selecting random order_id for modify and cancel orders
        OrderId pick_and_remove(std::mt19937_64& rng) {
            if(ids_.empty()) {
                return 0;
            }

            std::uniform_int_distribution<std::size_t> dist(0, ids_.size() - 1);
            std::size_t idx = dist(rng);

            OrderId id = ids_[idx];
            ids_[idx] = ids_.back();
            ids_.pop_back();

            return id;
        }

        OrderId pick_without_remove(std::mt19937_64& rng) {
            if(ids_.empty()) {
                return 0;
            }

            std::uniform_int_distribution<std::size_t> dist(0, ids_.size() - 1);

            return ids_[dist(rng)];;
        }

    private:
        std::vector<OrderId> ids_;
};

Quantity random_quantity(std::mt19937_64& rng, const WorkloadConfig& config) {
    std::uniform_int_distribution<Quantity> dist(config.min_quantity, config.max_quantity);
    return dist(rng);
}

Price random_price(std::mt19937_64& rng, Price lo, Price hi, Price tick_size) {
    if(lo > hi) {
        std::swap(lo, hi);
    }
    
    std::uint64_t steps = static_cast<std::uint64_t>((hi-lo) / tick_size);
    std::uniform_int_distribution<std::uint64_t> dist(0, steps);

    return lo + static_cast<Price>(dist(rng)) * tick_size;
}

Side random_side(std::mt19937_64& rng) {
    std::uniform_int_distribution<int> dist(0, 1);

    return dist(rng) == 0 ? Side::Buy : Side::Sell;
}

BenchRequest make_new_limit(const WorkloadConfig& config,
                            OrderId id,
                            Side side,
                            Price price,
                            Quantity qty) {
    BenchRequest req;
    req.bench_op_type = BenchOpType::NewLimit;
    req.order_id = id;
    req.symbol_id = config.symbol_id;
    req.side = side;
    req.order_type = OrderType::Limit;
    req.price = price;
    req.quantity = qty;
    return req;
}

BenchRequest make_new_market(const WorkloadConfig& config,
                             OrderId id,
                             Side side,
                             Quantity qty) {
    BenchRequest req;
    req.bench_op_type = BenchOpType::NewMarket;
    req.order_id = id;
    req.symbol_id = config.symbol_id;
    req.side = side;
    req.order_type = OrderType::Market;
    req.price = 0;
    req.quantity = qty;
    return req;
}

BenchRequest make_cancel(const WorkloadConfig& config, OrderId id) {
    BenchRequest req;
    req.bench_op_type = BenchOpType::Cancel;
    req.order_id = id;
    req.symbol_id = config.symbol_id;
    req.order_type = OrderType::Unknown;
    req.side = Side::Unknown;
    req.price = 0;
    req.quantity = 0;
    return req;
}

BenchRequest make_modify(const WorkloadConfig& config,
                         OrderId id,
                         OrderType order_type,
                         Price new_price,
                         Quantity new_qty) {
    BenchRequest req;
    req.bench_op_type = BenchOpType::Modify;
    req.order_id = id;
    req.symbol_id = config.symbol_id;
    req.order_type = order_type;
    req.side = Side::Unknown;
    req.price = new_price;
    req.quantity = new_qty;
    return req;
}

// Non-crossing price generation.
// Buy orders below mid, sell orders above mid.
Price passive_price(std::mt19937_64& rng, const WorkloadConfig& config, Side side) {
    if (side == Side::Buy) {
        return random_price(
            rng,
            config.min_price,
            config.mid_price - config.tick_size,
            config.tick_size
        );
    }

    return random_price(
        rng,
        config.mid_price + config.tick_size,
        config.max_price,
        config.tick_size
    );
}

// Aggressive/crossing price generation.
// Buy orders above mid, sell orders below mid.
Price aggressive_price(std::mt19937_64& rng, const WorkloadConfig& config, Side side) {
    if (side == Side::Buy) {
        return random_price(
            rng,
            config.mid_price + config.tick_size,
            config.max_price,
            config.tick_size
        );
    }

    return random_price(
        rng,
        config.min_price,
        config.mid_price - config.tick_size,
        config.tick_size
    );
}

std::vector<BenchRequest> mostly_adds(const WorkloadConfig& config) {
    std::mt19937_64 rng(config.seed);
    std::vector<BenchRequest> trace;
    std::uniform_int_distribution<int> op_dist(0, 100);
    trace.reserve(config.num_ops);

    ActiveOrderIds active;
    OrderId next_id = 1;

    for(std::uint64_t i=0; i < config.num_ops; i++) {
        int op = op_dist(rng);
        
        if(op <= 90 || active.empty()) {
            Side side = random_side(rng);
            Price price = passive_price(rng, config, side);
            Quantity qty = random_quantity(rng, config);

            trace.push_back(make_new_limit(config, next_id, side, price, qty));
            active.add(next_id);
            ++next_id;
        } else {
            OrderId id = active.pick_and_remove(rng);
            trace.push_back(make_cancel(config, id));
        }

    }

    return trace;
}

std::vector<BenchRequest> many_cancels(const WorkloadConfig& config) {
    std::mt19937_64 rng(config.seed);
    std::vector<BenchRequest> trace;
    std::uniform_int_distribution<int> op_dist(0, 100);
    trace.reserve(config.num_ops);

    ActiveOrderIds active;
    OrderId next_id = 1;

    for(std::uint64_t i=0; i < config.num_ops; i++) {
        int op = op_dist(rng);
        
        if(op <= 50 || active.empty()) {
            Side side = random_side(rng);
            Price price = passive_price(rng, config, side);
            Quantity qty = random_quantity(rng, config);

            trace.push_back(make_new_limit(config, next_id, side, price, qty));
            active.add(next_id);
            ++next_id;
        } else {
            OrderId id = active.pick_and_remove(rng);
            trace.push_back(make_cancel(config, id));
        }

    }

    return trace;
}

std::vector<BenchRequest> deep_book(const WorkloadConfig& config) {
    std::mt19937_64 rng(config.seed);
    std::vector<BenchRequest> trace;
    trace.reserve(config.num_ops);

    OrderId next_id = 1;

    Price narrow_lo = config.mid_price - 5 * config.tick_size;
    Price narrow_hi = config.mid_price + 5 * config.tick_size;

    for(std::uint64_t i=0; i < config.num_ops; i++) {
        Side side = random_side(rng);

        Price price;

        if(side == Side::Buy) {
            price = random_price(rng, narrow_lo, config.mid_price - config.tick_size, config.tick_size);
        } else {
            price = random_price(rng, config.mid_price + config.tick_size, narrow_hi, config.tick_size);
        }

        Quantity qty = random_quantity(rng, config);
        trace.push_back(make_new_limit(config, next_id, side, price, qty));
        ++next_id;
    }

    return trace;
}

std::vector<BenchRequest> wide_price_range(const WorkloadConfig& config) {
    std::mt19937_64 rng(config.seed);
    std::vector<BenchRequest> trace;
    trace.reserve(config.num_ops);

    OrderId next_id = 1;

    for (std::uint64_t i = 0; i < config.num_ops; ++i) {
        Side side = random_side(rng);
        Price price = passive_price(rng, config, side);
        Quantity qty = random_quantity(rng, config);

        trace.push_back(make_new_limit(config, next_id, side, price, qty));
        ++next_id;
    }

    return trace;
}

std::vector<BenchRequest> same_price_fifo(const WorkloadConfig& config) {
    std::vector<BenchRequest> trace;
    trace.reserve(config.num_ops);

    OrderId next_id = 1;

    std::uint64_t half = config.num_ops / 2;
    Price ask_price = config.mid_price + config.tick_size;

    for (std::uint64_t i = 0; i < half; ++i) {
        trace.push_back(make_new_limit(config, next_id, Side::Sell, ask_price, 1));
        ++next_id;
    }

    for (std::uint64_t i = half; i < config.num_ops; ++i) {
        trace.push_back(make_new_market(config, next_id, Side::Buy, 1));
        ++next_id;
    }

    return trace;
}

std::vector<BenchRequest> market_orders(const WorkloadConfig& config) {
    std::mt19937_64 rng(config.seed);
    std::vector<BenchRequest> trace;
    trace.reserve(config.num_ops);

    OrderId next_id = 1;

    std::uint64_t seed_orders = config.num_ops / 4;

    for (std::uint64_t i = 0; i < seed_orders; ++i) {
        Side side = random_side(rng);
        Price price = passive_price(rng, config, side);
        Quantity qty = random_quantity(rng, config);

        trace.push_back(make_new_limit(config, next_id, side, price, qty));
        ++next_id;
    }

    for (std::uint64_t i = seed_orders; i < config.num_ops; ++i) {
        Side side = random_side(rng);
        Quantity qty = random_quantity(rng, config);

        trace.push_back(make_new_market(config, next_id, side, qty));
        ++next_id;
    }

    return trace;
}

std::vector<BenchRequest> cross_heavy(const WorkloadConfig& config) {
    std::mt19937_64 rng(config.seed);
    std::vector<BenchRequest> trace;
    trace.reserve(config.num_ops);

    OrderId next_id = 1;

    std::uint64_t seed_orders = config.num_ops / 4;

    for (std::uint64_t i = 0; i < seed_orders; ++i) {
        Side side = random_side(rng);
        Price price = passive_price(rng, config, side);
        Quantity qty = random_quantity(rng, config);

        trace.push_back(make_new_limit(config, next_id, side, price, qty));
        ++next_id;
    }

    for (std::uint64_t i = seed_orders; i < config.num_ops; ++i) {
        Side side = random_side(rng);
        Price price = aggressive_price(rng, config, side);
        Quantity qty = random_quantity(rng, config);

        trace.push_back(make_new_limit(config, next_id, side, price, qty));
        ++next_id;
    }

    return trace;
}

std::vector<BenchRequest> modify_heavy(const WorkloadConfig& config) {
    std::mt19937_64 rng(config.seed);
    std::vector<BenchRequest> trace;
    trace.reserve(config.num_ops);

    ActiveOrderIds active;
    OrderId next_id = 1;

    for (std::uint64_t i = 0; i < config.num_ops; ++i) {
        std::uniform_int_distribution<int> op_dist(1, 100);
        int op = op_dist(rng);

        if (op <= 50 || active.empty()) {
            Side side = random_side(rng);
            Price price = passive_price(rng, config, side);
            Quantity qty = random_quantity(rng, config);

            trace.push_back(make_new_limit(config, next_id, side, price, qty));
            active.add(next_id);
            ++next_id;
        } else if (op <= 80) {
            OrderId id = active.pick_without_remove(rng);
            Quantity qty = random_quantity(rng, config);

            // Modify quantity only, keep price 0 if your modify allows quantity-only.
            // If your modify requires price for limit orders, use a valid passive price.
            trace.push_back(make_modify(config, id, OrderType::Limit, config.mid_price, qty));
        } else {
            OrderId id = active.pick_and_remove(rng);
            trace.push_back(make_cancel(config, id));
        }
    }

    return trace;
}

std::vector<BenchRequest> mixed_realistic(const WorkloadConfig& config) {
    std::mt19937_64 rng(config.seed);
    std::vector<BenchRequest> trace;
    trace.reserve(config.num_ops);

    ActiveOrderIds active;
    OrderId next_id = 1;

    for (std::uint64_t i = 0; i < config.num_ops; ++i) {
        std::uniform_int_distribution<int> op_dist(1, 100);
        int op = op_dist(rng);

        if (op <= 40 || active.empty()) {
            Side side = random_side(rng);
            Price price = passive_price(rng, config, side);
            Quantity qty = random_quantity(rng, config);

            trace.push_back(make_new_limit(config, next_id, side, price, qty));
            active.add(next_id);
            ++next_id;
        } else if (op <= 60) {
            OrderId id = active.pick_and_remove(rng);
            trace.push_back(make_cancel(config, id));
        } else if (op <= 70) {
            OrderId id = active.pick_without_remove(rng);
            Quantity qty = random_quantity(rng, config);
            trace.push_back(make_modify(config, id, OrderType::Limit, config.mid_price, qty));
        } else if (op <= 85) {
            Side side = random_side(rng);
            Quantity qty = random_quantity(rng, config);
            trace.push_back(make_new_market(config, next_id, side, qty));
            ++next_id;
        } else {
            Side side = random_side(rng);
            Price price = aggressive_price(rng, config, side);
            Quantity qty = random_quantity(rng, config);
            trace.push_back(make_new_limit(config, next_id, side, price, qty));
            ++next_id;
        }
    }

    return trace;
}

}   // namespace

std::vector<std::string> all_workload_names() {
    return {
        "mostly_adds",
        "many_cancels",
        "market_orders",
        "deep_book",
        "wide_price_range",
        "same_price_fifo",
        "cross_heavy",
        "modify_heavy",
        "mixed_realistic"
    };
}

std::vector<BenchRequest> generate_workload(const WorkloadConfig& config) {
    if (config.name == "mostly_adds") {
        return mostly_adds(config);
    }

    if (config.name == "many_cancels") {
        return many_cancels(config);
    }

    if (config.name == "market_orders") {
        return market_orders(config);
    }

    if (config.name == "deep_book") {
        return deep_book(config);
    }

    if (config.name == "wide_price_range") {
        return wide_price_range(config);
    }

    if (config.name == "same_price_fifo") {
        return same_price_fifo(config);
    }

    if (config.name == "cross_heavy") {
        return cross_heavy(config);
    }

    if (config.name == "modify_heavy") {
        return modify_heavy(config);
    }

    if (config.name == "mixed_realistic") {
        return mixed_realistic(config);
    }

    throw std::invalid_argument("unknown workload: " + config.name);
}
