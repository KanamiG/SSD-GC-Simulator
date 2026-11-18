
#include "ssd/gc_policy.hpp"

#include <iostream>
#include <stdexcept>
#include <cmath>
#include <limits>

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error(
            "Wear-aware score test failed"
        );
    }
}

int main() {

    // Block 0:
    // Valid = 2, Erases = 120
    //
    // Block 1:
    // Valid = 4, Erases = 20

    const double score0 =
        ssd::wearAwareScore(2, 120, 0.05);

    const double score1 =
        ssd::wearAwareScore(4, 20, 0.05);

    check(std::abs(score0 - 8.0) < 1e-9);
    check(std::abs(score1 - 5.0) < 1e-9);

    check(score1 < score0);
    

    auto greedy0 =
    ssd::wearAwareScore(2, 120, 0.0);

    auto greedy1 =
    ssd::wearAwareScore(4, 20, 0.0);

    check(greedy0 < greedy1);

    bool caught = false;
    try {
        ssd::wearAwareScore(2, 120, -0.05);
    }
    catch(const std::invalid_argument& e){
        caught = true;
    }
    check(caught);

    const double nan =
    std::numeric_limits<double>::quiet_NaN();

    caught = false;
    try {
        ssd::wearAwareScore(2, 120, nan);
    }
    catch(const std::invalid_argument& e){
        caught = true;
    }
    check(caught);

    const double inf =
        std::numeric_limits<double>::infinity();

    caught = false;
    try {
        ssd::wearAwareScore(2, 120, inf);
    }
    catch(const std::invalid_argument& e){
        caught = true;
    }
    check(caught);

    std::cout
        << "[PASS] Wear-aware score\n";
}
