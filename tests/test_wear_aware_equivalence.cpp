
#include "ssd/ftl.hpp"

#include <cstddef>
#include <iostream>
#include <random>
#include <stdexcept>

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error(
            "Wear-aware equivalence test failed"
        );
    }
}

int main() {

    ssd::FTL greedy{
        32, 16, ssd::GcPolicy::Greedy
    };

    ssd::FTL wear0{
        32, 16,
        ssd::GcPolicy::WearAwareGreedy,
        0.0
    };

    std::mt19937_64 rng(42);

    std::uniform_int_distribution<int> dist(
        0, 479
    );

    for (int i = 0; i < 10000; ++i) {

        ssd::LogicalPageId lpn = dist(rng);

        bool success_greedy =
            greedy.writeManaged(lpn);

        bool success_wear0 =
            wear0.writeManaged(lpn);

        check(greedy.lookup(lpn) == wear0.lookup(lpn));

        check(success_greedy and success_wear0);
    }

    const auto& events_greedy =
        greedy.gcEvents();

    const auto& events_wear0 =
        wear0.gcEvents();


    check(events_greedy.size() == events_wear0.size());

    for(std::size_t i = 0; i < events_greedy.size(); i++){
        const auto& event_greedy = events_greedy[i];
        const auto& event_wear0 = events_wear0[i];

        check(event_greedy.victim_block == event_wear0.victim_block);
        check(event_greedy.migrated_pages == event_wear0.migrated_pages);
        check(event_greedy.invalid_pages == event_wear0.invalid_pages);
    }

    check(greedy.totalEraseCount() == wear0.totalEraseCount());

    for (std::size_t b = 0; b < 32; ++b) {
        auto erases_greedy =
            greedy.flash().block(b).eraseCount();

        auto erases_wear0 =
            wear0.flash().block(b).eraseCount();
        
        check(erases_greedy == erases_wear0);
    }


    std::cout
        << "[PASS] Lambda zero equivalence\n";
}
