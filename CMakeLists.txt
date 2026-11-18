cmake_minimum_required(VERSION 3.16)
project(SSD_GC_Simulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build experimental research/diagnostic tools only when requested.
option(SSD_BUILD_EXPERIMENTS "Build SSD GC experiment executables" OFF)
option(SSD_BUILD_LEGACY_TRACES "Build compiler-specific historical trace diagnostics" OFF)

# Reusable simulator core: no main() or experiment code belongs here.
add_library(ssd_core STATIC
    src/block.cpp
    src/flash.cpp
    src/ftl.cpp
    src/workload.cpp
    src/benchmark.cpp
    src/csv_export.cpp
)
target_include_directories(ssd_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
if(MSVC)
    target_compile_options(ssd_core PRIVATE /W4)
else()
    target_compile_options(ssd_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Main application.
add_executable(ssd_sim apps/ssd_sim.cpp)
target_link_libraries(ssd_sim PRIVATE ssd_core)

# Testing is independent of the optional experiment programs.
include(CTest)
if(BUILD_TESTING)
    function(add_ssd_test target_name test_name)
        add_executable(${target_name} tests/${target_name}.cpp)
        target_link_libraries(${target_name} PRIVATE ssd_core)
        add_test(NAME ${test_name} COMMAND ${target_name})
    endfunction()

    add_ssd_test(test_block                  block_basics)
    add_ssd_test(test_flash                  flash_basics)
    add_ssd_test(test_flash_program          flash_program)
    add_ssd_test(test_flash_operations       flash_operations)
    add_ssd_test(test_ftl                    ftl_basics)
    add_ssd_test(test_ftl_update             ftl_update)
    add_ssd_test(test_ftl_invariant          ftl_invariant)
    add_ssd_test(test_gc_selection           gc_selection)
    add_ssd_test(test_gc_migration           gc_migration)
    add_ssd_test(test_gc_complete            gc_complete)
    add_ssd_test(test_gc_automatic           gc_automatic)
    add_ssd_test(test_metrics                metrics)
    add_ssd_test(test_workload               workload)
    add_ssd_test(test_benchmark              benchmark)
    add_ssd_test(test_csv_export             csv_export)
    add_ssd_test(test_gc_policy              gc_policy)
    add_ssd_test(test_gc_trace               gc_trace)
    add_ssd_test(test_wear_aware_score       wear_aware_score)
    add_ssd_test(test_gc_wear_aware          gc_wear_aware)
    add_ssd_test(test_gc_wear_effect         gc_wear_effect)
    add_ssd_test(test_wear_aware_equivalence wear_aware_equivalence)
    add_ssd_test(test_gc_regression           gc_regression)
    # CLI integration tests; these exercise the installed user entry point.
    add_test(NAME cli_help COMMAND ssd_sim --help)
    set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: ssd_sim")

    add_test(NAME cli_sequential COMMAND ssd_sim
        --blocks 8 --pages-per-block 8 --logical-pages 48 --writes 120
        --workload sequential --policy greedy)
    set_tests_properties(cli_sequential PROPERTIES PASS_REGULAR_EXPRESSION "Failed writes: 0")

    add_test(NAME cli_hotcold COMMAND ssd_sim
        --blocks 8 --pages-per-block 8 --logical-pages 48 --writes 120
        --workload hotcold --hot-set-size 8 --hot-probability 0.8
        --policy wear-aware --lambda 0.05 --seed 9)
    set_tests_properties(cli_hotcold PROPERTIES PASS_REGULAR_EXPRESSION "Failed writes: 0")

    add_test(NAME cli_invalid_policy COMMAND ssd_sim --policy nonexistent)
    set_tests_properties(cli_invalid_policy PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli_zero_writes COMMAND ssd_sim --writes 0)
    set_tests_properties(cli_zero_writes PROPERTIES PASS_REGULAR_EXPRESSION "WAF: n/a")

    add_test(NAME cli_reproducible
        COMMAND ${CMAKE_COMMAND} -DSSD_SIM=$<TARGET_FILE:ssd_sim>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_cli_repro.cmake)

    add_test(NAME cli_csv_export
        COMMAND ${CMAKE_COMMAND} -DSSD_SIM=$<TARGET_FILE:ssd_sim>
                -DCSV_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_csv_test
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_cli_csv.cmake)

    add_test(NAME cli_fifo COMMAND ssd_sim
        --blocks 8 --pages-per-block 8 --logical-pages 48 --writes 120
        --workload random --policy fifo --seed 2)
    set_tests_properties(cli_fifo PROPERTIES PASS_REGULAR_EXPRESSION "Failed writes: 0")

    add_test(NAME cli_invalid_geometry COMMAND ssd_sim
        --blocks 4 --pages-per-block 8 --logical-pages 25)
    set_tests_properties(cli_invalid_geometry PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli_invalid_lambda COMMAND ssd_sim
        --policy wear-aware --lambda nan)
    set_tests_properties(cli_invalid_lambda PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli_invalid_unsigned COMMAND ssd_sim --writes -1)
    set_tests_properties(cli_invalid_unsigned PROPERTIES WILL_FAIL TRUE)

    add_test(NAME cli_incompatible_hot_option COMMAND ssd_sim
        --workload random --hot-set-size 8)
    set_tests_properties(cli_incompatible_hot_option PROPERTIES WILL_FAIL TRUE)
endif()

# Optional reproducible experiments, excluded from a normal build.
if(SSD_BUILD_EXPERIMENTS)
    function(add_ssd_experiment target_name source_name)
        add_executable(${target_name} experiments/${source_name}.cpp)
        target_link_libraries(${target_name} PRIVATE ssd_core)
    endfunction()

    add_ssd_experiment(ssd_experiment               experiment)
    add_ssd_experiment(ssd_experiment_multiseed     experiment_multiseed)
    add_ssd_experiment(ssd_experiment_policy        experiment_policy)
    add_ssd_experiment(ssd_experiment_utilization   experiment_utilization)
    add_ssd_experiment(ssd_experiment_gc_trace     experiment_gc_trace)
    add_ssd_experiment(experiment_wear_weight       experiment_wear_weight)
    add_ssd_experiment(experiment_lambda_trace     experiment_lambda_trace)
    # These historical traces assume exact macOS/libc++ Workload RNG sequences.
    # Do not include them in the standard cross-platform experiments.
    if(SSD_BUILD_LEGACY_TRACES)
        add_ssd_experiment(experiment_lambda_threshold experiment_lambda_threshold)
        add_ssd_experiment(experiment_gc_decision_trace experiment_gc_decision_trace)
    endif()
endif()