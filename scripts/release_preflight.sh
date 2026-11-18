#!/bin/sh
# Read-only validation for an already committed release candidate.
# It does NOT audit historical secrets, licensing rights, or run C++ tests.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

fail() {
    printf '[FAIL] %s\n' "$1" >&2
    exit 1
}

command -v git >/dev/null 2>&1 || fail 'git is required'
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || fail 'not a Git working tree'

if [ -n "$(git status --porcelain --untracked-files=normal)" ]; then
    fail 'Git worktree is not clean; commit or resolve changes first'
fi

grep -Fq 'project(SSD_GC_Simulator VERSION 1.0.0 LANGUAGES CXX)' CMakeLists.txt \
    || fail 'CMake version is not 1.0.0'

grep -Fq 'MIT License' LICENSE || fail 'MIT License file missing or unexpected'

for file in README.md docs/DESIGN.md docs/METRICS.md \
            docs/RELEASING.md docs/RELEASE_NOTES_v1.0.0.md \
            .github/workflows/ci.yml .github/workflows/macos-manual.yml; do
    [ -s "$file" ] || fail "missing or empty: $file"
done

# Include all tracked paths even though .gitignore has exclusions.
# Generated/unwanted files should not be included in the release source tree.
artifact_count=0
artifact_examples=''
while IFS= read -r tracked; do
    case "$tracked" in
        .venv/*|venv/*|build/*|build-*/*|build_*/*|cmake-build-*/*|results/*|*/__pycache__/*|__pycache__/*|*.pyc|*.pyo|*.DS_Store|.env|.env.*)
            artifact_count=$((artifact_count + 1))
            if [ "$artifact_count" -le 5 ]; then
                artifact_examples="${artifact_examples}${tracked}\n"
            fi
            ;;
    esac
done <<EOF
$(git ls-files)
EOF

if [ "$artifact_count" -gt 0 ]; then
    printf '[FAIL] %s unwanted tracked paths remain; examples:\n' "$artifact_count" >&2
    printf '%b' "$artifact_examples" >&2
    exit 1
fi

printf '[PASS] Git worktree clean; project version 1.0.0; required release files present.\n'
printf '[PASS] No generated artifacts among tracked files.\n'
printf '[NOTE] You must still verify MIT rights, history safety, CI, tests, and release notes.\n'
