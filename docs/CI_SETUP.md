# GitHub Actions CI — Checkpoint 10E

This repository ships a CI workflow in `.github/workflows/ci.yml`.
It runs on every push, pull request, or manual invocation (`Actions` → `CI` → `Run workflow`).

## CI checks

- **C++:** six configurations: Ubuntu 24.04 (GCC & Clang, Debug & Release) and macOS 15 (Apple Clang, Debug & Release). Each builds `ssd_sim` and runs the 34 CTest cases. Optional experiments remain disabled.
- **Python:** Ubuntu 24.04, Python 3.12; installs `requirements-analysis.txt`, checks Python syntax, then runs the three synthetic-data analysis regression tests.

These jobs deliberately do **not** check numerical equality of the generated Random/HotCold workloads across standard libraries; seed reproducibility is only guaranteed within a fixed standard-library implementation.

## Verify locally before your first push

From the project root:

```bash
cmake -S . -B build-ci -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build-ci --parallel 4
ctest --test-dir build-ci --output-on-failure
python -m pip install -r requirements-analysis.txt
python -m unittest discover -s tests -p 'test_analysis_*.py' -v
```

A successful run currently reports **34/34 CTest** and **3/3 Python unit tests**.

## Create the repository (once)

Make a backup and verify `.gitignore` before staging. If this directory is not a Git repository:

```bash
git init -b main
git status --short
git add .
git diff --cached --stat
git diff --cached --name-only
```

Inspect `git diff --cached --name-only`: no `build-*`, `.venv/`, `results/`, generated CSVs, credentials, or private data should appear. Remove anything unexpected from the staging area before committing:

```bash
git restore --staged PATH/TO/FILE
```

Then configure your Git identity if necessary, and commit:

```bash
git commit -m "Initial SSD GC simulator with CLI, tests, and CI"
```

Create a **new empty repository** called `SSD-GC-Simulator` on GitHub (do not initialize it with another README or license to avoid a conflicting history). From the local repository:

```bash
git remote add origin https://github.com/YOUR_USERNAME/SSD-GC-Simulator.git
git push -u origin main
```

Use your real GitHub username in the URL. Authentication is handled by your Git credential manager or GitHub CLI; do not paste access tokens into the repository or chat.

GitHub Actions will start after the workflow file is pushed. The first green run must be checked on the GitHub **Actions** tab; local tests alone do not confirm hosted CI works.

## If CI fails

1. Open the failed job and its first failing step.
2. For CMake errors, compare the runner compiler and `CMAKE_BUILD_TYPE` shown in the logs.
3. For CTest failures, inspect the `ctest --output-on-failure` output.
4. For Python failures, check dependency installation, expected CSV fixture, and working directory.
5. Fix and push a new commit; do not disable failing tests to make CI green.

## Before public v1.0

Add an explicit license, finish documentation, and check a clean clone. The CI workflow itself does not constitute confirmation of a public release.
