#include "ssd/block.hpp"

#include <stdexcept>

namespace ssd {

    Block::Block(std::size_t pages_per_block) : pages_(pages_per_block) {
        if (pages_per_block == 0) {
            throw std::invalid_argument("A block must contain at least one page");
        }
    }

    std::size_t Block::capacity() const noexcept { return pages_.size(); }
    std::size_t Block::eraseCount() const noexcept { return erase_count_; }

    const Page& Block::page(std::size_t page_index) const {
        return pages_.at(page_index);
    }

    std::optional<std::size_t> Block::program(LogicalPageId lpn) {

        if(next_program_page_ == pages_.size()) {
            return std::nullopt;
        }
        pages_[next_program_page_].state = PageState::Valid;
        pages_[next_program_page_].lpn = lpn;
        return next_program_page_++;
    }

    bool Block::invalidate(std::size_t page_index) {
        auto& page = pages_.at(page_index); // Check bounds
        if(page.state == PageState::Valid) {
            page.state = PageState::Invalid;
            return true;
        }
        return false;

    }

    bool Block::erase() {
        for(const auto& page : pages_ ){
            if(page.state == PageState::Valid) {
                return false;
            }
        }
        for(auto& page : pages_ ){
            page = Page{};
        }
        next_program_page_ = 0;
        ++erase_count_;
        return true;
    }

    std::size_t Block::freeCount() const {
        std::size_t count{0};
        for(const auto& page : pages_){
            if(page.state == PageState::Free) {
                ++count;
            }
        }
        return count;
    }

    std::size_t Block::validCount() const {
        std::size_t count{0};
        for(const auto& page : pages_){
            if(page.state == PageState::Valid) {
                ++count;
            }
        }
        return count;
    }

    std::size_t Block::invalidCount() const {
        std::size_t count{0};
        for(const auto& page : pages_){
            if(page.state == PageState::Invalid) {
                ++count;
            }
        }
        return count;
    }

} // namespace ssd
