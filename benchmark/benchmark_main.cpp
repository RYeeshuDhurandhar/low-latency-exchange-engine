#include "workload_generator.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <filesystem>

#include "benchmark_runner.hpp"
#include "benchmark_types.hpp"
#include "order_book_adapter.hpp"

namespace {
    
struct CliOptions {
    std::string impl = "map";
    std::string workload = "mostly_adds";
    bool all_workloads = false;

    std::uint64_t orders = 1'000'000;
    std::uint64_t seed = 1;

    Price mid_price = 10000;
    Price min_price = 9000;
    Price max_price = 11000;
    Price tick_size = 1;

    std::string csv_path;
};

void print_usage(const char* argv0) {
    std::cout
        << "Usage:\n"
        << "  " << argv0 << " [options]\n\n"
        << "Options:\n"
        << "  --impl <name>              Implementation: map\n"
        << "  --workload <name>          Workload name\n"
        << "  --all-workloads            Run all workloads\n"
        << "  --orders <N>               Number of operations\n"
        << "  --seed <N>                 RNG seed\n"
        << "  --mid-price <N>            Mid price in ticks\n"
        << "  --min-price <N>            Min price in ticks\n"
        << "  --max-price <N>            Max price in ticks\n"
        << "  --tick-size <N>            Tick size\n"
        << "  --csv <path>               Write CSV output\n"
        << "  --help                     Show help\n\n"
        << "Workloads:\n";

    for (const std::string& name : all_workload_names()) {
        std::cout << "  " << name << '\n';
    }
}

std::uint64_t parse_u64(const std::string& s) {
    return static_cast<std::uint64_t>(std::stoull(s));
}

Price parse_price(const std::string& s) {
    return static_cast<Price>(std::stoll(s));
}

CliOptions parse_cli(int argc, char** argv) {
    CliOptions options;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        auto need_value = [&](const std::string& name) -> std::string {
            if (i + 1 >= argc) {
                throw std::invalid_argument("missing value for " + name);
            }
            ++i;
            return argv[i];
        };

        if (arg == "--help") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (arg == "--impl") {
            options.impl = need_value(arg);
        } else if (arg == "--workload") {
            options.workload = need_value(arg);
        } else if (arg == "--all-workloads") {
            options.all_workloads = true;
        } else if (arg == "--orders") {
            options.orders = parse_u64(need_value(arg));
        } else if (arg == "--seed") {
            options.seed = parse_u64(need_value(arg));
        } else if (arg == "--mid-price") {
            options.mid_price = parse_price(need_value(arg));
        } else if (arg == "--min-price") {
            options.min_price = parse_price(need_value(arg));
        } else if (arg == "--max-price") {
            options.max_price = parse_price(need_value(arg));
        } else if (arg == "--tick-size") {
            options.tick_size = parse_price(need_value(arg));
        } else if (arg == "--csv") {
            options.csv_path = need_value(arg);
        } else {
            throw std::invalid_argument("unknown argument: " + arg);
        }
    }

    return options;
}

WorkloadConfig make_config(const CliOptions& options, const std::string& workload_name) {
    WorkloadConfig config;

    config.name = workload_name;
    config.num_ops = options.orders;
    config.seed = options.seed;

    config.symbol_id = 1;

    config.mid_price = options.mid_price;
    config.min_price = options.min_price;
    config.max_price = options.max_price;
    config.tick_size = options.tick_size;

    config.min_quantity = 1;
    config.max_quantity = 100;

    return config;
}

} // namespace

int main(int argc, char** argv) {
    try {
        CliOptions options = parse_cli(argc, argv);

        std::vector<std::string> workloads;

        if (options.all_workloads) {
            workloads = all_workload_names();
        } else {
            workloads.push_back(options.workload);
        }

        std::ofstream csv_file;

        if (!options.csv_path.empty()) {
            std::filesystem::path csv_path(options.csv_path);

            if (csv_path.has_parent_path()) {
                std::filesystem::create_directories(csv_path.parent_path());
            }

            csv_file.open(csv_path);
            if (!csv_file) {
                throw std::runtime_error("failed to open CSV file: " + options.csv_path);
            }

            write_csv_header(csv_file);
        }

        for (const std::string& workload_name : workloads) {
            WorkloadConfig config = make_config(options, workload_name);

            std::vector<BenchRequest> trace = generate_workload(config);

            std::unique_ptr<IBenchmarkBook> book = make_benchmark_book(options.impl);

            BenchmarkResult result = run_benchmark(*book, trace, workload_name);

            print_human_readable(result);
            std::cout << '\n';

            if (csv_file) {
                write_csv_row(csv_file, result);
            }
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "benchmark error: " << ex.what() << '\n';
        return 1;
    }
}
