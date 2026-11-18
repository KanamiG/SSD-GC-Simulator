
#include "ssd/flash.hpp"

#include <iostream>
#include <stdexcept>

int main() {
    ssd::Flash flash{2, 3};

    for (std::size_t i = 0; i < 6; ++i) {
        auto result = flash.program(i + 100);

        if (!result.has_value()) {
            throw std::runtime_error(
                "Unexpected write failure"
            );
        }

        const ssd::PhysicalPageAddress expected{
            i / 3,
            i % 3
        };

        if (*result != expected) {
            throw std::runtime_error(
                "Unexpected physical address"
            );
        }
    }

    if (flash.program(999).has_value()) {
        throw std::runtime_error(
            "Write should fail when flash is full"
        );
    }

    if (flash.block(0).validCount() != 3 ||
        flash.block(1).validCount() != 3) {
        throw std::runtime_error(
            "Incorrect valid page counts"
        );
    }

    std::cout << "[PASS] flash programming\n";
}
