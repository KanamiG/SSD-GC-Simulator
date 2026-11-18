
#include "ssd/ftl.hpp"

#include <iostream>
#include <stdexcept>

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error("FTL test failed");
    }
}

int main() {
    ssd::FTL ftl{2, 3};

    // 1. 初始狀態
    check(ftl.mappedCount() == 0);
    check(!ftl.lookup(100).has_value());

    // 2. 第一次寫入
    check(ftl.write(100));

    auto ppa = ftl.lookup(100);

    check(ppa.has_value());
    check(*ppa == (
        ssd::PhysicalPageAddress{0, 0}
    ));

    check(ftl.mappedCount() == 1);

    // 3. 第二筆寫入
    check(ftl.write(200));

    auto second = ftl.lookup(200);

    check(second.has_value());
    check(*second == (
        ssd::PhysicalPageAddress{0, 1}
    ));

    check(ftl.mappedCount() == 2);

    /* 4. 原本的 Mapping 不應改變
    check(ftl.lookup(100) == ppa);

    // 5. 本階段暫不支援重複 LPN
    check(!ftl.write(100));
    check(ftl.mappedCount() == 2);
    

    // 6. Flash 實際狀態
    check(ftl.flash().block(0).validCount() == 2);
    */

    std::cout << "[PASS] FTL basic mapping\n";
}
