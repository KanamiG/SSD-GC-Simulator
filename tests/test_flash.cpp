#include "ssd/flash.hpp"

#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            throw std::runtime_error(                                        \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) +    \
                ": CHECK failed: " #expr);                                  \
        }                                                                    \
    } while (false)

template <typename Expected, typename Action>
void checkThrows(Action&& action) {
    try {
        std::forward<Action>(action)();
    } catch (const Expected&) {
        return;
    } catch (const std::exception& ex) {
        throw std::runtime_error(
            std::string("Wrong exception type: ") + ex.what());
    }
    throw std::runtime_error("Expected exception was not thrown");
}

using ssd::Flash;
using ssd::PageState;
using ssd::PhysicalPageAddress;

void test_geometry() {
    const Flash flash{3, 4};
    CHECK(flash.blockCount() == 3);
    CHECK(flash.pagesPerBlock() == 4);
    CHECK(flash.totalPages() == 12);

    const Flash one{1, 1};
    CHECK(one.blockCount() == 1);
    CHECK(one.pagesPerBlock() == 1);
    CHECK(one.totalPages() == 1);
}

void test_initial_block_states() {
    const Flash flash{3, 4};
    for (std::size_t b = 0; b < flash.blockCount(); ++b) {
        const auto& block = flash.block(b);
        CHECK(block.capacity() == 4);
        CHECK(block.eraseCount() == 0);
        for (std::size_t p = 0; p < block.capacity(); ++p) {
            CHECK(block.page(p).state == PageState::Free);
            CHECK(!block.page(p).lpn.has_value());
        }
    }
}

void test_to_ppn() {
    const Flash flash{3, 4};
    CHECK(flash.toPpn({0, 0}) == 0);
    CHECK(flash.toPpn({0, 3}) == 3);
    CHECK(flash.toPpn({1, 0}) == 4);
    CHECK(flash.toPpn({1, 2}) == 6);
    CHECK(flash.toPpn({2, 3}) == 11);
}

void test_from_ppn() {
    const Flash flash{3, 4};
    const auto p0 = flash.fromPpn(0);
    CHECK(p0.block == 0 && p0.page == 0);
    const auto p6 = flash.fromPpn(6);
    CHECK(p6.block == 1 && p6.page == 2);
    const auto p11 = flash.fromPpn(11);
    CHECK(p11.block == 2 && p11.page == 3);
}

void test_round_trip_multiple_geometries() {
    for (const auto [blockCount, pagesPerBlock] :
         std::vector<std::pair<std::size_t, std::size_t>>{
             {1, 1}, {3, 4}, {5, 7}, {17, 13}}) {
        const Flash flash{blockCount, pagesPerBlock};
        for (std::size_t ppn = 0; ppn < flash.totalPages(); ++ppn) {
            CHECK(flash.toPpn(flash.fromPpn(ppn)) == ppn);
        }
        for (std::size_t b = 0; b < flash.blockCount(); ++b) {
            for (std::size_t p = 0; p < flash.pagesPerBlock(); ++p) {
                const PhysicalPageAddress address{b, p};
                CHECK(flash.fromPpn(flash.toPpn(address)) == address);
            }
        }
    }
}

void test_zero_geometry_rejected() {
    checkThrows<std::invalid_argument>([] { (void)Flash{0, 4}; });
    checkThrows<std::invalid_argument>([] { (void)Flash{3, 0}; });
    checkThrows<std::invalid_argument>([] { (void)Flash{0, 0}; });
}

void test_block_bounds() {
    const Flash flash{3, 4};
    CHECK(flash.block(0).capacity() == 4);
    CHECK(flash.block(2).capacity() == 4);
    checkThrows<std::out_of_range>([&] { (void)flash.block(3); });
    checkThrows<std::out_of_range>([&] {
        (void)flash.block(std::numeric_limits<std::size_t>::max());
    });
}

void test_to_ppn_bounds() {
    const Flash flash{3, 4};
    checkThrows<std::out_of_range>([&] { (void)flash.toPpn({3, 0}); });
    checkThrows<std::out_of_range>([&] { (void)flash.toPpn({0, 4}); });
    checkThrows<std::out_of_range>([&] { (void)flash.toPpn({3, 4}); });
    checkThrows<std::out_of_range>([&] {
        (void)flash.toPpn({std::numeric_limits<std::size_t>::max(), 0});
    });
}

void test_from_ppn_bounds() {
    const Flash flash{3, 4};
    CHECK(flash.fromPpn(11).block == 2);
    checkThrows<std::out_of_range>([&] { (void)flash.fromPpn(12); });
    checkThrows<std::out_of_range>([&] {
        (void)flash.fromPpn(std::numeric_limits<std::size_t>::max());
    });
}

void test_size_multiplication_overflow() {
    const auto max = std::numeric_limits<std::size_t>::max();
    // These must fail before reserve() or any massive allocation.
    checkThrows<std::invalid_argument>([&] { (void)Flash{max, 2}; });
    checkThrows<std::invalid_argument>([&] { (void)Flash{2, max}; });
}


// Empty/full transitions must not corrupt the physical program counter.
void test_program_counter_and_full_flash() {
    Flash flash{2, 2};
    CHECK(flash.physicalProgramCount() == 0);

    for (std::size_t n = 0; n < 4; ++n) {
        const auto ppa = flash.program(static_cast<ssd::LogicalPageId>(100 + n));
        CHECK(ppa.has_value());
        CHECK(*ppa == (PhysicalPageAddress{n / 2, n % 2}));
        CHECK(flash.physicalProgramCount() == n + 1);
    }

    CHECK(!flash.program(999).has_value());
    CHECK(flash.physicalProgramCount() == 4);
    CHECK(flash.block(0).validCount() == 2);
    CHECK(flash.block(1).validCount() == 2);

    CHECK(flash.invalidate({0, 0}));
    CHECK(flash.invalidate({0, 1}));
    CHECK(flash.eraseBlock(0));

    const auto reused = flash.program(200);
    CHECK(reused.has_value());
    CHECK(*reused == (PhysicalPageAddress{0, 0}));
    CHECK(flash.physicalProgramCount() == 5);
    CHECK(flash.block(0).eraseCount() == 1);
}

// FIFO closed epochs must be cleared on erase and monotonically reassigned
// when an erased block fills again.
void test_closed_epoch_lifecycle() {
    Flash flash{2, 2};
    CHECK(!flash.closedEpoch(0).has_value());
    CHECK(!flash.closedEpoch(1).has_value());

    CHECK(flash.program(10).has_value());
    CHECK(!flash.closedEpoch(0).has_value());
    CHECK(flash.program(11).has_value());
    CHECK(flash.closedEpoch(0) == std::optional<std::uint64_t>{1});

    CHECK(flash.program(12).has_value());
    CHECK(!flash.closedEpoch(1).has_value());
    CHECK(flash.program(13).has_value());
    CHECK(flash.closedEpoch(1) == std::optional<std::uint64_t>{2});

    CHECK(!flash.eraseBlock(0));
    CHECK(flash.closedEpoch(0) == std::optional<std::uint64_t>{1});

    CHECK(flash.invalidate({0, 0}));
    CHECK(flash.invalidate({0, 1}));
    CHECK(flash.eraseBlock(0));
    CHECK(!flash.closedEpoch(0).has_value());
    CHECK(flash.closedEpoch(1) == std::optional<std::uint64_t>{2});

    CHECK(flash.program(14).has_value());
    CHECK(!flash.closedEpoch(0).has_value());
    CHECK(flash.program(15).has_value());
    CHECK(flash.closedEpoch(0) == std::optional<std::uint64_t>{3});

    checkThrows<std::out_of_range>([&] { (void)flash.closedEpoch(2); });
}

// Failed operations must never silently corrupt existing block/page state.
void test_invalidate_and_erase_failure_guarantees() {
    Flash flash{1, 3};
    const auto ppa = flash.program(123);
    CHECK(ppa.has_value());

    checkThrows<std::out_of_range>([&] { (void)flash.invalidate({1, 0}); });
    checkThrows<std::out_of_range>([&] { (void)flash.invalidate({0, 3}); });
    CHECK(flash.block(0).validCount() == 1);
    CHECK(flash.block(0).freeCount() == 2);
    CHECK(flash.physicalProgramCount() == 1);

    CHECK(!flash.eraseBlock(0));
    CHECK(flash.block(0).eraseCount() == 0);
    CHECK(flash.block(0).page(0).state == PageState::Valid);

    CHECK(flash.invalidate(*ppa));
    CHECK(!flash.invalidate(*ppa));
    CHECK(!flash.invalidate({0, 1})); // Free page
    CHECK(flash.block(0).invalidCount() == 1);
    CHECK(flash.block(0).validCount() == 0);
    CHECK(flash.eraseBlock(0));
    CHECK(flash.block(0).freeCount() == 3);
    CHECK(flash.block(0).eraseCount() == 1);
    CHECK(flash.physicalProgramCount() == 1);
    CHECK(!flash.invalidate(*ppa)); // Erased page is now Free
    checkThrows<std::out_of_range>([&] { (void)flash.eraseBlock(1); });
}

int main() {
    using Test = std::pair<const char*, void (*)()>;
    const std::vector<Test> tests{
        {"geometry", test_geometry},
        {"initial block states", test_initial_block_states},
        {"toPpn known values", test_to_ppn},
        {"fromPpn known values", test_from_ppn},
        {"round-trip conversions", test_round_trip_multiple_geometries},
        {"zero geometry", test_zero_geometry_rejected},
        {"block bounds", test_block_bounds},
        {"toPpn bounds", test_to_ppn_bounds},
        {"fromPpn bounds", test_from_ppn_bounds},
        {"multiplication overflow", test_size_multiplication_overflow},
        {"physical program count / full flash", test_program_counter_and_full_flash},
        {"closed epoch lifecycle", test_closed_epoch_lifecycle},
        {"invalidate / erase failure guarantees", test_invalidate_and_erase_failure_guarantees},
    };

    std::size_t passed = 0;
    for (const auto& [name, run] : tests) {
        try {
            run();
            ++passed;
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& ex) {
            std::cerr << "[FAIL] " << name << ": " << ex.what() << '\n';
        } catch (...) {
            std::cerr << "[FAIL] " << name << ": unknown exception\n";
        }
    }
    std::cout << "Passed " << passed << "/" << tests.size() << " tests\n";
    return passed == tests.size() ? 0 : 1;
}
