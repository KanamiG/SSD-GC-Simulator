# SSD GC Simulator — Checkpoint 10A Repository Audit

Date: 2026-11-18. Scope: uploaded `ssd-gc-audit.zip` (source snapshot).

## Executive assessment

**Core simulator is usable and all current tests pass after fixing one portability issue.**
It is *not yet* GitHub v1.0: `ssd_sim` is still a development demo, documentation
was a Lesson 1 starter, there is no repository initialization or GitHub CI,
and some historical experimental diagnostics depend on an exact macOS/libc++
random-number sequence.

## Inventory (uploaded snapshot)

- 8 public headers under `include/ssd`.
- 6 core implementation files linked into `ssd_core`.
- 21 self-contained CTest source files.
- 9 research and diagnostic executables (originally exposed by optional CMake flag).
- 11 Python analysis/validation scripts.
- 185 `TODO` comment markers in source/scripts/tests. The executable code behind
  the teaching prompts was generally already implemented; *not* 185 missing features.

## Validated audit results

1. Initial Linux/GCC clean build: **FAIL** — `src/flash.cpp` omitted explicit
   `<stdexcept>` and `<limits>` includes. It built on macOS by relying on
   transitive includes.
2. After correcting these headers: Linux/GCC Debug **21/21 CTest PASS**.
3. Linux/GCC AddressSanitizer + UndefinedBehaviorSanitizer: **21/21 PASS**.
4. After cleanup: fresh Linux/GCC Debug build **21/21 PASS**.
5. After cleanup: all **9 optional experiment executables compile** in a
   separate build when legacy diagnostic targets are enabled; the standard
   experiment build contains **7 general-use executables**.
6. `python -m compileall scripts`: **PASS**. This validates Python syntax,
   **not** runtime correctness of every script.
7. **Runtime portability warning:** historical `experiment_gc_decision_trace`
   expects GC #455 at host write 2103 with specific victim IDs. This
   expectation does *not* hold in this Linux/GCC/libstdc++ build; a GC-count
   exception was observed. A GC #455 from `experiment_lambda_threshold` also
   occurred at host write 2138 instead. Do not treat fixed-seed trace details
   as portable across random-distribution implementations.
8. A full 320-run benchmark was **not completed in the audit environment**;
   earlier 320 runs were verified from user-provided macOS output. Do not
   claim end-to-end cross-platform 320-run validation from this audit.

## Changes included in `SSD-GC-Simulator-10A-clean.zip`

- Removed 185 outdated classroom `TODO` markers and adjacent instruction-only
  comments from 30 files; retained executable statements.
- Fixed `src/flash.cpp` includes for Linux/GCC build.
- Replaced `README.md` and `docs/DESIGN.md` to reflect the *implemented* design,
  document build steps, metrics, model limits, and CLI development status.
- Expanded `.gitignore` to exclude generated CSVs, `results/`, all `build*/`
  directories, Python caches and venvs, and IDE files.
- Added `requirements-analysis.txt` for Python optional dependencies.
- Fixed unconditional event-vector access and incorrect sign of critical
  lambda ratio in the historical lambda-threshold diagnostic.
- Changed the initial `ssd_sim` banner from "Lesson 1" to a development demo.
- Added `SSD_BUILD_LEGACY_TRACES` CMake option, default OFF. Combined with
  `SSD_BUILD_EXPERIMENTS=ON`, it opt-ins the two hard-coded historical
  diagnostics; the other seven experiment programs remain independently buildable.

## Triage: keep, isolate, defer

| Item | Decision | Why |
|---|---|---|
| `ssd_core` (six .cpp files) | KEEP | Real reusable simulator implementation |
| `include/ssd/*.hpp` | KEEP | Public API and policy helper |
| 21 `tests/test_*.cpp` files | KEEP | All passing, coverage of distinct invariants |
| Seven general experiments | OPTIONAL | Valuable, but not needed for typical build |
| `experiment_lambda_threshold`, `experiment_gc_decision_trace` | LEGACY OPT-IN | Hard-coded trace numbers depend on libc++ RNG behavior |
| Eleven analysis scripts | KEEP FOR NOW | Some overlap; runtime/data dependency check required before consolidation |
| Root CSV files (not in audit zip) | IGNORE FROM GIT | Generated results; optional future curated fixtures |
| `Block::page` vs `pageAt` | REVISIT | Duplicate public query methods; tests use both |
| Global `FTLMetrics`, `GcEvent` structs | REVISIT | Defined outside `ssd` namespace; public API hygiene issue |
| `FTL::migrateVictimPages()` | REVISIT | Public partial-operation API; migration without erase intentionally exposed to tests |
| `src/main.cpp` | REPLACE IN 10C | Current program is a small Block demo, not configurable CLI |

## Remaining GitHub v1.0 checkpoints

**10B Core reliability:** inspect write/GC invariants and edge conditions,
add regression cases, decide whether to namespace and encapsulate API types.
**10C CLI:** implement a robust `ssd_sim --help` with validated configuration,
policy/workload choices, and reproducible benchmark output.
**10D Repository structure:** move optional experiments to their own directory
only after checking relative includes and CMake targets; decide which Python
scripts are maintained vs historical.
**10E CI:** GNU/Clang build matrix, CTest, sanitizers where supported.
**10F Docs:** configuration and metrics reference, examples, support limitations,
license selection.
**10G Git release:** `git init`, clean-clone build, release tag, and GitHub
repository setup.

## Local verification commands

```bash
cmake -S . -B build-clean -DCMAKE_BUILD_TYPE=Debug
cmake --build build-clean -j 4
ctest --test-dir build-clean --output-on-failure

cmake -S . -B build-exp -DBUILD_TESTING=OFF -DSSD_BUILD_EXPERIMENTS=ON
cmake --build build-exp -j 4

python -m compileall -q scripts
```

Build the historical debug traces *only if you accept implementation-specific
trace assumptions*:

```bash
cmake -S . -B build-legacy -DBUILD_TESTING=OFF \
  -DSSD_BUILD_EXPERIMENTS=ON -DSSD_BUILD_LEGACY_TRACES=ON
cmake --build build-legacy -j 4
```

This is a **development cleanup snapshot**, not the completed public v1.0
release. Do not overwrite a newer working tree without backing it up first.
