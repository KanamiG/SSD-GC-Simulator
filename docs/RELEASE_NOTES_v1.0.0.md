# SSD GC Simulator v1.0.0 — draft release notes

> **Release candidate only.** This document is a draft; no GitHub Release or
> `v1.0.0` tag has been created by this change.

## Overview

First source-only release candidate for a C++20 **educational, in-memory,
metadata-only** SSD garbage-collection simulator. This is not a timing-accurate
NAND/SSD controller model, hardware emulator, or device lifetime predictor.

## What is included

- Page-mapped FTL with out-of-place logical writes and physical block erases.
- Three GC victim-selection policies: Greedy, FIFO, Wear-aware Greedy.
- Sequential, Random and HotCold workloads, configurable CLI, reproducible
  seeds **within the same C++ standard-library implementation**.
- Per-run WAF, physical programs, GC migrations, GC cycles, erase statistics.
- `ssd_sim --csv` append-only result export with 26 defined columns.
- Separate opt-in research experiments and Python statistical analysis.
- 34 CTest cases and 3 Python regression tests.
- Linux GCC/Clang Debug/Release and Python automatic GitHub Actions checks;
  macOS manual hosted workflow (Runner availability permitting).

## Compatibility and requirements

- CMake 3.16+ and a compiler supporting C++20.
- Optional Python 3.12, NumPy, pandas and Matplotlib for wear-weight analysis.
- Source-only GitHub Release: no pre-built binaries, installer or stable CMake
  package export is promised.

## Deliberate limitations

- No NAND latency, payloads, reads/trim, ECC, power-loss recovery, concurrent
  I/O, flash wear-out failures or production SSD allocation model.
- Workload RNG distributions are not guaranteed to generate identical streams
  across different standard library implementations, even with the same seed.
- WAF and erase-count CV are simulation metrics; not measured endurance or
  performance predictions.
- The CLI 26-column CSV is **not compatible** with the historical
  `wear_weight_sensitivity.csv` analysis schema.

## Validation to complete before publishing

- [ ] Verify copyright holder and confirm rights to release all included code
      under the MIT License.
- [ ] Remove tracked `.pyc`/`__pycache__` and other generated files from `main`.
- [ ] Run `scripts/release_preflight.sh` with a clean local Git worktree.
- [ ] Verify Linux GitHub Actions CI passes for the **release candidate commit**.
- [ ] Confirm local macOS Debug/Release testing results separately if hosted
      macOS jobs are still unavailable.
- [ ] Verify CLI help, representative simulation and CSV append operation.
- [ ] Audit Git history for secrets or private content and rotate anything
      exposed. Removing files from HEAD does **not** remove them from history.
- [ ] Confirm `v1.0.0` points to the final validated commit before publication.

See [release checklist](RELEASING.md) for the commands and detailed process.
