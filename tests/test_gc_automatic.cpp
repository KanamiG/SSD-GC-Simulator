
#include "ssd/ftl.hpp"

#include <iostream>
#include <stdexcept>

void require(bool condition) {
    if (!condition) {
        throw std::runtime_error(
            "Automatic GC test failed"
        );
    }
}


void testNoVictimButFreePages() {
    ssd::FTL ftl{2, 3};

    require(ftl.writeManaged(10));
    require(ftl.writeManaged(20));
    require(ftl.writeManaged(30));

    // Logical Capacity = 3
    // Free Pages = 3
    // 沒有 Invalid Pages，所以 GC 找不到 Victim

    require(ftl.freePageCount() == 3);
    require(!ftl.selectVictimBlock().has_value());

    // 更新既有 LPN，不增加 Logical Capacity
    require(ftl.writeManaged(10));

    require(ftl.mappedCount() == 3);
    require(ftl.freePageCount() == 2);

    require(ftl.flash().block(0).invalidCount() == 1);
}


int main() {
    ssd::FTL ftl{3, 3};

    require(ftl.logicalCapacity() == 6);
    require(ftl.freePageCount() == 9);

    require(ftl.writeManaged(10));
    require(ftl.writeManaged(20));
    require(ftl.writeManaged(30));

    // 更新造成 Invalid Pages
    require(ftl.writeManaged(10));
    require(ftl.writeManaged(20));

    require(ftl.writeManaged(40));

    // 此時剩下 3 個 Free Pages
    require(ftl.freePageCount() == 3);

    auto oldPpa = ftl.lookup(30);

    // 應觸發一次 GC
    require(ftl.writeManaged(50));

    require(ftl.flash().block(0).eraseCount() == 1);
    require(ftl.lookup(30) != oldPpa);
    require(ftl.mappedCount() == 5);
    require(ftl.freePageCount() == 4);

    testNoVictimButFreePages();

    std::cout << "[PASS] Automatic GC\n";
}