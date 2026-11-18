
#include "ssd/workload.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error(
            "Workload generator test failed"
        );
    }
}

int main() {
    // Case 1: Sequential
    {
        ssd::WorkloadConfig config{
            .kind = ssd::WorkloadKind::Sequential,
            .operations = 10,
            .logical_page_count = 4
        };

        auto workload = ssd::generateWorkload(config);

        std::vector<ssd::LogicalPageId> expected{
            0, 1, 2, 3, 0, 1, 2, 3, 0, 1
        };

        check(workload == expected);
    }

    // Case 2: Random Reproducibility
    {
        ssd::WorkloadConfig config{
            .kind = ssd::WorkloadKind::Random,
            .operations = 1000,
            .logical_page_count = 8,
            .seed = 12345
        };

        auto first = ssd::generateWorkload(config);
        auto second = ssd::generateWorkload(config);

        check(first == second);
        check(first.size() == 1000);

        for (auto lpn : first) {
            check(lpn < 8);
        }
    }

    // Case 3: Hot/Cold Ratio
    {
        ssd::WorkloadConfig config{
            .kind = ssd::WorkloadKind::HotCold,
            .operations = 10000,
            .logical_page_count = 10,
            .seed = 42,
            .hot_set_size = 2,
            .hot_probability = 0.8
        };

        auto workload = ssd::generateWorkload(config);

        std::size_t hot_writes = 0;

        for (auto lpn : workload) {
            check(lpn < 10);

            if (lpn < 2) {
                ++hot_writes;
            }
        }

        const double ratio =
            static_cast<double>(hot_writes) /
            workload.size();

        check(ratio > 0.75);
        check(ratio < 0.85);
    }

    // Case 4: Invalid Configuration
    {
        ssd::WorkloadConfig config{
            .kind = ssd::WorkloadKind::Random,
            .operations = 10,
            .logical_page_count = 0
        };

        bool thrown = false;

        try {
            (void)ssd::generateWorkload(config);
        } catch (const std::invalid_argument&) {
            thrown = true;
        }

        check(thrown);
    }

    std::cout
        << "[PASS] Workload Generator\n";
}
