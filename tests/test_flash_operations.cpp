#include "ssd/flash.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            throw std::runtime_error( \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
                ": CHECK failed: " #expr); \
        } \
    } while (false)

int main() {
    ssd::Flash flash{2, 3};

    auto a = flash.program(100);
    auto b = flash.program(200);
    CHECK(a.has_value());
    CHECK(b.has_value());

    // Erasing a block with live data must fail without state changes.
    CHECK(!flash.eraseBlock(0));
    CHECK(flash.block(0).validCount() == 2);

    // Invalidating must execute in both Debug and Release builds.
    CHECK(flash.invalidate(*a));
    CHECK(flash.invalidate(*b));
    CHECK(!flash.invalidate(*a));
    CHECK(flash.block(0).invalidCount() == 2);

    // Erase and verify that all pages have become free.
    CHECK(flash.eraseBlock(0));
    CHECK(flash.block(0).freeCount() == 3);
    CHECK(flash.block(0).invalidCount() == 0);
    CHECK(flash.block(0).validCount() == 0);

    // Out-of-range addresses must throw.
    bool caught = false;
    try {
        (void)flash.invalidate({2, 0});
    } catch (const std::out_of_range&) {
        caught = true;
    }
    CHECK(caught);

    std::cout << "[PASS] Flash operations\n";
    return 0;
}
