
#include "ssd/benchmark.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error(
            "Benchmark test failed"
        );
    }
}

int main() {

    // Case 1: 全部成功，沒有 GC
    {
        ssd::FTL ftl{3, 3};

        std::vector<ssd::LogicalPageId> workload{
            10, 20, 30
        };

        auto r = ssd::runBenchmark(ftl, workload);

        check(r.attempted_writes == 3);
        check(r.successful_writes == 3);
        check(r.failed_writes == 0);

        check(r.host_writes == 3);
        check(r.gc_page_writes == 0);
        check(r.physical_programs == 3);
        check(r.gc_cycles == 0);

        check(r.waf.has_value());
        check(std::abs(*r.waf - 1.0) < 1e-9);

        check(r.success_rate.has_value());
        check(std::abs(*r.success_rate - 1.0) < 1e-9);
    }

    // Case 2: Automatic GC + Valid Page Migration
    {
        ssd::FTL ftl{3, 3};

        std::vector<ssd::LogicalPageId> workload{
            10, 20, 30, 10, 20, 40, 50
        };

        auto r = ssd::runBenchmark(ftl, workload);

        check(r.attempted_writes == 7);
        check(r.successful_writes == 7);
        check(r.failed_writes == 0);

        check(r.host_writes == 7);
        check(r.gc_page_writes == 1);
        check(r.physical_programs == 8);
        check(r.gc_cycles == 1);
        check(r.block_erases == 1);

        check(r.waf.has_value());
        check(std::abs(*r.waf - 8.0 / 7.0) < 1e-9);
    }

    // Case 3: Logical Capacity 限制
    {
        ssd::FTL ftl{2, 2};

        std::vector<ssd::LogicalPageId> workload{
            0, 1, 2
        };

        auto r = ssd::runBenchmark(ftl, workload);

        check(r.attempted_writes == 3);
        check(r.successful_writes == 2);
        check(r.failed_writes == 1);

        check(r.host_writes == 2);
        check(r.physical_programs == 2);

        check(r.success_rate.has_value());
        check(
            std::abs(*r.success_rate - 2.0 / 3.0)
            < 1e-9
        );

        check(!ftl.lookup(2).has_value());
    }

    // Case 4: Empty Workload
    {
        ssd::FTL ftl{3, 3};

        std::vector<ssd::LogicalPageId> workload;

        auto r = ssd::runBenchmark(ftl, workload);

        check(r.attempted_writes == 0);
        check(r.successful_writes == 0);
        check(r.failed_writes == 0);

        check(!r.waf.has_value());
        check(!r.success_rate.has_value());
    }

    // Case 5: 第二次 Benchmark 必須使用 Counter Delta
    {
        ssd::FTL ftl{3, 3};

        auto first = ssd::runBenchmark(
            ftl, {10, 20}
        );

        auto second = ssd::runBenchmark(
            ftl, {10}
        );

        check(first.host_writes == 2);
        check(first.physical_programs == 2);

        check(second.host_writes == 1);
        check(second.physical_programs == 1);

        check(second.waf.has_value());
        check(std::abs(*second.waf - 1.0) < 1e-9);

        // FTL 本身的累積計數仍為 3
        check(ftl.metrics().host_writes == 3);
    }

    // Case 6
    {   
        ssd::FTL ftl{3, 3};

        std::vector<ssd::LogicalPageId> workload{
            10, 20, 30, 10, 20, 40, 50
        };

        auto result = ssd::runBenchmark(ftl, workload);

        check(
            result.attempted_writes ==
            result.successful_writes + result.failed_writes
        );

        check(
            result.host_writes ==
            result.successful_writes
        );

        check(
            result.physical_programs ==
            result.host_writes + result.gc_page_writes
        );
    }

    std::cout << "[PASS] Benchmark Runner\n";
}
