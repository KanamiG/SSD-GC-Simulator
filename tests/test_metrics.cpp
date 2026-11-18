
#include "ssd/ftl.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error(
            "Metrics test failed"
        );
    }
}

void testMetricsConsistency() {

    ssd::FTL ftl{3, 3};

    for (auto lpn : {10ULL, 20ULL, 30ULL}) {
        check(ftl.write(lpn));
    }

    check(ftl.write(10));
    check(ftl.write(20));

    check(ftl.garbageCollectOne());

    const auto& m = ftl.metrics();

    check(m.host_writes == 5);
    check(m.gc_page_writes == 1);
    check(m.gc_cycles == 1);

    check(
        ftl.flash().physicalProgramCount() == 6
    );

    check(ftl.totalEraseCount() == 1);

    auto waf = ftl.writeAmplification();

    check(waf.has_value());
    check(std::abs(*waf - 1.2) < 1e-9);
}

int main() {

    // Case 1: 初始統計
    {
        ssd::FTL ftl{2, 3};

        check(ftl.metrics().host_writes == 0);
        check(ftl.metrics().gc_page_writes == 0);
        check(ftl.metrics().gc_cycles == 0);

        check(
            ftl.flash().physicalProgramCount() == 0
        );

        check(!ftl.writeAmplification().has_value());
    }

    // Case 2: 只有 Host Writes
    {
        ssd::FTL ftl{2, 3};

        check(ftl.write(10));
        check(ftl.write(20));
        check(ftl.write(10));

        check(ftl.metrics().host_writes == 3);
        check(ftl.metrics().gc_page_writes == 0);

        check(
            ftl.flash().physicalProgramCount() == 3
        );

        auto waf = ftl.writeAmplification();
        check(waf.has_value());

        check(std::abs(*waf - 1.0) < 1e-9);
    }

    // Case 3: GC 產生額外的 Page Program
    {
        ssd::FTL ftl{3, 3};

        check(ftl.write(10));
        check(ftl.write(20));
        check(ftl.write(30));
        check(ftl.write(10));
        check(ftl.write(20));

        check(ftl.garbageCollectOne());

        check(ftl.metrics().host_writes == 5);
        check(ftl.metrics().gc_page_writes == 1);
        check(ftl.metrics().gc_cycles == 1);

        check(
            ftl.flash().physicalProgramCount() == 6
        );

        check(ftl.totalEraseCount() == 1);

        auto waf = ftl.writeAmplification();
        check(waf.has_value());

        check(std::abs(*waf - 1.2) < 1e-9);
    }

    // Case 4: GC 不需要搬移有效頁
    {
        ssd::FTL ftl{2, 2};

        check(ftl.write(10));
        check(ftl.write(20));
        check(ftl.write(10));
        check(ftl.write(20));

        check(ftl.garbageCollectOne());

        check(ftl.metrics().host_writes == 4);
        check(ftl.metrics().gc_page_writes == 0);
        check(ftl.metrics().gc_cycles == 1);

        check(ftl.totalEraseCount() == 1);

        check(
            ftl.flash().physicalProgramCount() == 4
        );
    }

    // Case 5: 失敗的 Host Write 不增加計數
    {
        ssd::FTL ftl{1, 1};

        check(ftl.write(10));
        check(!ftl.write(20));
        check(!ftl.write(10));

        check(ftl.metrics().host_writes == 1);

        check(
            ftl.flash().physicalProgramCount() == 1
        );
    }

    testMetricsConsistency();

    std::cout << "[PASS] SSD Performance Metrics\n";
}
