
#include "ssd/benchmark.hpp"
#include "ssd/csv_export.hpp"
#include "ssd/workload.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main() {

    constexpr std::size_t block_count = 32;
    constexpr std::size_t pages_per_block = 16;

    constexpr std::size_t operations = 10000;
    constexpr std::size_t logical_pages = 200;

    constexpr std::array<std::uint64_t, 5> seeds{
        42, 43, 44, 45, 46
    };

    constexpr std::array kinds{
        ssd::WorkloadKind::Sequential,
        ssd::WorkloadKind::Random,
        ssd::WorkloadKind::HotCold
    };

    std::ofstream csv{
        "benchmark_results_multiseed.csv"
    };

    if (!csv) {
        throw std::runtime_error(
            "Cannot open benchmark CSV"
        );
    }

    ssd::writeCsvHeader(csv);

    for (auto kind : kinds){
        for (auto seed : seeds){
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
            std::cout   << "Kind: "
                        << static_cast<int>(kind)
                        << " | Seed: " << seed
                        << " | WAF: ";
            if (result.waf) {
                std::cout << result.waf.value() << '\n';
            } else {
                std::cout << "N/A\n";
            }
            if (result.success_rate) {
                std::cout << "Success%: " << result.success_rate.value() * 100 << '%' << '\n';
            } else {
                std::cout << "Success%: N/A\n";
            }

            
        }
    }
    csv.close();

    if (!csv) {
        std::cerr << "Failed to finalize CSV\n";
        return 1;
    }

    

    return 0;
}
