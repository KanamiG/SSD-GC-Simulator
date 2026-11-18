#include "ssd/block.hpp"

#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

#define CHECK(expr) do { if (!(expr)) { throw std::runtime_error("CHECK failed: " #expr); } } while (false)

using ssd::Block;
using ssd::PageState;

void test_initial_state() {
    Block b{4};
    CHECK(b.capacity() == 4);
    CHECK(b.eraseCount() == 0);
    CHECK(b.freeCount() == 4);
    CHECK(b.validCount() == 0);
    CHECK(b.invalidCount() == 0);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(b.page(i).state == PageState::Free);
        CHECK(!b.page(i).lpn.has_value());
    }
}

void test_program_and_invalidate() {
    Block b{4};
    CHECK(b.program(100) == 0);
    CHECK(b.program(200) == 1);
    CHECK(b.page(0).state == PageState::Valid);
    CHECK(b.page(0).lpn == 100);
    CHECK(b.page(1).lpn == 200);
    CHECK(b.freeCount() == 2);
    CHECK(b.validCount() == 2);
    CHECK(b.invalidate(0));
    CHECK(!b.invalidate(0));
    CHECK(!b.invalidate(2)); // Free page cannot be invalidated
    CHECK(b.page(0).state == PageState::Invalid);
    CHECK(b.page(0).lpn == 100); // keep old LPN until erase
    CHECK(b.validCount() == 1);
    CHECK(b.invalidCount() == 1);
    CHECK(b.freeCount() == 2);
}

void test_invalid_does_not_become_free() {
    Block b{3};
    CHECK(b.program(1) == 0);
    CHECK(b.program(2) == 1);
    CHECK(b.program(3) == 2);
    CHECK(b.invalidate(1));
    CHECK(!b.program(4).has_value()); // full despite an Invalid page
    CHECK(b.freeCount() == 0);
    CHECK(b.validCount() == 2);
    CHECK(b.invalidCount() == 1);
}

void test_safe_erase_and_reuse() {
    Block b{3};
    CHECK(b.program(10) == 0);
    CHECK(b.program(20) == 1);
    CHECK(!b.erase()); // Cannot erase live pages in our teaching model
    CHECK(b.eraseCount() == 0);
    CHECK(b.validCount() == 2);
    CHECK(b.invalidate(0));
    CHECK(b.invalidate(1));
    CHECK(b.erase());
    CHECK(b.eraseCount() == 1);
    CHECK(b.freeCount() == 3);
    CHECK(b.validCount() == 0);
    CHECK(b.invalidCount() == 0);
    for (std::size_t i = 0; i < b.capacity(); ++i) {
        CHECK(b.page(i).state == PageState::Free);
        CHECK(!b.page(i).lpn.has_value());
    }
    CHECK(b.program(99) == 0); // cursor must be reset
}

void test_invalid_arguments() {
    bool thrown = false;
    try { Block bad{0}; } catch (const std::invalid_argument&) { thrown = true; }
    CHECK(thrown);

    Block b{2};
    thrown = false;
    try { (void)b.page(2); } catch (const std::out_of_range&) { thrown = true; }
    CHECK(thrown);
    thrown = false;
    try { (void)b.invalidate(2); } catch (const std::out_of_range&) { thrown = true; }
    CHECK(thrown);
}

int main() {
    using Test = std::pair<const char*, void(*)()>;
    const std::vector<Test> tests{
        {"initial state", test_initial_state},
        {"program and invalidate", test_program_and_invalidate},
        {"invalid is not free", test_invalid_does_not_become_free},
        {"safe erase and reuse", test_safe_erase_and_reuse},
        {"boundary cases", test_invalid_arguments},
    };
    std::size_t passed = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            ++passed;
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& ex) {
            std::cerr << "[FAIL] " << name << ": " << ex.what() << '\n';
        }
    }
    std::cout << "Passed " << passed << "/" << tests.size() << " tests\n";
    return passed == tests.size() ? 0 : 1;
}
