
#include "ssd/ftl.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error(
            "Wear effect test failed"
        );
    }
}

int main() {

    ssd::FTL greedy{
        4, 3, ssd::GcPolicy::Greedy
    };

    ssd::FTL wear0{
        4, 3,
        ssd::GcPolicy::WearAwareGreedy,
        0.0
    };

    ssd::FTL wear05{
        4, 3,
        ssd::GcPolicy::WearAwareGreedy,
        0.05
    };

    constexpr std::array<ssd::LogicalPageId, 16> writes{
        3, 3, 0, 2,
        4, 3, 3, 2,
        3, 2, 4, 1,
        4, 1, 2, 1
    };

    for (auto lpn : writes) {
        check(greedy.writeManaged(lpn));
        check(wear0.writeManaged(lpn));
        check(wear05.writeManaged(lpn));
    }


    check(wear05.flash().block(0).validCount() == 1);
    check(wear05.flash().block(0).invalidCount() == 2);
    check(wear05.flash().block(0).eraseCount() == 1);

    check(wear05.flash().block(3).validCount() == 1);
    check(wear05.flash().block(3).invalidCount() == 2);
    check(wear05.flash().block(3).eraseCount() == 0);


    std::optional<std::size_t> greedyVictim = 
        greedy.selectVictimBlock();
    std::optional<std::size_t> wear0Victim = 
        wear0.selectVictimBlock();
    std::optional<std::size_t> wear05Victim = 
        wear05.selectVictimBlock();   
    check(greedyVictim.has_value());
    check(wear0Victim.has_value());
    check(wear05Victim.has_value());


    check(greedyVictim.value() == 0);
    check(wear0Victim.value() == 0);
    check(wear05Victim.value() == 3);


    std::cout
        << "[PASS] Wear-aware changes victim selection\n";
}
