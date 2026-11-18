
#pragma once

#include "ssd/benchmark.hpp"
#include "ssd/workload.hpp"

#include <cstddef>
#include <ostream>

namespace ssd {

void writeCsvHeader(std::ostream& out);

void writeCsvRow(
    std::ostream& out,
    const WorkloadConfig& config,
    std::size_t block_count,
    std::size_t pages_per_block,
    const BenchmarkResult& result
);

} // namespace ssd
