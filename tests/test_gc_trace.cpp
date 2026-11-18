
#include "ssd/ftl.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error(
            "GC Trace test failed"
        );
    }
}

void testGcTrace(ssd::GcPolicy policy) {

    ssd::FTL ftl{4, 3, policy};

    constexpr std::array<ssd::LogicalPageId, 9> writes{
        10, 11, 12,
        20, 21, 22,
        10, 20, 21
    };

    for (auto lpn : writes) {
        check(ftl.write(lpn));
    }

    // 還沒有執行 Garbage Collection
    check(ftl.gcEvents().empty());

    // 手動執行一次 GC
    check(ftl.garbageCollectOne());

    const auto& events = ftl.gcEvents();

    check(events.size() == 1);

    const auto& event = events.front();

    check(event.gc_cycle == 1);

    check(event.migrated_pages == event.valid_pages);


    check(event.valid_pages + event.invalid_pages == 3);


    check(event.free_pages_outside >= event.valid_pages);


    if(policy == ssd::GcPolicy::Greedy){
        check(event.victim_block == 1);
        check(event.valid_pages == 1);
        check(event.invalid_pages == 2);
    }
    else if(policy == ssd::GcPolicy::FIFO){
        check(event.victim_block == 0);
        check(event.valid_pages == 2);
        check(event.invalid_pages == 1);
    }


    check(
        ftl.flash()
            .block(event.victim_block)
            .freeCount() == 3
    );
}

int main() {

    testGcTrace(ssd::GcPolicy::Greedy);
    testGcTrace(ssd::GcPolicy::FIFO);

    std::cout << "[PASS] GC Event Tracing\n";
}
