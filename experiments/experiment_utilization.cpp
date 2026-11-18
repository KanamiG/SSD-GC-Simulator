
#include "ssd/ftl.hpp"
#include "ssd/benchmark.hpp"
#include "ssd/csv_export.hpp"
#include "ssd/workload.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string_view>

int main() {

    constexpr std::size_t block_count = 32;
    constexpr std::size_t pages_per_block = 16;
    constexpr std::size_t operations = 10000;

    constexpr std::size_t logical_capacity =
        (block_count - 1) * pages_per_block;

    constexpr std::array<std::size_t, 4> logical_counts{
        100, 200, 350, 480
    };

    constexpr std::array<std::uint64_t, 5> seeds{
        42, 43, 44, 45, 46
    };

    constexpr std::array kinds{
        ssd::WorkloadKind::Random,
        ssd::WorkloadKind::HotCold
    };

    constexpr std::array policies{
        ssd::GcPolicy::Greedy,
        ssd::GcPolicy::FIFO
    };

    std::ofstream csv{"utilization_comparison.csv"};

    if (!csv) {
        std::cerr << "Cannot open CSV\n";
        return 1;
    }

    csv << std::setprecision(10);

    // 在既有 CSV 格式前面新增兩欄
    csv << "policy,utilization,";
    ssd::writeCsvHeader(csv);

    for (auto logical_pages : logical_counts) {

        const double utilization =
            static_cast<double>(logical_pages) /
            logical_capacity;

        for (auto kind : kinds) {
            for (auto seed : seeds) {

                ssd::WorkloadConfig config{};

                config.kind = kind;
                config.operations = operations;
                config.logical_page_count = logical_pages;
                config.seed = seed;
                config.hot_set_size = 40;
                config.hot_probability = 0.8;

                auto workload = ssd::generateWorkload(config);

                for (auto policy : policies) {

                    ssd::FTL ftl{block_count, pages_per_block, policy};
                    auto result = ssd::runBenchmark(ftl, workload);
                    if (result.attempted_writes !=
                        result.successful_writes + result.failed_writes ||
                        result.host_writes != result.successful_writes) {

                        std::cerr << "Benchmark metrics mismatch\n";
                        return 1;
                    }

                    std::string_view policy_name =
                    policy == ssd::GcPolicy::Greedy
                        ? "Greedy"
                        : "FIFO";
                    
                    csv << policy_name << ',' << utilization << ',';
                    ssd::writeCsvRow(csv, config, block_count, pages_per_block, result);

                    std::cout   << "Kind: "
                                << static_cast<int>(kind)
                                << " | Seed: " << seed
                                << " | Policy: " << policy_name
                                << " | WAF: ";
                    if (result.waf) {
                        std::cout << result.waf.value();
                    } else {
                        std::cout << "N/A";
                    }

                    std::cout << " | Success Rate: ";
                    if (result.success_rate) {
                        std::cout << *result.success_rate * 100 << "%\n";
                    } else {
                        std::cout << "N/A\n";
                    }

                }
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
