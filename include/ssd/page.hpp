#pragma once

#include <cstdint>
#include <optional>

namespace ssd {

using LogicalPageId = std::uint64_t;

enum class PageState : std::uint8_t {
    Free,     // Erased and not yet programmed
    Valid,    // Contains the current version of an LPN
    Invalid   // Previously programmed; no longer current
};

struct Page {
    PageState state{PageState::Free};
    std::optional<LogicalPageId> lpn{};
};

} // namespace ssd
