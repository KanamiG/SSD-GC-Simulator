#include "ssd/benchmark.hpp"
#include "ssd/ftl.hpp"
#include "ssd/workload.hpp"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

struct Options {
    std::size_t blocks{32};
    std::size_t pages_per_block{16};
    std::size_t logical_pages{480};
    std::size_t writes{10'000};
    std::size_t hot_set_size{40};
    std::uint64_t seed{42};
    double hot_probability{0.8};
    double lambda{0.0};
    ssd::WorkloadKind workload{ssd::WorkloadKind::Random};
    ssd::GcPolicy policy{ssd::GcPolicy::Greedy};
    bool lambda_specified{false};
    bool hot_set_specified{false};
    bool hot_probability_specified{false};
    bool help{false};
    std::string csv_path{};
};

void printHelp(std::ostream& out) {
    out <<
        "SSD GC Simulator (C++20, metadata-only model)\n"
        "Usage: ssd_sim [options]\n\n"
        "Options:\n"
        "  --help                  Print this help and exit\n"
        "  --blocks N              Number of flash blocks (default: 32, min: 2)\n"
        "  --pages-per-block N     Pages in each block (default: 16, min: 1)\n"
        "  --logical-pages N       LPN address range (default: 480)\n"
        "  --writes N              Number of host writes (default: 10000)\n"
        "  --workload NAME         sequential | random | hotcold (default: random)\n"
        "  --policy NAME           greedy | fifo | wear-aware (default: greedy)\n"
        "  --lambda X              Wear weight (wear-aware only, default: 0)\n"
        "  --seed N                Random seed (default: 42)\n"
        "  --csv FILE              Append one run to FILE (create header if new)\n"
        "  --hot-set-size N        Hot LPN count (hotcold only, default: 40)\n"
        "  --hot-probability X     Probability of hot access (hotcold only, default: 0.8)\n\n"
        "Example:\n"
        "  ssd_sim --blocks 32 --pages-per-block 16 --logical-pages 480 \\\n"
        "      --writes 10000 --workload hotcold --policy wear-aware \\\n"
        "      --lambda 0.05 --seed 42 --csv results/cli_runs.csv\n\n"
        "Note: Logical pages define the workload address range, not measured occupancy.\n";
}

std::uint64_t parseUnsigned(std::string_view input, std::string_view flag) {
    std::uint64_t parsed{};
    if (input.empty()) {
        throw std::invalid_argument(std::string(flag) + " requires an unsigned integer");
    }
    const auto [end, ec] = std::from_chars(input.data(), input.data() + input.size(), parsed);
    if (ec != std::errc{} || end != input.data() + input.size()) {
        throw std::invalid_argument(std::string(flag) + " requires an unsigned integer");
    }
    return parsed;
}

std::size_t parseSize(std::string_view input, std::string_view flag) {
    const auto value = parseUnsigned(input, flag);
    if (value > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument(std::string(flag) + " is too large for this platform");
    }
    return static_cast<std::size_t>(value);
}

double parseReal(std::string_view input, std::string_view flag) {
    if (input.empty()) {
        throw std::invalid_argument(std::string(flag) + " requires a number");
    }
    const std::string text(input);
    std::size_t consumed{};
    double value{};
    try {
        value = std::stod(text, &consumed);
    } catch (const std::exception&) {
        throw std::invalid_argument(std::string(flag) + " requires a finite number");
    }
    if (consumed != text.size() || !std::isfinite(value)) {
        throw std::invalid_argument(std::string(flag) + " requires a finite number");
    }
    return value;
}

Options parseArgs(int argc, char* argv[]) {
    Options options{};
    for (int i = 1; i < argc; ++i) {
        const std::string_view flag(argv[i]);
        if (flag == "--help") {
            options.help = true;
            return options;
        }
        if (flag.empty() || flag.front() != '-' || i + 1 >= argc) {
            throw std::invalid_argument("Expected --option value (see --help)");
        }
        const std::string_view value(argv[++i]);
        if (flag == "--blocks") {
            options.blocks = parseSize(value, flag);
        } else if (flag == "--pages-per-block") {
            options.pages_per_block = parseSize(value, flag);
        } else if (flag == "--logical-pages") {
            options.logical_pages = parseSize(value, flag);
        } else if (flag == "--writes") {
            options.writes = parseSize(value, flag);
        } else if (flag == "--seed") {
            options.seed = parseUnsigned(value, flag);
        } else if (flag == "--csv") {
            if (value.empty()) {
                throw std::invalid_argument("--csv requires a non-empty file path");
            }
            options.csv_path = std::string(value);
        } else if (flag == "--hot-set-size") {
            options.hot_set_size = parseSize(value, flag);
            options.hot_set_specified = true;
        } else if (flag == "--hot-probability") {
            options.hot_probability = parseReal(value, flag);
            options.hot_probability_specified = true;
        } else if (flag == "--lambda") {
            options.lambda = parseReal(value, flag);
            options.lambda_specified = true;
        } else if (flag == "--workload") {
            if (value == "sequential") {
                options.workload = ssd::WorkloadKind::Sequential;
            } else if (value == "random") {
                options.workload = ssd::WorkloadKind::Random;
            } else if (value == "hotcold") {
                options.workload = ssd::WorkloadKind::HotCold;
            } else {
                throw std::invalid_argument("Unknown workload: " + std::string(value));
            }
        } else if (flag == "--policy") {
            if (value == "greedy") {
                options.policy = ssd::GcPolicy::Greedy;
            } else if (value == "fifo") {
                options.policy = ssd::GcPolicy::FIFO;
            } else if (value == "wear-aware") {
                options.policy = ssd::GcPolicy::WearAwareGreedy;
            } else {
                throw std::invalid_argument("Unknown GC policy: " + std::string(value));
            }
        } else {
            throw std::invalid_argument("Unknown option: " + std::string(flag));
        }
    }
    return options;
}

void validate(const Options& options) {
    if (options.csv_path == "-") {
        throw std::invalid_argument("--csv requires a file path, not stdout");
    }
    if (options.blocks < 2 || options.pages_per_block == 0 || options.logical_pages == 0) {
        throw std::invalid_argument("--blocks must be >= 2; --pages-per-block and --logical-pages must be > 0");
    }
    if (options.blocks > std::numeric_limits<std::size_t>::max() / options.pages_per_block) {
        throw std::invalid_argument("Flash geometry exceeds addressable size");
    }
    if (options.logical_pages > (options.blocks - 1) * options.pages_per_block) {
        throw std::invalid_argument("--logical-pages exceeds FTL logical capacity (one block reserved)");
    }
    if (options.lambda < 0.0) {
        throw std::invalid_argument("--lambda must be >= 0");
    }
    if (options.lambda_specified && options.policy != ssd::GcPolicy::WearAwareGreedy) {
        throw std::invalid_argument("--lambda is only valid with --policy wear-aware");
    }
    if (options.workload == ssd::WorkloadKind::HotCold) {
        if (options.hot_set_size == 0 || options.hot_set_size >= options.logical_pages) {
            throw std::invalid_argument("Hot set size must be in [1, logical-pages - 1]");
        }
        if (options.hot_probability <= 0.0 || options.hot_probability >= 1.0) {
            throw std::invalid_argument("--hot-probability must be strictly between 0 and 1");
        }
    } else if (options.hot_set_specified || options.hot_probability_specified) {
        throw std::invalid_argument("Hot set options require --workload hotcold");
    }
}

const char* workloadName(ssd::WorkloadKind workload) {
    switch (workload) {
        case ssd::WorkloadKind::Sequential: return "sequential";
        case ssd::WorkloadKind::Random: return "random";
        case ssd::WorkloadKind::HotCold: return "hotcold";
    }
    return "unknown";
}

const char* policyName(ssd::GcPolicy policy) {
    switch (policy) {
        case ssd::GcPolicy::Greedy: return "greedy";
        case ssd::GcPolicy::FIFO: return "fifo";
        case ssd::GcPolicy::WearAwareGreedy: return "wear-aware";
    }
    return "unknown";
}

// Stable, one-row-per-invocation CSV schema. This intentionally differs from
// the historical experiment_wear_weight.csv schema; mismatches are rejected.
constexpr std::string_view kCsvHeader =
    "blocks,pages_per_block,physical_pages,ftl_logical_capacity,logical_pages,"
    "writes_requested,workload,policy,lambda,seed,hot_set_size,hot_probability,"
    "attempted,successful,failed,success_rate,waf,host_page_programs,"
    "gc_migration_programs,physical_programs,gc_cycles,block_erases,"
    "max_erases,erase_cv,mapped_logical_pages,free_physical_pages";

void appendRunCsv(
    const std::filesystem::path& path,
    const Options& options,
    const ssd::FTL& ftl,
    const ssd::BenchmarkResult& result,
    std::uint64_t max_erases,
    double erase_cv
) {
    // Reject incompatible or truncated pre-existing files before modifying.
    bool write_header = true;
    if (std::filesystem::exists(path)) {
        if (!std::filesystem::is_regular_file(path)) {
            throw std::runtime_error("CSV destination is not a regular file: " + path.string());
        }
        if (std::filesystem::file_size(path) != 0) {
            std::ifstream old_file(path);
            std::string first_line;
            if (!std::getline(old_file, first_line) || first_line != kCsvHeader) {
                throw std::runtime_error("CSV header mismatch: " + path.string());
            }
            old_file.clear();
            old_file.seekg(-1, std::ios::end);
            char last_char{};
            if (!old_file.get(last_char) || last_char != '\n') {
                throw std::runtime_error("CSV file must end with a newline: " + path.string());
            }
            write_header = false;
        }
    }

    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream out(path, std::ios::app);
    if (!out) {
        throw std::runtime_error("Cannot open CSV for append: " + path.string());
    }
    out << std::setprecision(17);
    if (write_header) {
        out << kCsvHeader << '\n';
    }
    out << options.blocks << ','
        << options.pages_per_block << ','
        << ftl.flash().totalPages() << ','
        << ftl.logicalCapacity() << ','
        << options.logical_pages << ','
        << options.writes << ','
        << workloadName(options.workload) << ','
        << policyName(options.policy) << ','
        << options.lambda << ','
        << options.seed << ','
        << options.hot_set_size << ','
        << options.hot_probability << ','
        << result.attempted_writes << ','
        << result.successful_writes << ','
        << result.failed_writes << ',';
    if (result.success_rate) out << *result.success_rate;
    out << ',';
    if (result.waf) out << *result.waf;
    out << ','
        << result.host_writes << ','
        << result.gc_page_writes << ','
        << result.physical_programs << ','
        << result.gc_cycles << ','
        << result.block_erases << ','
        << max_erases << ','
        << erase_cv << ','
        << ftl.mappedCount() << ','
        << ftl.freePageCount() << '\n';
    out.flush();
    if (!out) {
        throw std::runtime_error("Failed writing CSV: " + path.string());
    }
}

int run(const Options& options) {
    ssd::WorkloadConfig config{};
    config.kind = options.workload;
    config.logical_page_count = options.logical_pages;
    config.operations = options.writes;
    config.seed = options.seed;
    config.hot_set_size = options.hot_set_size;
    config.hot_probability = options.hot_probability;

    ssd::FTL ftl(options.blocks, options.pages_per_block, options.policy, options.lambda);
    const auto workload = ssd::generateWorkload(config);
    const auto result = ssd::runBenchmark(ftl, workload);

    std::uint64_t max_erases = 0;
    const double mean_erases = static_cast<double>(ftl.totalEraseCount()) /
                               static_cast<double>(options.blocks);
    double sum_of_squared_differences = 0.0;
    for (std::size_t b = 0; b < options.blocks; ++b) {
        const auto erases = ftl.flash().block(b).eraseCount();
        if (erases > max_erases) {
            max_erases = erases;
        }
        const double diff = static_cast<double>(erases) - mean_erases;
        sum_of_squared_differences += diff * diff;
    }
    const double erase_cv = mean_erases == 0.0 ? 0.0 :
        std::sqrt(sum_of_squared_differences / static_cast<double>(options.blocks)) / mean_erases;

    std::cout << std::fixed << std::setprecision(4)
              << "SSD GC Simulator\n"
              << "Configuration:\n"
              << "  Blocks: " << options.blocks << '\n'
              << "  Pages per block: " << options.pages_per_block << '\n'
              << "  Physical pages: " << ftl.flash().totalPages() << '\n'
              << "  FTL logical capacity: " << ftl.logicalCapacity() << '\n'
              << "  Logical pages: " << options.logical_pages << '\n'
              << "  Writes requested: " << options.writes << '\n'
              << "  Workload: " << workloadName(options.workload) << '\n'
              << "  Policy: " << policyName(options.policy) << '\n'
              << "  Lambda: " << options.lambda << '\n'
              << "  Seed: " << options.seed << '\n';
    if (options.workload == ssd::WorkloadKind::HotCold) {
        std::cout << "  Hot set size: " << options.hot_set_size << '\n'
                  << "  Hot probability: " << options.hot_probability << '\n';
    }
    std::cout << "Results:\n"
              << "  Attempted writes: " << result.attempted_writes << '\n'
              << "  Successful writes: " << result.successful_writes << '\n'
              << "  Failed writes: " << result.failed_writes << '\n'
              << "  Success rate: ";
    if (result.success_rate) {
        std::cout << *result.success_rate;
    } else {
        std::cout << "n/a";
    }
    std::cout << "\n  WAF: ";
    if (result.waf) {
        std::cout << *result.waf;
    } else {
        std::cout << "n/a";
    }
    std::cout << "\n  Host page programs: " << result.host_writes
              << "\n  GC migration programs: " << result.gc_page_writes
              << "\n  Total physical programs: " << result.physical_programs
              << "\n  GC cycles: " << result.gc_cycles
              << "\n  Block erases: " << result.block_erases
              << "\n  Max erases per block: " << max_erases
              << "\n  Erase CV: " << erase_cv
              << "\n  Mapped logical pages: " << ftl.mappedCount()
              << "\n  Free physical pages: " << ftl.freePageCount()
              << '\n';
    if (!options.csv_path.empty()) {
        appendRunCsv(options.csv_path, options, ftl, result, max_erases, erase_cv);
        std::cout << "  CSV appended: " << options.csv_path << '\n';
    }
    if (result.failed_writes != 0) {
        std::cerr << "Warning: some host writes failed; inspect geometry and workload.\n";
        return 3;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const auto options = parseArgs(argc, argv);
        if (options.help) {
            printHelp(std::cout);
            return EXIT_SUCCESS;
        }
        validate(options);
        return run(options);
    } catch (const std::exception& error) {
        std::cerr << "ssd_sim: " << error.what() << "\nRun 'ssd_sim --help' for usage.\n";
        return 2;
    }
}
