# Release candidate checklist — v1.0.0

The CMake project version is now **1.0.0**, but this repository remains a
**release candidate until a `v1.0.0` tag and GitHub Release are created**.
Do not claim the official release is published until those actions are complete.

## 1. Confirm rights, repository scope and history

- Confirm `LICENSE` lists the desired copyright holder and that all included
  contributions may be distributed under **MIT**.
- Confirm the intended public C++ interfaces, CLI flags and CSV schema are
  frozen for v1.0.0.
- Review prior commits for secrets/private content (including original `.venv`,
  `build*` and `results` files). A clean current tree does not remove old
  files from Git history. **Revoke/rotate any exposed secret immediately.**
- Do not rewrite shared history or force-push casually; coordinate first.

Remove the *previously tracked* Python bytecode from the Git index (local
files are retained):

```sh
git rm -r --cached --ignore-unmatch scripts/__pycache__ tests/__pycache__
```

Our `.gitignore` already excludes these paths for future commits. Confirm no
other cached or generated files remain in the tracked source tree:

```sh
git ls-files | grep -E '(^\.venv/|^build[^/]*/|^results/|(^|/)__pycache__/|\.pyc$)' || true
```

## 2. Commit the candidate and run local preflight

```sh
git status --short
git add CMakeLists.txt README.md docs/RELEASING.md \
  docs/RELEASE_NOTES_v1.0.0.md scripts/release_preflight.sh

git commit -m "chore: prepare v1.0.0 release candidate"
sh scripts/release_preflight.sh
```

`release_preflight.sh` checks repository hygiene and version/document presence,
not license rights, historical secrets, compilation, or CI. Its successful
exit is **necessary but not sufficient** for release.

## 3. Test the exact candidate

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel 4
ctest --test-dir build-release --output-on-failure

python -m pip install -r requirements-analysis.txt
python -m unittest discover -s tests -p 'test_analysis_*.py' -v

./build-release/ssd_sim --help
./build-release/ssd_sim --blocks 32 --pages-per-block 16 \
  --logical-pages 480 --writes 10000 --workload hotcold \
  --policy wear-aware --lambda 0.05 --seed 42 \
  --csv results/release-smoke.csv
```

Expect **34 CTest** and **3 Python unittest** cases. The smoke run should
report `Successful writes: 10000` and append one 26-column CSV data row. Run
Debug CTest too when feasible; test the actual committed code, not older build
artifacts.

Use a clean checkout or a fresh CMake build directory for the final smoke.
Avoid running the same CSV append command repeatedly if counting rows.

## 4. Verify GitHub CI for the release candidate commit

```sh
git push origin main
git rev-parse HEAD
```

Confirm the Linux GCC/Clang Debug/Release + Python **5/5** CI checks passed
for this exact SHA. The hosted macOS workflow is manual and may be cancelled
before receiving a runner; such a result is not a macOS pass. Report local
Apple Clang Debug/Release results separately.

## 5. Publish only after sign-off

Review the draft in `docs/RELEASE_NOTES_v1.0.0.md` and ensure the repository
is clean, rights and history are reviewed, and all release checks pass.

```sh
git status --short
git rev-parse HEAD
git tag -a v1.0.0 -m "SSD GC Simulator v1.0.0"
git push origin v1.0.0
```

Create the GitHub Release for `v1.0.0`, using a reviewed copy of release notes:

```sh
gh release create v1.0.0 \
  --title "SSD GC Simulator v1.0.0" \
  --notes-file docs/RELEASE_NOTES_v1.0.0.md
```

**Do not run these tag/release commands during candidate preparation.** A
source-only release is sufficient; no pre-built executables are packaged.
