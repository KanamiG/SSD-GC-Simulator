
#pragma once

#include "ssd/page.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ssd {

enum class WorkloadKind {
    Sequential,
    Random,
    HotCold
};

struct WorkloadConfig {
    WorkloadKind kind{WorkloadKind::Sequential};

    std::size_t operations{0};
    std::size_t logical_page_count{0};

    std::uint64_t seed{42};

    // HotCold 專用
    std::size_t hot_set_size{2};
    double hot_probability{0.8};
};

[[nodiscard]]
std::vector<LogicalPageId>
generateWorkload(const WorkloadConfig& config);

} // namespace ssd
