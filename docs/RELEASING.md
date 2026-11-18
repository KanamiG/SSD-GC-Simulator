# Release checklist (pre-v1.0)

**Current status:** CMake project version `0.1.0`. This checklist describes
how to *prepare* a future v1.0.0 release. Completing a commit is not the same
as publishing a tagged release.

## 1. Verify scope and repository hygiene

- Confirm the intended public API and CLI options are frozen for v1.0.
- Verify that `LICENSE` uses the intended copyright holder and the project
  owner has the rights to distribute **all included source code** under MIT.
- Confirm `.venv/`, `build*/`, and `results/` are ignored and not tracked on
  `main`. Excluding files now does **not remove them from earlier Git history**.
- Review historical commits for accidental credentials or private content;
  if any secret was committed, **revoke/rotate it** before history cleanup.
- Do not use `git push --force` on shared history as a routine release step.

Useful local checks:

```sh
git status --short
git ls-files | grep -E '(^\.venv/|^build[^/]*/|^results/)' || true
```

## 2. Test the exact release candidate

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel 4
ctest --test-dir build-release --output-on-failure
python -m pip install -r requirements-analysis.txt
python -m unittest discover -s tests -p 'test_analysis_*.py' -v
./build-release/ssd_sim --help
```

Expected at the time this document was authored: 34 CTest cases and 3 Python
regression tests. Review counts if the suite changes.

The automatic CI is Linux GCC/Clang + Python. Manual macOS CI is in
`.github/workflows/macos-manual.yml` and can be queued/cancelled before running
if GitHub-hosted runners are unavailable. A **cancelled or never-started** job
must not be reported as a passing macOS CI test; record local Apple Clang
results separately.

## 3. Verify CLI examples and results

```sh
./build-release/ssd_sim --blocks 32 --pages-per-block 16 \
  --logical-pages 480 --writes 10000 --workload hotcold \
  --policy wear-aware --lambda 0.05 --seed 42 \
  --csv results/release-smoke.csv
```

Confirm the summary, WAF accounting, and CSV append behavior. Preserve seed,
platform, and toolchain in any published comparison. The analyzer for
`experiment_wear_weight` does **not** accept this 26-column CLI CSV.

## 4. Finalize the actual version and changelog

- Update `project(... VERSION 0.1.0 ...)` in `CMakeLists.txt` to `1.0.0`
  **only once the public v1.0 contract is ready**.
- Update README status, supported platforms, CLI examples, and documentation.
- Write release notes documenting scope, supported APIs, limitations, test
  results, and any behavior change from pre-v1.0.
- Commit and push the tested release candidate; verify CI for **that commit**.

## 5. Tag and publish deliberately

After reviewing the exact commit on `main` and verifying version/release notes:

```sh
git status --short
git tag -a v1.0.0 -m "SSD GC Simulator v1.0.0"
git push origin v1.0.0
```

Then create a release via the GitHub Releases interface (or `gh release
create v1.0.0 --generate-notes` after reviewing notes). Do **not** execute the
tag commands while the CMake version is still `0.1.0`.

No binaries or installer artifacts are currently packaged by the project; the
initial GitHub release can be source-only unless build packaging is added.
