
#include "ssd/benchmark.hpp"

#include <stdexcept>

namespace ssd {

BenchmarkResult runBenchmark(
    FTL& ftl,
    const std::vector<LogicalPageId>& workload
) {
    BenchmarkResult result{};

    const auto metrics_before = ftl.metrics();

    const auto physical_programs_before = ftl.flash().physicalProgramCount();
    const auto block_erases_before = ftl.totalEraseCount();

    result.attempted_writes = workload.size();

    for(const auto& lpn : workload) {
        if(ftl.writeManaged(lpn)) {
            result.successful_writes++;
        } else {
            result.failed_writes++;
        }
    }


    result.host_writes = ftl.metrics().host_writes - metrics_before.host_writes;
    result.gc_page_writes = ftl.metrics().gc_page_writes - metrics_before.gc_page_writes; 
    result.physical_programs = ftl.flash().physicalProgramCount() - physical_programs_before;
    result.gc_cycles = ftl.metrics().gc_cycles - metrics_before.gc_cycles;
    result.block_erases = ftl.totalEraseCount() - block_erases_before;

    if(result.host_writes > 0) {
        result.waf = static_cast<double>(result.physical_programs) /
            static_cast<double>(result.host_writes);
    }

    if(result.attempted_writes > 0) {
        result.success_rate = static_cast<double>(result.successful_writes) /
            static_cast<double>(result.attempted_writes);
    }

    return result;
}

} // namespace ssd
