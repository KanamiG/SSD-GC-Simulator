#pragma once

#include "ssd/page.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace ssd {

// One NAND erase block. Pages can be programmed sequentially, only once
// between erases. This educational model prevents erasing live data.
class Block {
public:
    explicit Block(std::size_t pages_per_block);

    [[nodiscard]] std::size_t capacity() const noexcept;
    [[nodiscard]] std::size_t eraseCount() const noexcept;
    [[nodiscard]] const Page& page(std::size_t page_index) const;

    // Program the next unused physical page, returning its index.
    // If there is no never-programmed page left, return std::nullopt.
    [[nodiscard]] std::optional<std::size_t> program(LogicalPageId lpn);

    // Mark a Valid page as Invalid. Return false for Free/Invalid pages.
    // Out-of-range indices must throw std::out_of_range.
    [[nodiscard]] bool invalidate(std::size_t page_index);

    // Educational safety invariant: refuse to erase a block with Valid pages.
    // Successful erase frees all pages and increments erase_count_.
    [[nodiscard]] bool erase();

    [[nodiscard]] std::size_t freeCount() const;
    [[nodiscard]] std::size_t validCount() const;
    [[nodiscard]] std::size_t invalidCount() const;

private:
    std::vector<Page> pages_;
    std::size_t next_program_page_{0};
    std::size_t erase_count_{0};
};

} // namespace ssd
