#pragma once

#include <string>
#include <vector>

#include "benchmark_types.hpp"

std::vector<std::string> all_workload_names();

std::vector<BenchRequest> generate_workload(const WorkloadConfig& config);
