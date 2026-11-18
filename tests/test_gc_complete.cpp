
#include "ssd/ftl.hpp"

#include <iostream>
#include <stdexcept>

void require(bool condition) {
    if (!condition) {
        throw std::runtime_error(
            "Complete GC test failed"
        );
    }
}

int main() {
    // Case 1: Migration + Erase
    {
        ssd::FTL ftl{2, 3};

        require(ftl.write(10));
        require(ftl.write(20));
        require(ftl.write(30));
        require(ftl.write(10));
        require(ftl.write(20));

        auto oldPpa = ftl.lookup(30);

        require(ftl.garbageCollectOne());

        const auto& victim =
            ftl.flash().block(0);

        require(victim.freeCount() == 3);
        require(victim.validCount() == 0);
        require(victim.invalidCount() == 0);
        require(victim.eraseCount() == 1);

        require(ftl.mappedCount() == 3);
        require(ftl.lookup(30) != oldPpa);

        // 必須能再次寫入
        require(ftl.write(40));
        require(ftl.lookup(40) ==
            (ssd::PhysicalPageAddress{0, 0}));
    }

    // Case 2: Victim 全部 Invalid
    {
        ssd::FTL ftl{2, 2};

        require(ftl.write(10));
        require(ftl.write(20));
        require(ftl.write(10));
        require(ftl.write(20));

        // 不需要 Migration，直接 Erase
        require(ftl.garbageCollectOne());

        require(
            ftl.flash().block(0).freeCount() == 2
        );

        require(
            ftl.flash().block(0).eraseCount() == 1
        );

        require(ftl.write(30));
    }

    // Case 3: 沒有合適的 Victim
    {
        ssd::FTL ftl{2, 3};

        require(ftl.write(10));
        require(!ftl.garbageCollectOne());

        require(
            ftl.flash().block(0).validCount() == 1
        );

        require(
            ftl.flash().block(0).eraseCount() == 0
        );
    }

    std::cout << "[PASS] Complete GC\n";
}
