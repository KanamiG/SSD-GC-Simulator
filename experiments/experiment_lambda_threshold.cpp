#include "ssd/ftl.hpp"
#include "ssd/workload.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>

int main() {
    constexpr std::array<double, 3> weights{0.100, 0.200, 0.201};
    constexpr std::size_t target_index = 454;  // GC #455

    ssd::WorkloadConfig config{};
    config.kind = ssd::WorkloadKind::Random;
    config.operations = 10000;
    config.logical_page_count = 480;
    config.seed = 42;
    config.hot_set_size = 40;
    config.hot_probability = 0.8;

    const auto writes = ssd::generateWorkload(config);
    std::cout << std::fixed << std::setprecision(6);

    for (const double lambda : weights) {
        ssd::FTL ftl{32, 16, ssd::GcPolicy::WearAwareGreedy, lambda};
        std::size_t host_writes = 0;
        bool found = false;

        for (const auto lpn : writes) {
            const auto before = ftl.gcEvents().size();
            const auto& block18 = ftl.flash().block(18);
            const auto& block29 = ftl.flash().block(29);

            // Snapshot values before the host write can trigger GC.
            const auto valid18 = static_cast<std::int64_t>(block18.validCount());
            const auto valid29 = static_cast<std::int64_t>(block29.validCount());
            const auto erase18 = static_cast<std::int64_t>(block18.eraseCount());
            const auto erase29 = static_cast<std::int64_t>(block29.eraseCount());

            if (!ftl.writeManaged(lpn)) {
                throw std::runtime_error("Host write failed");
            }
            ++host_writes;
            const auto after = ftl.gcEvents().size();
            if (before <= target_index && target_index < after) {
                const auto& event = ftl.gcEvents().at(target_index);
                const auto deltaV = valid29 - valid18;
                const auto deltaE = erase18 - erase29;

                std::cout << "GC #455 Snapshot\n"
                          << "Lambda: " << lambda << '\n'
                          << "Host Write: " << host_writes << '\n'
                          << "Block 18 Valid: " << valid18 << '\n'
                          << "Block 29 Valid: " << valid29 << '\n'
                          << "Block 18 Erases: " << erase18 << '\n'
                          << "Block 29 Erases: " << erase29 << '\n'
                          << "Delta E (18-29): " << deltaE << '\n'
                          << "Victim: " << event.victim_block << '\n';
                if (deltaE != 0) {
                    std::cout << "Lambda*: "
                              << static_cast<double>(deltaV) /
                                     static_cast<double>(deltaE)
                              << '\n';
                } else {
                    std::cout << "Lambda*: undefined (Delta E = 0)\n";
                }
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::runtime_error("GC #455 not reached");
        }
    }
}
