# SSD GC Simulator

A deterministic **C++20 educational simulator** for page-mapped flash translation
(FTL), out-of-place logical page writes, and block-level garbage collection.
This project simulates *metadata*, not real NAND hardware or timing.

> Project status: **v0.1 development snapshot**. The core library, tests, and
> experimental benchmark executables and a configurable CLI work. Automated
> CI, a stable release process, and final documentation are planned for v1.0.

## Features

- Flash blocks with Free / Valid / Invalid page states and erase counters.
- Sequential physical page programming and logical-to-physical page mapping.
- Out-of-place updates with invalidation and managed GC page relocation.
- Greedy, FIFO, and wear-aware greedy GC victim selection.
- Deterministic Sequential, Random, and HotCold write workloads.
- WAF, GC relocation writes, erase counts, and CSV benchmark export.
- Reproducible experimental workloads and Python statistical analysis.

## Requirements

- CMake 3.16+
- C++20 compiler (tested in this audit with GCC on Linux; previously tested with Apple Clang on macOS)
- Python 3 with `pip` (optional, for analysis scripts)

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

The repository now has **34 CTest cases**: 22 core tests and 12 CLI
integration checks (including invalid inputs and same-seed reproducibility).

## Run the simulator

```bash
./build/ssd_sim --help
./build/ssd_sim --blocks 32 --pages-per-block 16 \
    --logical-pages 480 --writes 10000 --workload hotcold \
    --policy wear-aware --lambda 0.05 --seed 42
```

Supported workloads: `sequential`, `random`, `hotcold`.
Supported GC policies: `greedy`, `fifo`, `wear-aware`.
For `hotcold` you can set `--hot-set-size` and `--hot-probability`.
The CLI checks geometry, logical-capacity limits, numeric parameters, and
incompatible options before running. A successful simulation prints WAF,
GC relocation writes, GC cycles, erase counts, erase CV, and other counters.

Return codes: `0` for successful completion/help, `2` for invalid arguments or
other exceptions, `3` when a valid simulation has unsuccessful host writes.
The CLI can also append a simulation result to a CSV file with `--csv FILE`:

```bash
./build/ssd_sim --workload hotcold --policy wear-aware --lambda 0.05 \
    --csv results/cli_runs.csv
./build/ssd_sim --workload random --policy greedy \
    --csv results/cli_runs.csv
```

The first invocation creates a header; subsequent invocations append one row.
Parent directories are created automatically. Existing CSVs with a different
header are rejected rather than mixed with incompatible data. Columns include
configuration and physical/GC metrics; WAF and success rate are empty when
undefined (`--writes 0`). CSV append is not concurrent-writer safe.
Use a separate filename from the historical `wear_weight_sensitivity.csv`
experiment, whose schema is different. The existing library CSV exporter is
not changed.


## Optional experiments

Experiments are excluded from the normal build. To build them:

```bash
cmake -S . -B build-exp -DBUILD_TESTING=OFF -DSSD_BUILD_EXPERIMENTS=ON
cmake --build build-exp -j 4
```

Examples (run from the repository root; the experiment creates `results/`):

```bash
./build-exp/experiment_wear_weight
python -m pip install -r requirements-analysis.txt
python scripts/analyze_wear_weight.py
```

The wear-weight experiment currently runs 320 combinations of four logical
address-space sizes, two workload types, five seeds, and eight policies/weights.
Results and generated plots live under `results/`, ignored by Git.

Two historical trace diagnostics are excluded by default because they rely on
specific event indices observed with an earlier macOS/libc++ random sequence.
They can be built with `-DSSD_BUILD_LEGACY_TRACES=ON` **together with**
`-DSSD_BUILD_EXPERIMENTS=ON`, but their hard-coded expected events are not
portable across standard-library RNG implementations.

Other experimental executables are development tools, not stable APIs.

## Layout

- `include/ssd/`: public C++ headers
- `src/`: simulator core implementation only
- `apps/ssd_sim.cpp`: command-line application
- `experiments/`: optional benchmark and historical diagnostic executables
- `tests/`: self-contained CTest executables
- `scripts/`: Python experiments/analysis
- `docs/DESIGN.md`: architecture and model assumptions

## Model limitations

- Metadata-only page model; no real data payload or read/trim interface.
- No NAND operation latency, ECC, wear-out failures, channels or concurrency.
- `FTL::logicalCapacity()` reserves one block worth of pages; the workload's
  `logical_page_count` is an address-range parameter, not measured occupancy.
- WAF and erase-count dispersion are *simulation metrics*, not device lifetime
  or real-world latency predictions.
- C++ standard-library random distribution implementations can vary across
  toolchains; seed-based reproducibility is within a fixed implementation.

See [Design](docs/DESIGN.md) for details.

## Roadmap to v1.0

Completed: core reliability regression tests, configurable CLI, CSV export, and
separate `apps/`, `src/`, `experiments/`, and `tests/` directories.

Still planned: GitHub Actions, stable documentation, license, and release validation.
