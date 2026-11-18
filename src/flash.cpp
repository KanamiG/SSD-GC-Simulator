#include "ssd/flash.hpp"

#include <limits>
#include <stdexcept>

namespace ssd {
    Flash::Flash(
        std::size_t block_count,
        std::size_t pages_per_block

    ) : pages_per_block_(pages_per_block) {
        if (block_count == 0) {
            throw std::invalid_argument("Flash must contain at least one block");
        }
        if (pages_per_block == 0) {
            throw std::invalid_argument("A block must contain at least one page");
        }
        if (block_count > std::numeric_limits<std::size_t>::max() / pages_per_block) {
            throw std::invalid_argument("Total pages exceed maximum size_t value");
        }
        blocks_.reserve(block_count);
        for (std::size_t i = 0; i < block_count; ++i) {
            blocks_.emplace_back(pages_per_block); 
        }
        closed_epochs_.resize(block_count, std::nullopt);
    }

    std::size_t Flash::blockCount() const noexcept {
        return blocks_.size();
    }

    std::size_t Flash::pagesPerBlock() const noexcept {
        return pages_per_block_;
    }

    std::size_t Flash::totalPages() const noexcept {
        return blockCount() * pagesPerBlock();
    }

    const Block& Flash::block(std::size_t index) const {
        return blocks_.at(index);
    }

    std::size_t Flash::toPpn(PhysicalPageAddress addr) const {
        if (addr.block >= blockCount()) {
            throw std::out_of_range("Block index out of range");
        }
        if (addr.page >= pagesPerBlock()) {
            throw std::out_of_range("Page index out of range");
        }
        return addr.block * pagesPerBlock() + addr.page;
    }

    PhysicalPageAddress Flash::fromPpn(std::size_t ppn) const {
        if (ppn >= totalPages()) {
            throw std::out_of_range("PPN out of range");
        }
        return PhysicalPageAddress{
            .block = ppn / pagesPerBlock(),
            .page = ppn % pagesPerBlock()
        };
    }


    std::optional<PhysicalPageAddress>
    Flash::program(LogicalPageId lpn) {

        const std::size_t count = blockCount();

        for (std::size_t i = 0; i < count; ++i) {

            const std::size_t index =
                (next_write_block_ + i) % count;

            auto& block = blocks_.at(index);

            auto result = block.program(lpn);

            if (result.has_value()) {
                if(block.freeCount() == 0){
                    closed_epochs_[index] = ++next_closed_epoch_;
                }

                next_write_block_ = index;

                ++physical_program_count_;

                return PhysicalPageAddress{
                    .block = index,
                    .page = result.value()
                };
            }
        }
        
        return std::nullopt;
    }


    bool Flash::invalidate(PhysicalPageAddress addr) {
        return blocks_.at(addr.block).invalidate(addr.page);
    }

    bool Flash::eraseBlock(std::size_t block_index) {
        if(!blocks_.at(block_index).erase()){
            return false;
        }
        closed_epochs_[block_index].reset();
        return true;
    }

    std::uint64_t
    Flash::physicalProgramCount() const noexcept {
        return physical_program_count_;
    }

    std::optional<std::uint64_t>
    Flash::closedEpoch(std::size_t block_index) const {
        return closed_epochs_.at(block_index);
    }

}