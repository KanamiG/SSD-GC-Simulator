
#include "ssd/ftl.hpp"
#include "ssd/gc_policy.hpp"
#include <cmath>
#include <stdexcept>

namespace ssd {

    FTL::FTL(
        std::size_t block_count,
        std::size_t pages_per_block,
        GcPolicy policy,
        double wear_weight
    )
        : flash_(block_count, pages_per_block),
        policy_(policy),
        wear_weight_(wear_weight)
    {

        if(wear_weight_ < 0 or !std::isfinite(wear_weight_)){
            throw std::invalid_argument("invalid wear weight");
        }
    }

    bool FTL::write(LogicalPageId lpn) {
        auto it = mapping_.find(lpn);

        // Case A: 更新既有 LPN
        if (it != mapping_.end()) {

            auto newPpa = flash_.program(lpn);

            if (!newPpa.has_value()) {
                return false;
            }

            if (!flash_.invalidate(it->second)) {
                throw std::logic_error(
                    "FTL mapping inconsistency"
                );
            }       
            it->second = newPpa.value();

            metrics_.host_writes++;

            return true;
        }

        // Case B: 首次寫入新 LPN

        auto map_it = mapping_.try_emplace(lpn, PhysicalPageAddress{}).first;

        auto newPpa = flash_.program(lpn);

        if(!newPpa.has_value()) {
            mapping_.erase(map_it);
            return false;
        }

        map_it->second = newPpa.value();

        metrics_.host_writes++;

        return true;
    }


    std::optional<PhysicalPageAddress>
    FTL::lookup(LogicalPageId lpn) const {

        auto it = mapping_.find(lpn);

        if (it == mapping_.end()) {
            return std::nullopt;
        }

        return it->second;
    }

    std::size_t FTL::mappedCount() const noexcept {

        return mapping_.size();
    }

    std::optional<std::size_t>
    FTL::selectVictimBlock() const {

        std::size_t total_free = 0;
        for(std::size_t b = 0; b < flash_.blockCount(); ++b) {
            total_free += flash_.block(b).freeCount();
        }

        std::optional<std::size_t> best_victim;
        std::size_t best_invaild_count = 0;
        std::optional<std::size_t> best_closed_epoch;
        std::optional<double> best_score;

        for (std::size_t b = 0;
            b < flash_.blockCount();
            ++b) {

            const auto& block = flash_.block(b);

            std::size_t validCount = block.validCount();
            std::size_t invalidCount = block.invalidCount();
            std::size_t freeCount = block.freeCount();

            std::size_t freePagesOutside = total_free - freeCount;
            if(invalidCount == 0 || freeCount != 0 || validCount > freePagesOutside) {
                continue;
            }

            if(policy_ == GcPolicy::Greedy){
                if(!best_victim || invalidCount > best_invaild_count) {
                    best_victim = b;
                    best_invaild_count = invalidCount;
                }
            }


            else if(policy_ == GcPolicy::FIFO){
                auto epoch = flash_.closedEpoch(b);
                if(!epoch.has_value()){
                    throw std::logic_error(
                        "Closed block has no epoch"
                    );
                }
                if(!best_victim || epoch.value() < best_closed_epoch.value()) {
                    best_victim = b;
                    best_closed_epoch = epoch.value();
                }
            }
            else if (policy_ == GcPolicy::WearAwareGreedy) {

                const auto eraseCount = block.eraseCount();

                const double score = wearAwareScore(
                    validCount, 
                    eraseCount,
                    wear_weight_
                );

                if (!best_victim || score < *best_score) {
                    best_victim = b;
                    best_score = score;
                }

            }
            else{
                throw std::logic_error(
                    "Unknown GC policy"
                );
            }
        }
        return best_victim;
    }

    bool FTL::garbageCollectOne() {

        auto victim = selectVictimBlock();

        if (!victim) {
            return false;
        }

        const std::size_t victim_index = *victim;

        const auto& block = flash_.block(victim_index);

        auto valid_before = block.validCount();
        auto invalid_before = block.invalidCount();

        auto free_outside = 
            freePageCount() - block.freeCount();

        auto epoch = flash_.closedEpoch(victim_index);

        if (!epoch) {
            throw std::logic_error(
                "GC victim has no Closed Epoch"
            );
        }


        // Guarantee room for the event before mutating flash/mappings.
        // Grow geometrically to avoid reallocating on every GC cycle.
        if (gc_events_.size() == gc_events_.capacity()) {
            const auto capacity = gc_events_.capacity();
            const auto remaining = gc_events_.max_size() - capacity;
            if (remaining == 0) {
                throw std::length_error("GC event log capacity exhausted");
            }
            const auto growth = capacity == 0 ? 1 : capacity;
            const auto increment = growth < remaining ? growth : remaining;
            gc_events_.reserve(capacity + increment);
        }

        const auto gc_writes_before = metrics_.gc_page_writes;


        // 原本的 Migration
        migrateBlockPages(victim_index);

        // 原本的 Migration 驗證
        if (flash_.block(victim_index).validCount() > 0) {
            throw std::logic_error(
                "Migration incomplete"
            );
        }

        // 原本的 Erase
        if (!flash_.eraseBlock(victim_index)) {
            throw std::logic_error(
                "Erase failed"
            );
        }

        ++metrics_.gc_cycles;

        const auto migrated_pages = metrics_.gc_page_writes - gc_writes_before;


        gc_events_.push_back(GcEvent{
            .gc_cycle = metrics_.gc_cycles,
            .victim_block = victim_index,
            .valid_pages = valid_before,
            .invalid_pages = invalid_before,
            .free_pages_outside = free_outside,
            .migrated_pages = migrated_pages,
            .closed_epoch = *epoch
        });


        return true;
    }


    std::size_t FTL::freePageCount() const {

        std::size_t total = 0;
        for(std::size_t b = 0; b < flash_.blockCount(); ++b) {
            total += flash_.block(b).freeCount();
        }
        return total;
    }

    std::size_t FTL::logicalCapacity() const noexcept {
        return (flash_.blockCount() - 1) * flash_.pagesPerBlock();
    }

    bool FTL::writeManaged(LogicalPageId lpn) {
        if(!mapping_.contains(lpn) &&
            mappedCount() >= logicalCapacity()) {
                return false;
        }

        if(freePageCount() <= flash_.pagesPerBlock()) {
            const bool gc_succeeded = garbageCollectOne();

            if(!gc_succeeded && freePageCount() == 0) {
                return false;
            }
        }

        return write(lpn);
    }


    std::optional<double>
    FTL::writeAmplification() const noexcept {

        if(metrics_.host_writes == 0) {
            return std::nullopt;
        }

        double physical_programs = static_cast<double>(flash_.physicalProgramCount());
        double host_writes = static_cast<double>(metrics_.host_writes);

        return physical_programs / host_writes;
    }


    std::uint64_t
    FTL::totalEraseCount() const noexcept {

        std::uint64_t total = 0;

        for(std::size_t b = 0; b < flash_.blockCount(); ++b) {
            total += flash_.block(b).eraseCount();
        }

        return total;
    }


    void FTL::migrateBlockPages(std::size_t victim) {
        const auto& victim_block = flash_.block(victim);
        for(std::size_t page_index = 0; page_index < flash_.pagesPerBlock(); ++page_index) {
            const auto& page = victim_block.page(page_index);
 
            if(page.state != PageState::Valid) {
                continue;
            }

            auto new_lpn = page.lpn; 
            if(!new_lpn.has_value()) {
                throw std::logic_error("Valid page has no LPN");
            }
            
            PhysicalPageAddress oldPpa{
                victim,
                page_index
            };

            auto it = mapping_.find(new_lpn.value());

            if (it == mapping_.end() ||
                it->second != oldPpa) {
                throw std::logic_error(
                    "GC mapping inconsistency"
                );
            }
            
            auto new_ppa = flash_.program(new_lpn.value());

            if(!new_ppa.has_value()) {
                throw std::logic_error("No space to migrate valid page");
            }

            ++metrics_.gc_page_writes;


            if(new_ppa->block == victim) {
                throw std::logic_error("Migrated page allocated in victim block");
            }
            if(!flash_.invalidate(it->second)) {
                throw std::logic_error("FTL mapping inconsistency during migration");
            }
            it->second = new_ppa.value();

        }
    }

    

} // namespace ssd
