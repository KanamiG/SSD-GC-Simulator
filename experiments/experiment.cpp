#include "ssd/benchmark.hpp"
#include "ssd/csv_export.hpp"
#include "ssd/workload.hpp"

#include <array>
#include <fstream>
#include <iostream>

int main() {

    constexpr std::size_t block_count = 32;
    constexpr std::size_t pages_per_block = 16;

    constexpr std::size_t operations = 10000;
    constexpr std::size_t logical_pages = 200;

    constexpr std::uint64_t seed = 42;

    const std::array workloads{
        ssd::WorkloadKind::Sequential,
        ssd::WorkloadKind::Random,
        ssd::WorkloadKind::HotCold
    };

    std::ofstream csv{"benchmark_results.csv"};

    if (!csv) {
        std::cerr << "Failed to open CSV\n";
        return 1;
    }

    ssd::writeCsvHeader(csv);

    for (auto kind : workloads) {

        ssd::WorkloadConfig config{};

        config.kind = kind;
        config.operations = operations;
        config.logical_page_count = logical_pages;
        config.seed = seed;
        config.hot_set_size = 40;
        config.hot_probability = 0.8;

        auto workload = ssd::generateWorkload(config);

        ssd::FTL ftl{block_count, pages_per_block};

        auto result = ssd::runBenchmark(ftl, workload);

        if (result.attempted_writes !=
            result.successful_writes + result.failed_writes ||
            result.host_writes != result.successful_writes) {

            std::cerr << "Benchmark metrics mismatch\n";
            return 1;
        }
        

        ssd::writeCsvRow(csv, config, block_count, pages_per_block, result);

        if (result.waf) {
            std::cout << "WAF: " << result.waf.value() << '\n';
        } else {
            std::cout << "WAF: N/A\n";
        }
        if (result.success_rate) {
            std::cout << "Success%: " << result.success_rate.value() * 100 << '%' << '\n';
        } else {
            std::cout << "Success%: N/A\n";
        }
    }

    csv.close();

    if (!csv) {
        std::cerr << "Failed to finalize CSV\n";
        return 1;
    }


    return 0;
}