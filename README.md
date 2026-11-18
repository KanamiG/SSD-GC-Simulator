# SSD GC Simulator

[![CI](https://github.com/KanamiG/SSD-GC-Simulator/actions/workflows/ci.yml/badge.svg)](https://github.com/KanamiG/SSD-GC-Simulator/actions/workflows/ci.yml)

A **C++20 educational SSD garbage-collection simulator** with a page-mapped
flash translation layer (FTL), three garbage-collection policies, configurable
write workloads, a command-line interface, and CSV export.

**Status:** **Version:** 1.0.0 (first stable source release).
This is an **in-memory, metadata-only simulation**, not a NAND controller,
hardware emulator, or model of SSD performance in real time.

## Features

- Free / Valid / Invalid pages; sequential physical programming and block erase.
- Out-of-place host updates with LPN-to-PPA mapping and valid-page relocation.
- GC victim selection: **Greedy**, **FIFO**, and **Wear-aware Greedy**.
- Write workloads: **Sequential**, **Random**, and **HotCold**.
- Metrics: WAF, host/GC physical programs, GC cycles, block erases, erase CV.
- `ssd_sim` CLI, append-only CSV result rows, optional batch experiments.
- CTest regression suite and optional Python experiment analysis.

## Requirements

- A C++20 compiler and CMake **3.16+**.
- Optional: Python 3.12 and `pip` for the maintained analysis script.

Linux (GCC/Clang) is covered by automatic CI. macOS (Apple Clang) has been
tested locally; hosted macOS CI is available as a *manual* workflow while
GitHub runner availability is unreliable. See [CI setup](docs/CI_SETUP.md).

## Quick start

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/ssd_sim --help
```

There are currently **34 CTest cases** (22 core and 12 CLI tests). CTest is
part of the default build via CMake's `BUILD_TESTING` option.

### Run a simulation

```sh
./build/ssd_sim \
  --blocks 32 \
  --pages-per-block 16 \
  --logical-pages 480 \
  --writes 10000 \
  --workload hotcold \
  --hot-set-size 40 \
  --hot-probability 0.8 \
  --policy wear-aware \
  --lambda 0.05 \
  --seed 42
```

The output includes the effective configuration, successful and failed writes,
WAF, physical programs, GC cycles, and block wear statistics. **The numerical
results can differ between C++ standard-library implementations**, even with
the same seed (see [Reproducibility](#reproducibility)).

### CLI options

| Option | Meaning | Default |
| --- | --- | --- |
| `--blocks N` | Number of erase blocks, at least 2 | `32` |
| `--pages-per-block N` | Pages per block, at least 1 | `16` |
| `--logical-pages N` | Workload LPN range size | `480` |
| `--writes N` | Host write requests; zero allowed | `10000` |
| `--workload NAME` | `sequential`, `random`, `hotcold` | `random` |
| `--policy NAME` | `greedy`, `fifo`, `wear-aware` | `greedy` |
| `--lambda X` | Nonnegative wear weight; wear-aware only | `0` |
| `--seed N` | Unsigned workload RNG seed | `42` |
| `--hot-set-size N` | Hot LPN count; HotCold only | `40` |
| `--hot-probability X` | Hot-access probability strictly between 0 and 1; HotCold only | `0.8` |
| `--csv FILE` | Append one simulation row to CSV | Disabled |
| `--help` | Show CLI help | — |

The FTL reserves one block's worth of **logical capacity**:
`logical_pages <= (blocks - 1) * pages_per_block`. `--logical-pages` describes
the possible LPN *address range*, not the number of currently mapped LPNs.
HotCold additionally requires `1 <= hot_set_size < logical_pages`.

**Exit status:** `0` on success/help, `2` on invalid inputs or runtime
exceptions, `3` if a valid run has unsuccessful host writes.

### CSV export

```sh
./build/ssd_sim --workload random --policy greedy \
  --seed 42 --csv results/cli_runs.csv
./build/ssd_sim --workload hotcold --policy wear-aware --lambda 0.05 \
  --seed 42 --csv results/cli_runs.csv
```

The first invocation creates the parent directory and writes a 26-column
header; later invocations append one row without replacing earlier results.
Existing files with a mismatched header or an incomplete final line are
rejected. Do **not** have multiple processes write to the same CSV concurrently.
Undefined rates (e.g. WAF with `--writes 0`) are written as empty fields.

**Important:** this 26-column CLI CSV is **not** the same schema as
`results/wear_weight_sensitivity.csv` from the experiment runner. The maintained
Python analyzer only accepts the latter. See [Metrics](docs/METRICS.md) and
[Python analysis](scripts/README.md).

## Optional research experiments and Python analysis

```sh
cmake -S . -B build-exp -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DSSD_BUILD_EXPERIMENTS=ON
cmake --build build-exp --parallel 4
./build-exp/experiment_wear_weight
python -m pip install -r requirements-analysis.txt
python scripts/analyze_wear_weight.py
```

The wear-weight experiment produces **320 configurations**: four logical-page
counts, two workloads, five seeds, and eight policy/weight combinations.
The analyzer validates that specific design and creates four summary CSV files
and 10 PNG plots under the ignored `results/` directory. Its regression tests
use **synthetic fixtures**, not physical SSD measurements:

```sh
python -m unittest discover -s tests -p 'test_analysis_*.py' -v
```

Historical scripts are preserved under `scripts/legacy/`; legacy trace
experiments may rely on toolchain-specific RNG event indices and are excluded
from the default experiment build. See [Python analysis](scripts/README.md).

## Repository layout

```text
include/ssd/      Public C++ library headers
src/              Block, Flash, FTL, workload, benchmark, CSV implementation
apps/ssd_sim.cpp  User-facing CLI executable
tests/            Core, CLI integration, and Python regression tests
experiments/      Optional batch and diagnostic executables
scripts/          Maintained analyzer and archived legacy scripts
docs/             Architecture, metrics, CI and release process
```

## Design and limitations

The simulated model uses **metadata rather than user-data payloads**. It omits
NAND timing, channels, ECC, read/trim, concurrent I/O, wear-out failures,
power-loss recovery, and production-grade free-block management. WAF and erase
statistics should not be interpreted as measured SSD lifetime or latency.

- [Architecture and GC policies](docs/DESIGN.md)
- [Metric definitions and CSV interpretation](docs/METRICS.md)
- [CI and macOS runner limitations](docs/CI_SETUP.md)
- [Release validation checklist](docs/RELEASING.md)
- [v1.0.0 release notes (draft)](docs/RELEASE_NOTES_v1.0.0.md)

## Reproducibility

The same seed and toolchain/standard-library implementation yield repeatable
workload sequences. Different standard libraries can implement random
distributions differently, so **identical seeds do not guarantee identical
cross-platform WAF or GC traces**. Record the compiler, standard library,
workload parameters, and seed when comparing results; use a common workload
and seed when comparing GC policies.

## License

MIT — see [LICENSE](LICENSE). If publishing under a different license, replace
`LICENSE` and update this section **before creating the release tag**.
