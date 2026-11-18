
#include "ssd/ftl.hpp"
#include "ssd/benchmark.hpp"
#include "ssd/workload.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string_view>

int main() {

    constexpr std::size_t block_count = 32;
    constexpr std::size_t pages_per_block = 16;
    constexpr std::size_t operations = 10000;
    constexpr std::uint64_t seed = 42;

    constexpr std::array logical_counts{
        200, 480
    };

    constexpr std::array kinds{
        ssd::WorkloadKind::Random,
        ssd::WorkloadKind::HotCold
    };

    constexpr std::array policies{
        ssd::GcPolicy::Greedy,
        ssd::GcPolicy::FIFO
    };

    std::ofstream csv{"gc_event_trace.csv"};

    if (!csv) {
        std::cerr << "Failed to open CSV\n";
        return 1;
    }

    csv << "workload,logical_pages,policy,seed,"
           "gc_cycle,victim_block,valid_pages,"
           "invalid_pages,free_pages_outside,"
           "migrated_pages,closed_epoch\n";

    for (auto logical_pages : logical_counts) {
        for (auto kind : kinds) {

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

                const auto& events = ftl.gcEvents();

                std::string_view workload_name =
                    kind == ssd::WorkloadKind::Random
                        ? "Random"
                        : "HotCold";
                std::string_view policy_name =
                    policy == ssd::GcPolicy::Greedy
                        ? "Greedy"
                        : "FIFO";

                if (events.size() != result.gc_cycles){
                    std::cerr << "Events size do not match gc cycles\n";
                    return 1;
                }

                std::uint64_t total_migrated = 0;

                for(const auto& event : events){
                    
                    if(event.migrated_pages != event.valid_pages) {
                        std::cerr << "Migration count mismatch\n";
                        return 1;
                    }

                    total_migrated += event.migrated_pages;

                    csv << workload_name << ','
                        << config.logical_page_count << ','
                        << policy_name << ','
                        << config.seed << ','
                        << event.gc_cycle << ','
                        << event.victim_block << ','
                        << event.valid_pages << ','
                        << event.invalid_pages << ','
                        << event.free_pages_outside << ','
                        << event.migrated_pages << ','
                        << event.closed_epoch << '\n';
                    
                }

                if (total_migrated != result.gc_page_writes) {
                        std::cerr << "GC Trace mismatch\n";
                        return 1;
                }


                std::cout   << "Workload: " << workload_name
                            << " | Logical Pages: " << config.logical_page_count
                            << " | Policy: " << policy_name
                            << " | WAF: ";
                if (result.waf) {
                    std::cout << result.waf.value();
                } else {
                    std::cout << "N/A";
                }
                std::cout   << " | GC Cycles: " << result.gc_cycles
                            << " | Success Rate: ";
                if (result.success_rate) {
                    std::cout << *result.success_rate * 100 << "%\n";
                } else {
                    std::cout << "N/A\n";
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
