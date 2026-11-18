# SSD GC Simulator v1.0.0

First source-only release of an educational **C++20 SSD garbage-collection simulator**. The project models page-mapped FTL metadata and out-of-place writes, rather than physical NAND latency or real SSD hardware.

## Included in this release

- Page-mapped Flash Translation Layer (FTL), sequential physical page programming, page invalidation, and block erasure.
- Three GC victim-selection policies: **Greedy**, **FIFO**, and **Wear-aware Greedy**.
- **Sequential**, **Random**, and **HotCold** write workloads.
- Configurable `ssd_sim` CLI for geometry, workload, policy, wear weight (lambda), write count, and random seed.
- CSV export of per-run configuration and 26 result fields; optional research experiments and Python analysis.
- **34 CTest cases** and **3 Python regression tests**.

## Build and run

Requires **CMake 3.16+** and a **C++20** compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/ssd_sim --help
```

Example:

```sh
./build/ssd_sim \
  --blocks 32 --pages-per-block 16 --logical-pages 480 \
  --writes 10000 --workload hotcold --policy wear-aware \
  --lambda 0.05 --seed 42 --csv results/cli_runs.csv
```

## Testing and portability

The release-candidate commit `b714fb42601d33366b6e246bc517e2714382f536` passed GitHub Actions checks for Linux GCC and Clang (Debug and Release) and Python analysis (five successful jobs). The project was also tested locally using Apple Clang on macOS. Hosted macOS testing remains a separate manually triggered workflow due to runner-availability problems; it is **not claimed as a passing cloud CI job**.

Random distributions may produce different sequences across C++ standard-library implementations, so the same seed alone does not guarantee identical cross-platform results.

## Scope and limitations

This is a **metadata-only educational model**, not a production SSD controller or a NAND timing simulator. It does not model I/O latency, ECC, channels, data payloads, reads/trim, power-loss recovery, concurrent operations, or wear-out failures. WAF and erase-count variation should not be interpreted as real-device performance or lifetime predictions.

The 26-column CLI CSV format is distinct from the historical CSV expected by the wear-weight research analysis script. See `README.md`, `docs/DESIGN.md`, and `docs/METRICS.md` for details.

## Licensing

Released under the **MIT License**; see `LICENSE`.

**Distribution:** source-only. No prebuilt binaries or installer packages are included.
