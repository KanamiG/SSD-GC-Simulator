
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace ssd {

inline double wearAwareScore(
    std::size_t valid_pages,
    std::uint64_t erase_count,
    double wear_weight
) {
    if(wear_weight < 0 or !std::isfinite(wear_weight)){
            throw std::invalid_argument("wear_weight is negative or not finite");
        }

    double score = static_cast<double>(valid_pages) + wear_weight * static_cast<double>(erase_count);

    return score;
}

} // namespace ssd
