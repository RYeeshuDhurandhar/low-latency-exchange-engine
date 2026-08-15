#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "benchmark_types.hpp"
#include "benchmark_common.hpp"

BenchmarkResult run_benchmark(IBenchmarkBook& book, const std::vector<BenchRequest>& trace, const std::string& workload_name, BenchmarkLayer layer);

void print_human_readable(const BenchmarkResult& result);

void write_csv_header(std::ostream& os);
void write_csv_row(std::ostream& os, const BenchmarkResult& result);
