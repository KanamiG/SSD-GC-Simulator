
#include "ssd/ftl.hpp"

#include <cstddef>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <string>
#include <iostream>

void verifyInvariants(
    const ssd::FTL& ftl,
    const std::vector<ssd::LogicalPageId>& lpns
) {
    const auto& flash = ftl.flash();

    std::size_t total_valid = 0;

    for(std::size_t b = 0; b < flash.blockCount(); ++b) {
        total_valid += flash.block(b).validCount();
    }

    for(std::size_t b = 0; b < flash.blockCount(); ++b) {
        const auto& block = flash.block(b);
        std::size_t sum = block.freeCount() + block.validCount() + block.invalidCount();
        if(sum != flash.pagesPerBlock()) {
            throw std::runtime_error("Invariant violation: Block counts do not sum to pagesPerBlock");
        }
    }

    std::unordered_set<std::size_t> used_ppns;

    for(const auto& lpn : lpns) {
        auto ppa = ftl.lookup(lpn);
        if(ppa.has_value()) {

            std::size_t ppn = flash.toPpn(*ppa);
            if(!used_ppns.insert(ppn).second) {
                throw std::runtime_error("Invariant violation: Duplicate PPN for LPN " + std::to_string(lpn));
            }
            const auto& page = flash.block(ppa->block).page(ppa->page);
            if(page.state != ssd::PageState::Valid || page.lpn != lpn) {
                throw std::runtime_error("Invariant violation: Page state or LPN mismatch for LPN " + std::to_string(lpn));
            }
        }
    }

    

    

    if(total_valid != ftl.mappedCount()) {
        throw std::runtime_error("Invariant violation: total_valid does not match mappedCount");
    }
    if(used_ppns.size() != ftl.mappedCount()) {
        throw std::runtime_error("Invariant violation: used_ppns size does not match mappedCount");
    }
}


void check(bool condition) {
    if (!condition) {
        throw std::runtime_error(
            "FTL invariant test failed"
        );
    }
}


void testFailedWriteRollback() {
    ssd::FTL ftl{1, 1};

    // 唯一的 Physical Page 已被使用
    if (!ftl.write(10)) {
        throw std::runtime_error(
            "Initial write failed"
        );
    }

    auto oldPpa = ftl.lookup(10);

    // 嘗試寫入新的 LPN
    // 此時 Flash 已滿
    if (ftl.write(20)) {
        throw std::runtime_error(
            "Write should have failed"
        );
    }

    // 不可留下暫時 Mapping
    if (ftl.lookup(20).has_value()) {
        throw std::runtime_error(
            "Temporary mapping was not removed"
        );
    }

    if (ftl.mappedCount() != 1) {
        throw std::runtime_error(
            "Incorrect mapping count"
        );
    }

    if (ftl.lookup(10) != oldPpa) {
        throw std::runtime_error(
            "Original mapping changed"
        );
    }

    verifyInvariants(ftl, {10});
}


int main() {
    ssd::FTL ftl{1, 4};

    // Case 1: Initial state
    verifyInvariants(ftl, {});
    std::cout << "[PASS] Initial state\n";

    // Case 2: First write
    check(ftl.write(10));
    verifyInvariants(ftl, {10});
    std::cout << "[PASS] First write\n";

    // Case 3: Second write
    check(ftl.write(20));
    verifyInvariants(ftl, {10, 20});
    std::cout << "[PASS] Second write\n";

    // Case 4: Out-of-place update
    check(ftl.write(10));
    verifyInvariants(ftl, {10, 20});
    std::cout << "[PASS] Out-of-place update\n";

    // Case 5: Fill remaining space
    check(ftl.write(30));
    verifyInvariants(ftl, {10, 20, 30});
    std::cout << "[PASS] Fill flash\n";

    // Case 6: Flash full
    auto oldPpa = ftl.lookup(10);

    check(!ftl.write(10));
    check(ftl.lookup(10) == oldPpa);

    testFailedWriteRollback();
    std::cout << "[PASS] Failed write rollback\n";

    verifyInvariants(ftl, {10, 20, 30});
    std::cout << "[PASS] Flash full consistency\n";

    std::cout
        << "All FTL invariant tests passed!\n";

    return 0;
}
