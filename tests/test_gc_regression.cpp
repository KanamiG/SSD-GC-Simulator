#include "ssd/ftl.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool ok, const std::string& context) {
    if (!ok) {
        throw std::runtime_error("GC regression: " + context);
    }
}

// Verify both directions of the mapping, not just aggregate page counts.
void verifyState(const ssd::FTL& ftl,
                 const std::vector<bool>& written,
                 std::size_t expected_host_writes) {
    const auto& flash = ftl.flash();
    std::vector<bool> physical_seen(flash.totalPages(), false);
    std::size_t physical_valid = 0;
    std::size_t physical_free = 0;
    std::uint64_t erase_sum = 0;

    for (std::size_t b = 0; b < flash.blockCount(); ++b) {
        const auto& block = flash.block(b);
        require(block.freeCount() + block.validCount() +
                    block.invalidCount() == flash.pagesPerBlock(),
                "block page accounting");
        erase_sum += block.eraseCount();

        for (std::size_t p = 0; p < flash.pagesPerBlock(); ++p) {
            const auto& page = block.page(p);
            const ssd::PhysicalPageAddress addr{b, p};
            if (page.state == ssd::PageState::Valid) {
                ++physical_valid;
                require(page.lpn.has_value(), "valid page without LPN");
                require(*page.lpn < written.size() && written[*page.lpn],
                        "unexpected valid LPN");
                require(ftl.lookup(*page.lpn) == addr,
                        "reverse page-to-mapping inconsistency");
            } else if (page.state == ssd::PageState::Free) {
                ++physical_free;
                require(!page.lpn.has_value(), "free page retains LPN");
            }
        }
    }

    std::size_t expected_mapped = 0;
    for (std::size_t lpn = 0; lpn < written.size(); ++lpn) {
        const auto addr = ftl.lookup(static_cast<ssd::LogicalPageId>(lpn));
        require(addr.has_value() == written[lpn], "missing/unexpected mapping");
        if (!addr) {
            continue;
        }
        ++expected_mapped;
        const std::size_t ppn = flash.toPpn(*addr);
        require(!physical_seen.at(ppn), "two LPNs map to the same PPN");
        physical_seen[ppn] = true;
        const auto& page = flash.block(addr->block).page(addr->page);
        require(page.state == ssd::PageState::Valid && page.lpn == lpn,
                "mapping-to-page inconsistency");
    }

    require(physical_valid == expected_mapped, "duplicate valid versions");
    require(ftl.mappedCount() == expected_mapped, "mapped count mismatch");
    require(ftl.freePageCount() == physical_free, "free count mismatch");
    require(ftl.metrics().host_writes == expected_host_writes,
            "host write count mismatch");
    require(ftl.gcEvents().size() == ftl.metrics().gc_cycles,
            "GC event count mismatch");
    require(erase_sum == ftl.totalEraseCount() &&
                erase_sum == ftl.metrics().gc_cycles,
            "erase count mismatch");
    require(flash.physicalProgramCount() ==
                ftl.metrics().host_writes + ftl.metrics().gc_page_writes,
            "physical program count mismatch");
    const auto waf = ftl.writeAmplification();
    if (expected_host_writes == 0) {
        require(!waf.has_value(), "WAF should be absent without host writes");
    } else {
        require(waf.has_value() && std::isfinite(*waf) && *waf >= 1.0,
                "invalid WAF");
    }
}

void stressManagedGc(ssd::GcPolicy policy, double lambda,
                     bool hot_cold, std::uint64_t seed) {
    constexpr std::size_t block_count = 12;
    constexpr std::size_t pages_per_block = 8;
    constexpr std::size_t logical_pages = 72;
    constexpr std::size_t operations = 2500;

    ssd::FTL ftl{block_count, pages_per_block, policy, lambda};
    std::vector<bool> written(logical_pages, false);
    std::uint64_t random_state = seed;
    std::uint64_t recorded_migrations = 0;
    std::size_t previous_event_count = 0;

    verifyState(ftl, written, 0);
    for (std::size_t step = 0; step < operations; ++step) {
        // An explicit generator keeps this test independent of the
        // standard library's distribution implementation.
        random_state = random_state * 6364136223846793005ULL +
                       1442695040888963407ULL;
        const auto random_value = random_state >> 24;
        const std::size_t lpn =
            hot_cold && (step % 10 < 8)
                ? static_cast<std::size_t>(random_value % 8)
                : static_cast<std::size_t>(random_value % logical_pages);

        require(ftl.writeManaged(static_cast<ssd::LogicalPageId>(lpn)),
                "managed write failed at step " + std::to_string(step));
        written[lpn] = true;

        const auto& events = ftl.gcEvents();
        require(events.size() >= previous_event_count,
                "GC event count went backwards");
        for (std::size_t i = previous_event_count; i < events.size(); ++i) {
            const auto& event = events[i];
            require(event.gc_cycle == i + 1, "GC cycle numbering");
            require(event.victim_block < block_count,
                    "GC victim out of range");
            require(event.valid_pages + event.invalid_pages ==
                        pages_per_block,
                    "GC victim was not closed");
            require(event.invalid_pages > 0, "GC collected clean block");
            require(event.migrated_pages == event.valid_pages,
                    "incomplete migration in GC event");
            require(event.free_pages_outside >= event.valid_pages,
                    "insufficient migration space");
            recorded_migrations += event.migrated_pages;
        }
        previous_event_count = events.size();
        require(recorded_migrations == ftl.metrics().gc_page_writes,
                "migration metric/event mismatch");

        if ((step + 1) % 50 == 0) {
            verifyState(ftl, written, step + 1);
        }
    }
    verifyState(ftl, written, operations);
    require(ftl.metrics().gc_cycles > 0,
            "stress workload did not exercise GC");
}

void checkManagedCapacityRejection() {
    ssd::FTL ftl{3, 4}; // Capacity 8, physical pages 12.
    std::vector<bool> written(9, false);
    for (std::size_t lpn = 0; lpn < 8; ++lpn) {
        require(ftl.writeManaged(lpn), "initial capacity fill");
        written[lpn] = true;
    }
    verifyState(ftl, written, 8);
    const auto metrics_before = ftl.metrics();
    const auto free_before = ftl.freePageCount();
    require(!ftl.writeManaged(8), "new LPN should exceed logical capacity");
    require(!ftl.lookup(8).has_value(), "failed write created mapping");
    require(ftl.metrics().host_writes == metrics_before.host_writes &&
                ftl.metrics().gc_cycles == metrics_before.gc_cycles &&
                ftl.freePageCount() == free_before,
            "capacity rejection mutated the device");
    verifyState(ftl, written, 8);
    require(ftl.writeManaged(0), "overwrite at capacity should succeed");
    verifyState(ftl, written, 9);
}

void checkNoGcVictim() {
    ssd::FTL ftl{2, 3};
    std::vector<bool> written(6, false);
    for (std::size_t lpn = 0; lpn < written.size(); ++lpn) {
        require(ftl.write(lpn), "low-level fill failed");
        written[lpn] = true;
    }
    verifyState(ftl, written, 6);
    require(!ftl.selectVictimBlock().has_value(),
            "fully valid flash unexpectedly has a GC victim");
    require(!ftl.garbageCollectOne(), "GC succeeded without invalid pages");
    require(!ftl.writeManaged(0), "write succeeded on fully valid flash");
    verifyState(ftl, written, 6);
}

} // namespace

int main() {
    for (const auto policy : {ssd::GcPolicy::Greedy,
                              ssd::GcPolicy::FIFO,
                              ssd::GcPolicy::WearAwareGreedy}) {
        const double lambda =
            policy == ssd::GcPolicy::WearAwareGreedy ? 0.05 : 0.0;
        for (const bool hot_cold : {false, true}) {
            for (const std::uint64_t seed : {7ULL, 53ULL}) {
                stressManagedGc(policy, lambda, hot_cold, seed);
            }
        }
    }
    checkManagedCapacityRejection();
    checkNoGcVictim();
    std::cout << "[PASS] GC regression: 12 sustained-workload cases, "
                 "capacity rejection, no-victim safety\n";
}
