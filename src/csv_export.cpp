
#include "ssd/csv_export.hpp"

#include <iomanip>
#include <stdexcept>
#include <string>

namespace ssd {

void writeCsvHeader(std::ostream& out) {

    out << "workload,"
        << "seed,"
        << "operations,"
        << "logical_pages,"
        << "blocks,"
        << "pages_per_block,"
        << "attempted,"
        << "successful,"
        << "failed,"
        << "success_rate,"
        << "host_writes,"
        << "gc_writes,"
        << "physical_programs,"
        << "gc_cycles,"
        << "block_erases,"
        << "waf\n";
}

void writeCsvRow(
    std::ostream& out,
    const WorkloadConfig& config,
    std::size_t block_count,
    std::size_t pages_per_block,
    const BenchmarkResult& result
) {
    std::string_view workload_name;

    switch (config.kind) {
        case WorkloadKind::Sequential:
            workload_name = "Sequential";
            break;

        case WorkloadKind::Random:
            workload_name = "Random";
            break;

        case WorkloadKind::HotCold:
            workload_name = "HotCold";
            break;

        default:
            throw std::invalid_argument(
                "Unknown WorkloadKind"
            );
    }
    

    out << std::setprecision(10);

    out << workload_name << ','
        << config.seed << ','
        << config.operations << ','
        << config.logical_page_count << ','
        << block_count << ','
        << pages_per_block << ','
        << result.attempted_writes << ','
        << result.successful_writes << ','
        << result.failed_writes << ',';


    if(result.success_rate.has_value()){
        out << result.success_rate.value();
    }
    out << ',';

    out << result.host_writes << ','
        << result.gc_page_writes << ','
        << result.physical_programs << ','
        << result.gc_cycles << ','
        << result.block_erases << ',';

    if(result.waf.has_value()){
        out << result.waf.value();
    }

    out << '\n';
}

} // namespace ssd
