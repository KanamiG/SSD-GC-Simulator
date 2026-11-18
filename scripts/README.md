# Python analysis tools

Run commands **from the repository root**. Install the optional dependencies:

```bash
python -m pip install -r requirements-analysis.txt
```

## Maintained analysis

`analyze_wear_weight.py` analyzes the **320-run experiment** produced by
`experiment_wear_weight` (four logical address-space sizes × two workloads ×
five seeds × eight policy/weight configurations):

```bash
cmake -S . -B build-exp -DBUILD_TESTING=OFF -DSSD_BUILD_EXPERIMENTS=ON
cmake --build build-exp -j 4
./build-exp/experiment_wear_weight
python scripts/analyze_wear_weight.py
```

Input: `results/wear_weight_sensitivity.csv`. Outputs (ignored by Git):
`results/wear_weight_summary_by_utilization.csv`,
`results/wear_weight_paired_deltas.csv`,
`results/wear_weight_paired_ci95.csv`,
`results/wear_weight_pareto_by_utilization.csv`, and
10 PNG images in `results/wear_weight_plots/`.

This script intentionally validates a **specific experiment design**: 320 rows,
10,000 requested writes, seeds 42–46, logical page counts 400/440/464/480,
Random/HotCold workloads and eight policy/weight configurations. It is not a
universal analyzer for arbitrary `ssd_sim --csv` output.

Run the Python smoke/regression checks without compiling the C++ code:

```bash
python -m unittest discover -s tests -p 'test_analysis_*.py' -v
```

They use synthetic fixture data, **not** real SSD measurements.

## Historical scripts

The scripts under [`legacy/`](legacy/) are preserved for historical
experiments and require their original CSV schemas. They do not read the
26-column CLI CSV format. See [`legacy/README.md`](legacy/README.md).

They are kept for reproducibility, not supported as stable GitHub-facing APIs.
