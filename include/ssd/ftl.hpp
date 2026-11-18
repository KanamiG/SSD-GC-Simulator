
#pragma once

#include "ssd/flash.hpp"

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <cstdint>
#include <vector>

namespace ssd {

struct FTLMetrics {
    std::uint64_t host_writes{0};
    std::uint64_t gc_page_writes{0};
    std::uint64_t gc_cycles{0};
};

struct GcEvent {
    std::uint64_t gc_cycle{};

    std::size_t victim_block{};

    std::size_t valid_pages{};
    std::size_t invalid_pages{};

    std::size_t free_pages_outside{};

    std::size_t migrated_pages{};

    std::uint64_t closed_epoch{};
};


enum class GcPolicy {
        Greedy,
        FIFO,
        WearAwareGreedy
    };

class FTL {
    public:
        FTL(
            std::size_t block_count,
            std::size_t pages_per_block,
            GcPolicy policy = GcPolicy::Greedy,
            double wear_weight = 0.0
        );

        // Low-level host write; does not perform GC or enforce the
        // managed logical-capacity limit. Prefer writeManaged() for simulations.
        // Host 寫入一個 Logical Page
        [[nodiscard]]
        bool write(LogicalPageId lpn);

        // 查詢 LPN 的目前實體位置
        [[nodiscard]]
        std::optional<PhysicalPageAddress>
        lookup(LogicalPageId lpn) const;

        [[nodiscard]]
        std::size_t mappedCount() const noexcept;

        // 供測試與統計觀察 Flash 狀態
        [[nodiscard]]
        const Flash& flash() const noexcept {
            return flash_;
        }

        [[nodiscard]]
        std::optional<std::size_t>
        selectVictimBlock() const;

        // Run one complete GC cycle: migrate, erase, and record an event.
        [[nodiscard]]
        bool garbageCollectOne();
        
        [[nodiscard]]
        std::size_t freePageCount() const;

        [[nodiscard]]
        std::size_t logicalCapacity() const noexcept;

        [[nodiscard]]
        bool writeManaged(LogicalPageId lpn);

        [[nodiscard]]
        const FTLMetrics& metrics() const noexcept {
            return metrics_;
        }

        [[nodiscard]]
        std::optional<double>
        writeAmplification() const noexcept;

        [[nodiscard]]
        std::uint64_t totalEraseCount() const noexcept;

        [[nodiscard]]
        const std::vector<GcEvent>&
        gcEvents() const noexcept {
            return gc_events_;
        }


    private:
        Flash flash_;

        std::unordered_map<
            LogicalPageId,
            PhysicalPageAddress
        > mapping_;

        void migrateBlockPages(
            std::size_t victim
        );

        FTLMetrics metrics_{};

        GcPolicy policy_{GcPolicy::Greedy};

        std::vector<GcEvent> gc_events_;

        double wear_weight_{0.0};
    };

} // namespace ssd
