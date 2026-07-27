#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <cstdint>

#include "benchmark_runner.hpp"
#include "allocation_counter.hpp"
#include "benchmark_event_sink.hpp"

namespace {

using Clock = std::chrono::steady_clock;

std::uint64_t ns_between(Clock::time_point start, Clock::time_point end) {
    return static_cast<std::uint64_t> (
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
    );
}

std::uint64_t percentile(const std::vector<std::uint64_t>& sorted_values, double p) {
    if (sorted_values.empty()) {
        return 0;
    }

    std::size_t idx =
        static_cast<std::size_t>(p * static_cast<double>(sorted_values.size() - 1));

    return sorted_values[idx];
}

void warmup(IBenchmarkBook& book, const std::vector<BenchRequest>& trace) {
    BenchmarkEventSink sink;
    book.reset();

    std::size_t warmup_ops = std::min<std::size_t>(trace.size(), 50'000);

    for (std::size_t i = 0; i < warmup_ops; ++i) {
        book.process(trace[i], sink);
    }
}

}   // namespace

BenchmarkResult run_benchmark(
    IBenchmarkBook& book,
    const std::vector<BenchRequest>& trace,
    const std::string& workload_name
) {
    if (trace.empty()) {
        throw std::invalid_argument("benchmark trace is empty");
    }

    warmup(book, trace);

    BenchmarkEventSink sink;

    std::vector<std::uint64_t> latencies;
    latencies.resize(trace.size());

    book.reset();
    sink.reset();

    // Count allocations only during actual book processing.
    // Trace generation, latency vector allocation, and warmup are excluded.
    g_alloc_stats.reset();

    auto total_start = Clock::now();

    for (std::size_t i = 0; i < trace.size(); ++i) {
        auto op_start = Clock::now();

        book.process(trace[i], sink);

        auto op_end = Clock::now();

        latencies[i] = ns_between(op_start, op_end);
    }

    auto total_end = Clock::now();

    std::uint64_t total_ns = ns_between(total_start, total_end);

    std::uint64_t alloc_count =
        g_alloc_stats.alloc_count.load(std::memory_order_relaxed);
    std::uint64_t alloc_bytes =
        g_alloc_stats.alloc_bytes.load(std::memory_order_relaxed);

    std::sort(latencies.begin(), latencies.end());

    BenchmarkResult result;

    result.impl_name = book.name();
    result.workload_name = workload_name;

    result.operations = static_cast<std::uint64_t>(trace.size());
    result.total_ns = total_ns;

    result.throughput_ops_per_sec =
        static_cast<double>(result.operations) /
        (static_cast<double>(total_ns) / 1'000'000'000.0);

    result.p50_ns = percentile(latencies, 0.50);
    result.p90_ns = percentile(latencies, 0.90);
    result.p99_ns = percentile(latencies, 0.99);
    result.max_ns = latencies.back();

    result.alloc_count = alloc_count;
    result.alloc_bytes = alloc_bytes;

    result.trades = sink.trades;
    result.accepted = sink.accepted;
    result.rejected = sink.rejected;
    result.cancelled = sink.cancelled;
    result.modified = sink.modified;
    result.rested = sink.rested;

    return result;
}

void print_human_readable(const BenchmarkResult& result) {
    double throughput_mops =
        result.throughput_ops_per_sec / 1'000'000.0;

    std::cout << "impl: " << result.impl_name << '\n';
    std::cout << "workload: " << result.workload_name << '\n';
    std::cout << "ops: " << result.operations << '\n';
    std::cout << "total_ns: " << result.total_ns << '\n';

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "throughput: " << throughput_mops << " M ops/sec\n";

    std::cout << "p50_ns: " << result.p50_ns << '\n';
    std::cout << "p90_ns: " << result.p90_ns << '\n';
    std::cout << "p99_ns: " << result.p99_ns << '\n';
    std::cout << "max_ns: " << result.max_ns << '\n';

    std::cout << "alloc_count: " << result.alloc_count << '\n';
    std::cout << "alloc_bytes: " << result.alloc_bytes << '\n';

    std::cout << "trades: " << result.trades << '\n';
    std::cout << "accepted: " << result.accepted << '\n';
    std::cout << "rejected: " << result.rejected << '\n';
    std::cout << "cancelled: " << result.cancelled << '\n';
    std::cout << "modified: " << result.modified << '\n';
    std::cout << "rested: " << result.rested << '\n';
}

void write_csv_header(std::ostream& os) {
    os
        << "impl,"
        << "workload,"
        << "ops,"
        << "total_ns,"
        << "throughput_ops_sec,"
        << "p50_ns,"
        << "p90_ns,"
        << "p99_ns,"
        << "max_ns,"
        << "alloc_count,"
        << "alloc_bytes,"
        << "trades,"
        << "accepted,"
        << "rejected,"
        << "cancelled,"
        << "modified,"
        << "rested"
        << '\n';
}

void write_csv_row(std::ostream& os, const BenchmarkResult& result) {
    os
        << result.impl_name << ','
        << result.workload_name << ','
        << result.operations << ','
        << result.total_ns << ','
        << std::fixed << std::setprecision(3)
        << result.throughput_ops_per_sec << ','
        << result.p50_ns << ','
        << result.p90_ns << ','
        << result.p99_ns << ','
        << result.max_ns << ','
        << result.alloc_count << ','
        << result.alloc_bytes << ','
        << result.trades << ','
        << result.accepted << ','
        << result.rejected << ','
        << result.cancelled << ','
        << result.modified << ','
        << result.rested
        << '\n';
}
