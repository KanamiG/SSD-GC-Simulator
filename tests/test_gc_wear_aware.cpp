#include "ssd/ftl.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error(
            "Wear-aware FTL test failed"
        );
    }
}

int main() {

    ssd::FTL greedy{
        4, 3, ssd::GcPolicy::Greedy
    };

    ssd::FTL wear0{
        4, 3,
        ssd::GcPolicy::WearAwareGreedy,
        0.0
    };

    constexpr std::array<ssd::LogicalPageId, 9> writes{
        10, 11, 12,
        20, 21, 22,
        10, 20, 21
    };

    for (auto lpn : writes) {
        check(greedy.write(lpn));
        check(wear0.write(lpn));
    }

    std::optional<std::size_t>greedyVictim = greedy.selectVictimBlock();
    std::optional<std::size_t>wearAwareGreedyVictim = wear0.selectVictimBlock();

    check(greedyVictim.has_value());
    check(wearAwareGreedyVictim.has_value());


    check(*greedyVictim == 1);
    check(*wearAwareGreedyVictim == 1);

    check(greedy.garbageCollectOne());
    check(wear0.garbageCollectOne());

    check(greedy.gcEvents().size() == 1);
    check(wear0.gcEvents().size() == 1);

    const auto& greedyEvents = greedy.gcEvents();
    const auto& greedyEvent = greedyEvents.front();
    const auto& wearAwareGreedyEvents = wear0.gcEvents();
    const auto& wearAwareGreedyEvent = wearAwareGreedyEvents.front();
    check(greedyEvent.victim_block == wearAwareGreedyEvent.victim_block);
    check(greedyEvent.migrated_pages == wearAwareGreedyEvent.migrated_pages);

    ssd::FTL wear1{
        4, 3,
        ssd::GcPolicy::WearAwareGreedy,
        0.05
    };
    for (auto lpn : writes) {
        check(wear1.write(lpn));
    }
    std::optional<std::size_t>wearAwareVictim = wear1.selectVictimBlock();
    check(wearAwareVictim.has_value());
    check(*wearAwareVictim == 1);
    check(wear1.garbageCollectOne());
    const auto& wearAwareEvents = wear1.gcEvents();
    check(wearAwareEvents.size() == 1);
    const auto& wearAwareEvent = wearAwareEvents.front();
    check(wearAwareEvent.victim_block == 1);
    check(wearAwareEvent.migrated_pages == 1);
    std::cout
        << "[PASS] Wear-aware FTL integration\n";
}