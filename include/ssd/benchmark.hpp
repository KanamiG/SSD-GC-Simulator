
#pragma once

#include "ssd/ftl.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ssd {

struct BenchmarkResult {

    // Host Requests
    std::size_t attempted_writes{0};
    std::size_t successful_writes{0};
    std::size_t failed_writes{0};

    // FTL / Flash Metrics
    std::uint64_t host_writes{0};
    std::uint64_t gc_page_writes{0};
    std::uint64_t physical_programs{0};
    std::uint64_t gc_cycles{0};
    std::uint64_t block_erases{0};

    // Derived Metrics
    std::optional<double> waf;
    std::optional<double> success_rate;
};

[[nodiscard]]
BenchmarkResult runBenchmark(
    FTL& ftl,
    const std::vector<LogicalPageId>& workload
);

} // namespace ssd
