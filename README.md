# Low-Latency Exchange Engine

> **Project status:** This project is actively in progress. The current version implements a working single-symbol matching engine, multiple order book storage designs, correctness checks, and a reusable benchmark framework. More exchange features, performance optimizations, tests, and system components will be added incrementally.

## Overview

This project is a C++20 low-latency exchange matching engine simulator. It is designed to model the core behavior of an exchange-side limit order book, including order acceptance, matching, cancellation, modification, trade generation, book updates, invariant checking, and performance benchmarking.

The goal is to build the system step by step, starting from a clean and correct implementation and then improving the data structures and hot path performance.

Current focus:

- Correct price-time priority matching
- Deterministic order processing
- Low allocation overhead
- Benchmark-driven optimization
- Comparing multiple order book implementations under identical workloads

## Current Features

### Order Types

The engine currently supports:

- Limit orders
- Market orders
- Cancel requests
- Modify requests

Limit orders can rest in the book if they are not fully matched. Market orders execute immediately against available liquidity and any unfilled remaining quantity is cancelled.

### Matching Rules

The engine follows standard price-time priority:

- Buy orders match against the lowest available ask price.
- Sell orders match against the highest available bid price.
- A limit buy matches while `best_ask <= buy_limit_price`.
- A limit sell matches while `best_bid >= sell_limit_price`.
- Market orders match until fully filled or until the opposite side is empty.
- Trade price is the price of the resting order.
- Orders at the same price level are processed in FIFO order.

FIFO means **First-In, First-Out**. In this context, the order that arrived earlier at the same price level gets matched first.

### Event Output

Each submitted request produces one or more events.

Current event types include:

- `OrderAccepted`
- `OrderRejected`
- `OrderCancelled`
- `OrderModified`
- `OrderRested`
- `Trade`
- `UnfilledMarketOrderCancelled`

The event model is useful for debugging, replay, benchmarking, and eventually market-data style output.

### Invariant Checks

The engine includes invariant checks to verify internal correctness.

Examples of checked conditions:

- No zero-quantity resting orders
- Orders have the correct side
- Orders are stored at the correct price level
- Price-level total quantity matches the sum of resting orders
- Lookup table entries point to valid orders
- Bid and ask book are not crossed
- Book size and lookup size remain consistent

These checks are useful while changing data structures and optimizing the hot path.

## Implementations

The project currently has two order book implementations.

### 1. Map-Based Order Book

The baseline implementation uses:

```cpp
std::map<Price, PriceLevel, std::greater<Price>> bids;
std::map<Price, PriceLevel> asks;
std::list<Order> orders_per_price_level;
std::unordered_map<OrderId, OrderLocation> order_lookup;
```

This version is simple and correctness-focused.

Strengths:

- Easy to understand
- Easy to debug
- Naturally ordered price levels
- Good baseline for correctness comparison

Weaknesses:

- `std::map` uses tree nodes and pointer chasing
- `std::list` allocates one node per order
- More dynamic allocation
- Lower cache locality

### 2. Ladder + Object Pool Order Book

The optimized implementation uses:

```cpp
std::vector<PriceLevel> bid_levels;
std::vector<PriceLevel> ask_levels;
std::vector<OrderNode> node_pool;
std::unordered_map<OrderId, NodeIndex> order_lookup;
```

This version replaces map/list storage with:

- Fixed price ladder
- Object pool
- Intrusive linked list using node indices
- Cached best bid and best ask indices

An intrusive linked list means the linked-list pointers are stored inside the order node itself. This avoids separate `std::list` node allocation for every resting order.

Strengths:

- Better cache locality
- Fewer allocations
- Faster price-level access
- Better benchmark performance on most workloads

Tradeoff:

- Best-price refresh can require scanning empty price levels after a level is depleted.
- The fixed ladder requires a configured price range.

## Performance Work Completed

### Version 1 / 2: Baseline Map Implementation

Implemented a correct map-based limit order book with:

- New limit orders
- Market orders
- Cancel
- Modify as cancel + new
- Trade generation
- Best bid / best ask queries
- Invariant checking
- Debug printing

### Version 3: Removed Easy Hot-Path Overhead

Optimizations added to the map implementation:

- Reserved `std::unordered_map` capacity
- Reserved event vector capacity
- Reused a single event vector through request handlers
- Removed string allocation from hot-path rejection reasons
- Avoided unnecessary copies
- Compiled benchmark target with `-O3` and `-DNDEBUG`

Result: Version 3 significantly reduced allocation count and improved throughput while preserving the same logical event counts.

### Version 4: Faster Storage

Added a second implementation using:

- Price ladder
- Object pool
- Intrusive linked list
- Node-index based order lookup
- Cached best bid and best ask levels

This reduced allocations further and improved throughput on most benchmark workloads.

## Benchmarking

The project includes a reusable benchmark framework under the `benchmark/` directory.

The benchmark system generates deterministic workloads using a fixed seed and replays the same request trace against each implementation. This makes comparisons fair between implementations.

### Current Benchmark Workloads

Current workloads include:

- `mostly_adds`
- `many_cancels`
- `market_orders`
- `deep_book`
- `wide_price_range`
- `same_price_fifo`
- `cross_heavy`
- `modify_heavy`
- `mixed_realistic`

### Metrics Collected

The benchmark records:

- Total operations
- Total runtime in nanoseconds
- Throughput in operations per second
- Minimum latency
- Mean latency
- p50 latency
- p90 latency
- p99 latency
- Maximum latency
- Allocation count
- Allocation bytes
- Event counts: trades, accepted, rejected, cancelled, modified, rested

p50, p90, and p99 mean percentile latencies. For example, p99 = 500 ns means 99% of measured operations completed within 500 nanoseconds.

### Latest Benchmark Snapshot

Benchmark configuration:

- operations: 10,000,000 per workload
- seed: 1
- build: Release / `-O3`
- implementation: `ladder_pool`

| Workload | Throughput | Mean Latency | p50 | p99 | Allocations |
|---|---|---|---|---|---|
| mostly_adds | 7.88M ops/sec | 114.122 ns | 42 ns | 667 ns | 19.01M |
| many_cancels | 10.79M ops/sec | 78.791 ns | 42 ns | 375 ns | 15.05M |
| market_orders | 10.53M ops/sec | 82.332 ns | 42 ns | 583 ns | 12.66M |
| deep_book | 16.66M ops/sec | 46.922 ns | 42 ns | 84 ns | 20.00M |
| wide_price_range | 14.44M ops/sec | 56.101 ns | 42 ns | 84 ns | 20.00M |
| same_price_fifo | 17.97M ops/sec | 42.592 ns | 42 ns | 84 ns | 15.00M |
| cross_heavy | 8.41M ops/sec | 106.019 ns | 42 ns | 625 ns | 15.64M |
| modify_heavy | 5.69M ops/sec | 162.852 ns | 83 ns | 625 ns | 16.56M |
| mixed_realistic | 8.96M ops/sec | 98.599 ns | 42 ns | 500 ns | 14.93M |

The ladder-pool implementation preserves the same logical event counts as the map implementation while reducing allocations and improving average throughput across most workloads.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run Demo

```bash
./build/exchange_engine
```

## Run Benchmarks

Run one workload:

```bash
./build/exchange_benchmark \
  --book_type map \
  --workload mostly_adds \
  --orders 10000000 \
  --seed 1
```

Run all workloads for the map implementation:

```bash
./build/exchange_benchmark \
  --book_type map \
  --all-workloads \
  --orders 10000000 \
  --seed 1 \
  --csv results/results_map.csv
```

Run all workloads for the ladder-pool implementation:

```bash
./build/exchange_benchmark \
  --book_type ladder_pool \
  --all-workloads \
  --orders 10000000 \
  --seed 1 \
  --csv results/results_ladder_pool.csv
```

## Project Structure

```
.
├── benchmark/
│   ├── allocation_counter.cpp
│   ├── allocation_counter.hpp
│   ├── benchmark_event_sink.hpp
│   ├── benchmark_main.cpp
│   ├── benchmark_runner.cpp
│   ├── benchmark_runner.hpp
│   ├── benchmark_types.hpp
│   ├── order_book_adapter.hpp
│   ├── workload_generator.cpp
│   └── workload_generator.hpp
│
├── include/
│   ├── event.hpp
│   ├── order.hpp
│   ├── map_order_book.hpp
│   ├── ladder_pool_order_book.hpp
│   └── types.hpp
│
├── src/
│   ├── main.cpp
│   ├── order_book.cpp
│   └── order_book_ladder_pool.cpp
│
├── results/
├── CMakeLists.txt
└── README.md
```

## Current Design Notes

### Why Start with std::map?

The map-based version is easier to reason about and debug. It provides a correctness baseline before introducing lower-level storage optimizations.

### Why Add a Price Ladder?

A price ladder maps valid prices directly to array indices. This avoids tree lookup overhead for price levels and improves locality.

### Why Add an Object Pool?

The object pool stores order nodes in a vector and reuses freed slots. This reduces dynamic allocation compared with allocating a separate list node for every resting order.

### Why Use an Intrusive List?

Each `OrderNode` stores its own `prev` and `next` indices. This gives FIFO behavior per price level without using `std::list`.

## Planned Work

Possible next steps:
V
- Add stronger unit tests for both implementations
- Add benchmark validation mode with periodic invariant checks
- Add side-by-side correctness comparison between map and ladder-pool outputs
- Add event sink API to avoid returning `std::vector<Event>` per request
- Add multi-symbol matching engine wrapper
- Add replay from recorded request logs
- Add market data publisher style output
- Add persistent logging
- Add more realistic workload models
- Optimize best-price refresh in ladder-pool implementation
- Explore custom allocator or flat hash map for order lookup

## Summary

This project currently implements a working exchange-side matching engine with two storage designs:

- A correctness-focused `std::map` + `std::list` order book
- A faster price ladder + object pool + intrusive list order book

The benchmark framework allows both implementations to be tested on the same deterministic workloads. So far, the ladder-pool implementation reduces allocations and improves throughput on most workloads while preserving the same logical behavior as the map implementation.
