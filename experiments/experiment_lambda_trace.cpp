
#include "ssd/ftl.hpp"
#include "ssd/benchmark.hpp"
#include "ssd/workload.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include <algorithm>
#include <optional>

std::optional<std::size_t> firstDifferentGcEvent(
    const ssd::FTL& a,
    const ssd::FTL& b
) {
    const auto& eventsA = a.gcEvents();
    const auto& eventsB = b.gcEvents();

    const auto common_size = std::min(
        eventsA.size(),
        eventsB.size()
    );

    for (std::size_t i = 0; i < common_size; ++i) {
        const auto& x = eventsA[i];
        const auto& y = eventsB[i];

        if(x.victim_block != y.victim_block){
            return i;
        }

        if(x.migrated_pages != y.migrated_pages){
            return i;
        }

        if(x.invalid_pages != y.invalid_pages){
            return i;
        }
    }

    if(eventsA.size() != eventsB.size()){
        return common_size;
    }

    return std::nullopt;
}


bool sameGcTrace(
    const ssd::FTL& a,
    const ssd::FTL& b
) {
    const auto& eventsA = a.gcEvents();
    const auto& eventsB = b.gcEvents();

    if(eventsA.size() != eventsB.size()){
        return false;
    }

    for (std::size_t i = 0; i < eventsA.size(); ++i) {
        const auto& x = eventsA[i];
        const auto& y = eventsB[i];

        if(x.victim_block != y.victim_block){
            return false;
        }

        if(x.migrated_pages != y.migrated_pages){
            return false;
        }

        if(x.invalid_pages != y.invalid_pages){
            return false;
        }
    }

    return true;
}


int main() {

    ssd::WorkloadConfig config{};

    config.kind = ssd::WorkloadKind::Random;
    config.operations = 10000;
    config.logical_page_count = 480;
    config.seed = 42;
    config.hot_set_size = 40;
    config.hot_probability = 0.8;

    auto writes = ssd::generateWorkload(config);

    ssd::FTL wear01{
        32, 16,
        ssd::GcPolicy::WearAwareGreedy,
        0.01
    };

    ssd::FTL wear10{
        32, 16,
        ssd::GcPolicy::WearAwareGreedy,
        0.10
    };

    ssd::FTL wear20{
        32, 16,
        ssd::GcPolicy::WearAwareGreedy,
        0.20
    };

    auto result01 = ssd::runBenchmark(wear01, writes);
    auto result10 = ssd::runBenchmark(wear10, writes);
    auto result20 = ssd::runBenchmark(wear20, writes);

    std::optional<std::size_t> comp110 = firstDifferentGcEvent(wear01, wear10);

    std::optional<std::size_t> comp120 = firstDifferentGcEvent(wear01, wear20);

    if (comp110) {
        std::cout
            << "0.01 vs 0.10 first difference: "
            << comp110.value()
            << '\n';
    } else {
        std::cout
            << "0.01 vs 0.10: SAME\n";
    }
    if (comp120) {
        std::cout
            << "0.01 vs 0.20 first difference: "
            << comp120.value()
            << '\n';
    } else {
        std::cout
            << "0.01 vs 0.20: SAME\n";
    }

    auto diff = firstDifferentGcEvent(wear01, wear10);

    if (diff.has_value() &&
        *diff < wear01.gcEvents().size() &&
        *diff < wear10.gcEvents().size()) {

        const auto& eventA = wear01.gcEvents()[*diff];
        const auto& eventB = wear10.gcEvents()[*diff];

        std::cout
            << "Lambda 0.01 Victim: "
            << eventA.victim_block << '\n';

        std::cout
            << "Lambda 0.10 Victim: "
            << eventB.victim_block << '\n';

        std::cout
            << "Migrated Pages: "
            << eventA.migrated_pages
            << " vs "
            << eventB.migrated_pages << '\n';
    }

    auto diff2 = firstDifferentGcEvent(wear01, wear20);

    if (diff2.has_value() &&
        *diff2 < wear01.gcEvents().size() &&
        *diff2 < wear20.gcEvents().size()) {

        const auto& eventA = wear01.gcEvents()[*diff2];
        const auto& eventB = wear20.gcEvents()[*diff2];

        std::cout
            << "Lambda 0.01 Victim: "
            << eventA.victim_block << '\n';

        std::cout
            << "Lambda 0.20 Victim: "
            << eventB.victim_block << '\n';

        std::cout
            << "Migrated Pages: "
            << eventA.migrated_pages
            << " vs "
            << eventB.migrated_pages << '\n';
    }
    return 0;
}
