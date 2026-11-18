
#include "ssd/ftl.hpp"
#include "ssd/workload.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

struct CandidateScore {
    std::size_t block_index{};
    std::size_t valid_pages{};
    std::size_t invalid_pages{};
    std::uint64_t erase_count{};
    double score{};
};


std::vector<CandidateScore> collectCandidates(
    const ssd::FTL& ftl,
    double lambda
) {
    std::vector<CandidateScore> candidates;

    constexpr std::size_t block_count = 32;

    const auto total_free = ftl.freePageCount();

    for (std::size_t i = 0; i < block_count; ++i) {

        const auto& block = ftl.flash().block(i);

        const auto valid = block.validCount();
        const auto invalid = block.invalidCount();
        const auto free = block.freeCount();
        const auto erases = block.eraseCount();

        if(invalid == 0){
            continue;
        }

        if(free != 0){
            continue;
        }

        const auto outside_free = total_free - free;
        if(valid > outside_free){
            continue;
        }

        const double score = static_cast<double>(valid) + static_cast<double>(erases) * lambda;

        candidates.push_back(CandidateScore{
            i,
            valid,
            invalid,
            static_cast<std::uint64_t>(erases),
            score
        });
    }

    return candidates;
}


std::optional<std::size_t> selectBestCandidate(
    const std::vector<CandidateScore>& candidates
) {
    if (candidates.empty()) {
        return std::nullopt;
    }

    std::size_t best = 0;

    for (std::size_t i = 1; i < candidates.size(); ++i) {

        const auto& current = candidates[i];
        const auto& selected = candidates[best];

        if(current.score < selected.score){
            best = i;
        }

        if(current.score == selected.score){
            best = current.block_index < selected.block_index ? i : best;
        }

    }

    return candidates[best].block_index;
}

struct TraceTarget {
    double lambda;
    std::size_t host_write;
    std::size_t gc_index;
};

constexpr std::array<TraceTarget, 5> targets{{
    {0.100, 2103, 454},
    {0.200, 2103, 454},
    {0.201, 2103, 454},
    {0.100, 2230, 513},
    {0.200, 2230, 513}
}};


int main() {
    ssd::WorkloadConfig config{};

    config.kind = ssd::WorkloadKind::Random;
    config.operations = 10000;
    config.logical_page_count = 480;
    config.seed = 42;
    config.hot_set_size = 40;
    config.hot_probability = 0.8;

    const auto writes = ssd::generateWorkload(config);

    std::cout << std::fixed
              << std::setprecision(17);

    for (const auto& target : targets) {

        const double lambda = target.lambda;

        // 每組實驗使用全新的 FTL
        ssd::FTL ftl{
            32,
            16,
            ssd::GcPolicy::WearAwareGreedy,
            lambda
        };

        std::size_t host_writes = 0;
        bool captured = false;

        for (const auto lpn : writes) {

            // 判斷下一次 Host Write
            // 是否為本次實驗的目標
            const bool at_target =
                host_writes + 1 == target.host_write;

            std::optional<std::size_t> predicted;

            // ===== Step 1：GC 前收集候選者 =====

            if (at_target) {

                const auto current_gc_count =
                    ftl.gcEvents().size();

                if (current_gc_count != target.gc_index) {
                    throw std::runtime_error(
                        "Unexpected GC count before target"
                    );
                }

                const auto candidates =
                    collectCandidates(ftl, lambda);

                predicted =
                    selectBestCandidate(candidates);

                if (!predicted.has_value()) {
                    throw std::runtime_error(
                        "No eligible GC candidate"
                    );
                }

                std::cout
                    << "\n===== GC #"
                    << target.gc_index + 1
                    << " | Lambda " << lambda
                    << " =====\n";

                std::cout
                    << "Host Write: "
                    << target.host_write << '\n';

                std::cout
                    << "Block  Valid  Invalid  Erases  Score\n";

                for (const auto& candidate : candidates) {

                    std::cout
                        << std::setw(5)
                        << candidate.block_index

                        << std::setw(7)
                        << candidate.valid_pages

                        << std::setw(9)
                        << candidate.invalid_pages

                        << std::setw(8)
                        << candidate.erase_count

                        << "  "
                        << candidate.score
                        << '\n';
                }

                std::cout
                    << "Predicted Victim: "
                    << *predicted
                    << '\n';
            }

            // ===== Step 2：執行 Host Write =====

            const auto before =
                ftl.gcEvents().size();

            if (!ftl.writeManaged(lpn)) {
                throw std::runtime_error(
                    "Host write failed"
                );
            }

            ++host_writes;

            // ===== Step 3：目標 GC 發生後驗證 =====

            if (at_target) {

                const auto after =
                    ftl.gcEvents().size();

                // 確保這次 Host Write
                // 恰好新增一筆目標 GC Event
                if (before != target.gc_index ||
                    after != target.gc_index + 1) {

                    throw std::runtime_error(
                        "Target GC event count mismatch"
                    );
                }

                // 已確認 index 有效，才能存取
                const auto actual =
                    ftl.gcEvents()
                       .at(target.gc_index)
                       .victim_block;

                std::cout
                    << "Actual Victim: "
                    << actual << '\n';

                if (*predicted != actual) {

                    std::cout
                        << "[FAIL] Decision mismatch\n";

                    throw std::runtime_error(
                        "Candidate prediction mismatch"
                    );
                }

                std::cout
                    << "[PASS] Decision reproduced\n";

                captured = true;
                break;
            }
        }

        if (!captured) {
            throw std::runtime_error(
                "Target GC not captured"
            );
        }
    }

    std::cout
        << "\n[PASS] All 5 GC decisions reproduced\n";

    return 0;
}
