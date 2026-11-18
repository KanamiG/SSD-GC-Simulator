#include "ssd/ftl.hpp"
#include "ssd/benchmark.hpp"
#include "ssd/workload.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct PolicyConfig {
    ssd::GcPolicy policy;
    double lambda;
    std::string name;
};

int main() {
    constexpr std::size_t block_count = 32;
    constexpr std::size_t pages_per_block = 16;
    constexpr std::size_t operations = 10'000;
    constexpr std::array<std::size_t, 4> logical_counts{
        400, 440, 464, 480
    };
    constexpr std::size_t logical_pages = 480;

    constexpr std::array<std::uint64_t, 5> seeds{42, 43, 44, 45, 46};
    constexpr std::array kinds{
        ssd::WorkloadKind::Random,
        ssd::WorkloadKind::HotCold
    };
    constexpr std::array<double, 6> weights{
        0.0, 0.01, 0.02, 0.05, 0.1, 0.2
    };

    std::vector<PolicyConfig> configs{
        {ssd::GcPolicy::Greedy, 0.0, "Greedy"},
        {ssd::GcPolicy::FIFO, 0.0, "FIFO"}
    };

    for (double lambda : weights) {
        configs.push_back({
            ssd::GcPolicy::WearAwareGreedy,
            lambda,
            "WearAwareGreedy"
        });
    }
    if (configs.size() != 8) {
        throw std::logic_error("Expected 8 policy configurations");
    }

    std::filesystem::create_directories("results");
    std::ofstream csv{"results/wear_weight_sensitivity.csv"};
    if (!csv) {
        throw std::runtime_error("Failed to open CSV");
    }
    csv << std::setprecision(10);
    csv << "workload,seed,policy,lambda,logical_pages,"
           "attempted,successful,success_rate,waf,"
           "gc_cycles,total_erases,max_erases,erase_cv\n";

    std::size_t run_count = 0;

    for (auto kind : kinds) {
        for (auto seed : seeds) {
            // One generated workload is reused by all eight policies.
            for (auto logical_pages : logical_counts) {
                ssd::WorkloadConfig workload_config{};
                workload_config.kind = kind;
                workload_config.operations = operations;
                workload_config.logical_page_count = logical_pages;
                workload_config.seed = seed;
                workload_config.hot_set_size = 40;
                workload_config.hot_probability = 0.8;

                const auto writes = ssd::generateWorkload(workload_config);
                if (writes.size() != operations) {
                    throw std::logic_error("Unexpected workload length");
                }

                const std::string_view workload_name =
                    kind == ssd::WorkloadKind::Random ? "Random" : "HotCold";

                // For validating WearAwareGreedy(lambda=0) against Greedy.
                std::vector<std::uint64_t> greedy_erases;
                std::size_t greedy_successful = 0;
                std::uint64_t greedy_gc_cycles = 0;
                std::uint64_t greedy_total_erases = 0;
                std::uint64_t greedy_gc_writes = 0;
                std::uint64_t greedy_physical_programs = 0;

                for (const auto& policy_config : configs) {
                    ssd::FTL ftl{
                        block_count,
                        pages_per_block,
                        policy_config.policy,
                        policy_config.lambda
                    };

                    // runBenchmark performs the writes: do not replay them.
                    const auto result = ssd::runBenchmark(ftl, writes);

                    if (result.attempted_writes != writes.size() ||
                        result.attempted_writes !=
                            result.successful_writes + result.failed_writes ||
                        result.host_writes != result.successful_writes) {
                        throw std::logic_error("Benchmark metrics mismatch");
                    }
                    if (ftl.gcEvents().size() != result.gc_cycles) {
                        throw std::logic_error("GC trace count mismatch");
                    }

                    const std::uint64_t total_erases = ftl.totalEraseCount();
                    if (total_erases != result.gc_cycles) {
                        throw std::logic_error("GC cycles / erase count mismatch");
                    }

                    std::vector<std::uint64_t> erase_counts;
                    erase_counts.reserve(block_count);
                    std::uint64_t max_erases = 0;
                    std::uint64_t erase_sum = 0;

                    for (std::size_t b = 0; b < block_count; ++b) {
                        const std::uint64_t count =
                            ftl.flash().block(b).eraseCount();
                        erase_counts.push_back(count);
                        erase_sum += count;
                        max_erases = std::max(max_erases, count);
                    }
                    if (erase_sum != total_erases) {
                        throw std::logic_error("Per-block erases do not sum to total");
                    }

                    const double mean_erases =
                        static_cast<double>(total_erases) / block_count;
                    double variance = 0.0;
                    for (std::uint64_t count : erase_counts) {
                        const double diff =
                            static_cast<double>(count) - mean_erases;
                        variance += diff * diff;
                    }
                    variance /= static_cast<double>(block_count);
                    const double std_erases = std::sqrt(variance);

                    // Check exact equivalence for lambda=0.
                    if (policy_config.policy == ssd::GcPolicy::Greedy) {
                        greedy_erases = erase_counts;
                        greedy_successful = result.successful_writes;
                        greedy_gc_cycles = result.gc_cycles;
                        greedy_total_erases = total_erases;
                        greedy_gc_writes = result.gc_page_writes;
                        greedy_physical_programs = result.physical_programs;
                    } else if (policy_config.policy == ssd::GcPolicy::WearAwareGreedy &&
                            policy_config.lambda == 0.0) {
                        if (erase_counts != greedy_erases ||
                            result.successful_writes != greedy_successful ||
                            result.gc_cycles != greedy_gc_cycles ||
                            total_erases != greedy_total_erases ||
                            result.gc_page_writes != greedy_gc_writes ||
                            result.physical_programs != greedy_physical_programs) {
                            throw std::logic_error("Lambda=0 differs from Greedy");
                        }
                    }

                    // One CSV row corresponds to one complete benchmark run.
                    csv << workload_name << ','
                        << seed << ','
                        << policy_config.name << ','
                        << policy_config.lambda << ','
                        << logical_pages << ','
                        << result.attempted_writes << ','
                        << result.successful_writes << ',';

                    if (result.success_rate) {
                        csv << *result.success_rate;
                    }
                    csv << ',';
                    if (result.waf) {
                        csv << *result.waf;
                    }
                    csv << ','
                        << result.gc_cycles << ','
                        << total_erases << ','
                        << max_erases << ',';

                    if (mean_erases > 0.0) {
                        csv << (std_erases / mean_erases);
                    }
                    csv << '\n';

                    if (!csv) {
                        throw std::runtime_error("Failed to write CSV row");
                    }
                    ++run_count;
                }
            }
        }
    }

    if (run_count != 320) {
        throw std::logic_error("Expected 320 benchmark runs");
    }
    csv.close();
    if (!csv) {
        throw std::runtime_error("Failed to finalize CSV");
    }

    std::cout << "[PASS] " << run_count
              << " benchmark runs saved to "
              << "results/wear_weight_sensitivity.csv\n";
    return 0;
}
