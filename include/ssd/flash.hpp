
#pragma once

#include "ssd/block.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ssd {

struct PhysicalPageAddress {
    std::size_t block;
    std::size_t page;

    bool operator==(
        const PhysicalPageAddress&
    ) const = default;
};

// In-memory page-level NAND model. Out-of-range block/page addresses throw
// std::out_of_range; programming a full device returns std::nullopt.
// Failed erase operations leave the block and its closed epoch unchanged.
class Flash {
public:
    Flash(
        std::size_t block_count,
        std::size_t pages_per_block
    );

    [[nodiscard]]
    std::size_t blockCount() const noexcept;

    [[nodiscard]]
    std::size_t pagesPerBlock() const noexcept;

    [[nodiscard]]
    std::size_t totalPages() const noexcept;

    [[nodiscard]]
    const Block& block(std::size_t index) const;

    [[nodiscard]]
    std::size_t toPpn(
        PhysicalPageAddress addr
    ) const;

    [[nodiscard]]
    PhysicalPageAddress fromPpn(
        std::size_t ppn
    ) const;


    // Sequentially program the next available page. Returns std::nullopt
    // without changing counters when all blocks are full.
    [[nodiscard]]
    std::optional<PhysicalPageAddress>
    program(LogicalPageId lpn);

    // Return false for Free/Invalid pages; invalid addresses throw.
    bool invalidate(PhysicalPageAddress addr);

    // Refuse erasure when any page is Valid.
    bool eraseBlock(std::size_t block_index);

    [[nodiscard]]
    std::uint64_t physicalProgramCount() const noexcept;

    // FIFO metadata: assigned when a block fills; reset when erased.
    [[nodiscard]]
    std::optional<std::uint64_t>
    closedEpoch(std::size_t block_index) const;
private:
    std::size_t pages_per_block_;
    std::vector<Block> blocks_;

    std::size_t next_write_block_{0};
    std::uint64_t physical_program_count_{0};

    std::uint64_t next_closed_epoch_{0};

    std::vector<std::optional<std::uint64_t>>
        closed_epochs_;
};

} // namespace ssd
