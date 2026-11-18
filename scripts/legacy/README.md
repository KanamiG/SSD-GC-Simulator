# Legacy / historical analysis scripts

These scripts were written for early, fixed-format experiments. They have not
been consolidated into the maintained 320-run analyzer. They are **archived
without code changes**, so historical analyses can still be reproduced when
the matching CSV data is available.

**Run from the repository root**. Paths below are relative to that root.
The historical input files are generated datasets, normally not versioned.

| Script | Required input | Purpose / caveats |
|---|---|---|
| `analyze_benchmark.py` | `benchmark_results.csv` | Initial benchmark WAF and migration plot; opens a plot window |
| `analyze_multiseed.py` | `benchmark_results_multiseed.csv` | 15-run workload summary and t intervals |
| `verif_multiseed.py` | `benchmark_results_multiseed.csv` | Short verification, redundant with parts of `analyze_multiseed.py` |
| `analyze_policy.py` | `policy_comparison.csv` | Greedy versus FIFO paired summary |
| `analyze_paired_ci.py` | `policy_comparison.csv` | Paired confidence intervals, asserts a positive FIFO-minus-Greedy result specific to the original experiment |
| `analyze_utilization.py` | `utilization_comparison.csv` | Old two-policy utilization experiment; its utilization field is an address-range ratio |
| `plot_utilization.py` | `utilization_comparison.csv` | Plots old utilization comparison; opens a plot window |
| `analyze_gc_trace.py` | `gc_event_trace.csv` | Migration counts and summary, assumes a 16-page block |
| `analyze_gc_wear.py` | `gc_event_trace.csv` **and** `utilization_comparison.csv` | Wear histograms, assumes 32 blocks and 480 logical pages; opens plot windows |
| `plot_gc_migration.py` | `gc_event_trace.csv` | Migration distribution plot, assumes 16-page blocks; opens plot window |

Example:

```bash
python scripts/legacy/analyze_policy.py
```

All scripts depend on some subset of NumPy, Pandas and Matplotlib; install
`requirements-analysis.txt`. Some make strong assertions about experiment
sizes and outcomes, and will intentionally fail when run with unrelated CSVs.

**Do not feed these scripts `results/cli_runs.csv`.** It has a different schema.
If you no longer need historical analysis, these scripts can eventually be
removed after their results and provenance are documented.
