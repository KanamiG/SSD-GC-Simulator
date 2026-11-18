
#include "ssd/csv_export.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <iostream>

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error(
            "CSV Export test failed"
        );
    }
}

int main() {

    ssd::WorkloadConfig config{
        .kind = ssd::WorkloadKind::Sequential,
        .operations = 100,
        .logical_page_count = 20,
        .seed = 42
    };

    ssd::BenchmarkResult result{};
    result.attempted_writes = 100;
    result.successful_writes = 100;
    result.host_writes = 100;
    result.gc_page_writes = 25;
    result.physical_programs = 125;
    result.gc_cycles = 5;
    result.block_erases = 5;
    result.success_rate = 1.0;
    result.waf = 1.25;

    std::ostringstream buffer;

    ssd::writeCsvHeader(buffer);
    ssd::writeCsvRow(
        buffer, config, 16, 16, result
    );

    const std::string csv = buffer.str();

    check(
        csv.find("workload,seed,operations")
        != std::string::npos
    );

    check(
        csv.find("Sequential,42,100,20,16,16")
        != std::string::npos
    );

    // 驗證最後兩欄及換行
    check(
        csv.find(",5,5,1.25\n")
        != std::string::npos
    );

    std::cout << "[PASS] CSV Export\n";
}
