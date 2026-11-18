
#include "ssd/ftl.hpp"

#include <iostream>
#include <stdexcept>

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error("FTL update test failed");
    }
}

int main() {
    {
        ssd::FTL ftl{2, 3};

        check(ftl.write(100));
        check(ftl.write(200));

        auto old_ppa = ftl.lookup(100);
        check(old_ppa.has_value());

        // 更新同一個 LPN
        check(ftl.write(100));

        auto new_ppa = ftl.lookup(100);
        check(new_ppa.has_value());

        check(*new_ppa != *old_ppa);
        check(*new_ppa ==
              (ssd::PhysicalPageAddress{0, 2}));

        // 更新不增加 Mapping 數量
        check(ftl.mappedCount() == 2);

        // 舊 Page 必須變成 Invalid
        check(ftl.flash().block(0).invalidCount() == 1);
        check(ftl.flash().block(0).validCount() == 2);

        // 其他 LPN 不受影響
        check(ftl.lookup(200) ==
              (ssd::PhysicalPageAddress{0, 1}));

        // 再次更新，同一個 LPN 應可移到下一個 Block
        check(ftl.write(100));
        check(ftl.lookup(100) ==
              (ssd::PhysicalPageAddress{1, 0}));

        check(ftl.flash().block(0).invalidCount() == 2);
        check(ftl.mappedCount() == 2);
    }

    {
        // 測試空間不足時，舊資料必須保留
        ssd::FTL ftl{1, 2};

        check(ftl.write(10));
        check(ftl.write(20));

        auto old_ppa = ftl.lookup(10);

        check(!ftl.write(10));
        check(ftl.lookup(10) == old_ppa);

        check(ftl.flash().block(0).validCount() == 2);
        check(ftl.flash().block(0).invalidCount() == 0);
    }

    std::cout << "[PASS] FTL out-of-place update\n";
}
