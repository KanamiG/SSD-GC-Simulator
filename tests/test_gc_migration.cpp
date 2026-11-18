#include "ssd/ftl.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("GC migration test failed");
    }
}

} // namespace

int main() {
    ssd::FTL ftl{3, 3};

    require(ftl.write(10));
    require(ftl.write(20));
    require(ftl.write(30));

    // Two pages in block 0 become invalid; LPN 30 remains valid.
    require(ftl.write(10));
    require(ftl.write(20));

    const auto victim = ftl.selectVictimBlock();
    require(victim && *victim == 0);

    const auto old_ppa = ftl.lookup(30);
    require(old_ppa.has_value());

    // Verify migration through the public, complete GC operation.
    require(ftl.garbageCollectOne());

    const auto new_ppa = ftl.lookup(30);
    require(new_ppa.has_value());
    require(new_ppa != old_ppa);
    require(new_ppa->block != 0);

    const auto& new_page = ftl.flash().block(new_ppa->block)
                               .page(new_ppa->page);
    require(new_page.state == ssd::PageState::Valid);
    require(new_page.lpn == 30);
    require(ftl.mappedCount() == 3);

    // Mappings for all other live logical pages remain valid.
    for (const ssd::LogicalPageId lpn : {10ULL, 20ULL}) {
        const auto addr = ftl.lookup(lpn);
        require(addr.has_value());
        const auto& page = ftl.flash().block(addr->block).page(addr->page);
        require(page.state == ssd::PageState::Valid);
        require(page.lpn == lpn);
    }

    const auto& erased_victim = ftl.flash().block(0);
    require(erased_victim.freeCount() == 3);
    require(erased_victim.validCount() == 0);
    require(erased_victim.invalidCount() == 0);
    require(erased_victim.eraseCount() == 1);

    require(ftl.metrics().host_writes == 5);
    require(ftl.metrics().gc_page_writes == 1);
    require(ftl.metrics().gc_cycles == 1);

    require(ftl.gcEvents().size() == 1);
    const auto& event = ftl.gcEvents().front();
    require(event.gc_cycle == 1);
    require(event.victim_block == 0);
    require(event.valid_pages == 1);
    require(event.invalid_pages == 2);
    require(event.migrated_pages == 1);

    std::cout << "[PASS] GC migration / complete GC API\n";
}
