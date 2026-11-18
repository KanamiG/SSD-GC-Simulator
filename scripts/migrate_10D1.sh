#!/usr/bin/env bash
# 10D-1: remove old source locations only after checking that each copy
# is byte-for-byte identical to the new path. Safe to run twice.
set -euo pipefail
cd "$(dirname "$0")/.."
old_files=()
shopt -s nullglob
for old in src/main.cpp src/experiment*.cpp; do
    name=$(basename "$old")
    if [[ "$name" == "main.cpp" ]]; then
        new="apps/ssd_sim.cpp"
    else
        new="experiments/$name"
    fi
    if [[ ! -f "$new" ]]; then
        echo "[ERROR] New file missing: $new" >&2
        exit 1
    fi
    if ! cmp -s "$old" "$new"; then
        echo "[ERROR] Source differs: $old vs $new" >&2
        echo "Merge your local changes before removing the original." >&2
        exit 1
    fi
    old_files+=("$old")
done
if (( ${#old_files[@]} > 0 )); then
    rm -- "${old_files[@]}"
fi
echo "[PASS] Removed ${#old_files[@]} identical old-location files; core src/ intact."
