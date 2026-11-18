#!/bin/sh
# Remove old root-level historical scripts after the 10D-2 overlay is extracted.
# Verify all copies match before deleting anything. Works with macOS /bin/sh.
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_root"

names='analyze_benchmark.py analyze_gc_trace.py analyze_gc_wear.py analyze_multiseed.py analyze_paired_ci.py analyze_policy.py analyze_utilization.py plot_gc_migration.py plot_utilization.py verif_multiseed.py'

# Preflight: never remove even one original when any destination is missing
# or differs (e.g. a locally modified file).
for name in $names; do
    new="scripts/legacy/$name"
    old="scripts/$name"
    if [ ! -f "$new" ]; then
        echo "[ERROR] Missing destination: $new" >&2
        exit 1
    fi
    if [ -e "$old" ] && ! cmp -s "$old" "$new"; then
        echo "[ERROR] Different file contents: $old and $new" >&2
        echo "Keep both and merge manually; no files were deleted." >&2
        exit 1
    fi
done

for name in $names; do
    old="scripts/$name"
    if [ -e "$old" ]; then
        rm -- "$old"
        echo "[MOVED] $old -> scripts/legacy/$name"
    fi
done

echo '[PASS] Historical scripts archived safely (or already migrated).'
