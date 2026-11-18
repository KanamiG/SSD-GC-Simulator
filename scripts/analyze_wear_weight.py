"""SSD GC Simulator — Checkpoint 9F-1: 320-run utilization sensitivity.

Run from the project root:
    python scripts/analyze_wear_weight.py

Input:
    results/wear_weight_sensitivity.csv

Outputs:
    results/wear_weight_summary_by_utilization.csv
    results/wear_weight_paired_deltas.csv
    results/wear_weight_paired_ci95.csv
    results/wear_weight_pareto_by_utilization.csv
    results/wear_weight_plots/*.png
"""

from pathlib import Path

import numpy as np
import pandas as pd

import matplotlib
matplotlib.use("Agg")  # Save PNG files without opening a GUI window.
import matplotlib.pyplot as plt


INPUT = Path("results/wear_weight_sensitivity.csv")
OUT_DIR = Path("results")
PLOT_DIR = OUT_DIR / "wear_weight_plots"
LOGICAL_COUNTS = (400, 440, 464, 480)
WORKLOADS = ("Random", "HotCold")
SEEDS = (42, 43, 44, 45, 46)
LAMBDAS = (0.0, 0.01, 0.02, 0.05, 0.1, 0.2)
KEYS = ["logical_pages", "workload", "seed"]
GROUP_KEYS = ["logical_pages", "workload", "lambda"]
METRICS = ["waf", "erase_cv", "max_erases"]
T_CRITICAL = 2.7764451052  # two-sided 95%, df = 4 (n = 5)


def validate_input(df: pd.DataFrame) -> tuple[pd.DataFrame, pd.DataFrame]:
    expected_columns = {
        "logical_pages", "workload", "seed", "policy", "lambda",
        "attempted", "successful", "waf", "erase_cv", "max_erases",
        "gc_cycles", "total_erases", "success_rate",
    }
    missing = expected_columns - set(df.columns)
    assert not missing, f"Missing columns: {sorted(missing)}"
    assert len(df) == 320, f"Expected 320 rows, found {len(df)}"
    assert set(df["logical_pages"]) == set(LOGICAL_COUNTS)
    assert set(df["workload"]) == set(WORKLOADS)
    assert set(df["seed"]) == set(SEEDS)
    assert not df.duplicated(KEYS + ["policy", "lambda"]).any()
    assert df.groupby(KEYS).size().eq(8).all()
    assert (df["attempted"] == 10_000).all()
    assert (df["successful"] <= df["attempted"]).all()
    assert (df["successful"] >= 0).all()
    assert (df["gc_cycles"] == df["total_erases"]).all()
    assert df[METRICS].notna().all().all(), "Unexpected missing metrics"

    # There must be exactly one of each policy/weight in each matched workload.
    expected_policy_pairs = {
        ("Greedy", 0.0), ("FIFO", 0.0),
        *(("WearAwareGreedy", lam) for lam in LAMBDAS),
    }
    for _, group in df.groupby(KEYS):
        assert set(zip(group["policy"], group["lambda"])) == expected_policy_pairs

    greedy = (df[df["policy"] == "Greedy"]
              .set_index(KEYS).sort_index())
    wear0 = (df[(df["policy"] == "WearAwareGreedy") &
                (df["lambda"] == 0.0)]
             .set_index(KEYS).sort_index())
    assert greedy.index.equals(wear0.index)
    for metric in ["successful", "waf", "gc_cycles", "total_erases",
                   "max_erases", "erase_cv"]:
        np.testing.assert_allclose(
            greedy[metric].to_numpy(), wear0[metric].to_numpy(),
            rtol=1e-12, atol=1e-12,
            err_msg=f"Greedy vs lambda=0 mismatch: {metric}",
        )

    print("[PASS] 320-row CSV / 40 matched workload groups")
    print("[PASS] Lambda=0 equals Greedy for every logical_pages/workload/seed")
    return greedy, df[df["policy"] == "WearAwareGreedy"].copy()


def make_summary(wear: pd.DataFrame) -> pd.DataFrame:
    # IMPORTANT: seed must not be a group key. Each group contains five seeds.
    counts = wear.groupby(GROUP_KEYS).size()
    assert len(counts) == 48 and counts.eq(5).all()
    summary = wear.groupby(GROUP_KEYS).agg(
        mean_waf=("waf", "mean"),
        std_waf=("waf", "std"),
        mean_cv=("erase_cv", "mean"),
        std_cv=("erase_cv", "std"),
        mean_max_erases=("max_erases", "mean"),
        mean_gc_cycles=("gc_cycles", "mean"),
    )
    return summary


def make_paired(wear: pd.DataFrame, greedy: pd.DataFrame) -> pd.DataFrame:
    paired = wear.merge(
        greedy[METRICS].reset_index(),
        on=KEYS, how="left", suffixes=("", "_greedy"),
        validate="many_to_one",
    )
    assert len(paired) == 240
    assert paired[[f"{m}_greedy" for m in METRICS]].notna().all().all()
    paired["delta_waf"] = paired["waf"] - paired["waf_greedy"]
    paired["delta_cv"] = paired["erase_cv"] - paired["erase_cv_greedy"]
    paired["delta_max_erases"] = (
        paired["max_erases"] - paired["max_erases_greedy"]
    )
    return paired


def make_comparison(paired: pd.DataFrame) -> pd.DataFrame:
    return paired.groupby(GROUP_KEYS).agg(
        mean_delta_waf=("delta_waf", "mean"),
        mean_delta_cv=("delta_cv", "mean"),
        mean_delta_max_erases=("delta_max_erases", "mean"),
    )
def make_ci(paired: pd.DataFrame) -> pd.DataFrame:
    rows = []
    for (logical_pages, workload, lam), group in paired.groupby(GROUP_KEYS):
        for metric in ("delta_waf", "delta_cv", "delta_max_erases"):
            values = group[metric].to_numpy(dtype=float)
            assert len(values) == 5
            mean = float(np.mean(values))
            sd = float(np.std(values, ddof=1))
            margin = T_CRITICAL * sd / np.sqrt(5)
            rows.append({
                "logical_pages": logical_pages,
                "workload": workload,
                "lambda": lam,
                "metric": metric,
                "mean_delta": mean,
                "ci_lower": mean - margin,
                "ci_upper": mean + margin,
                "improved_seeds": int(np.sum(values < 0)),
            })
    ci = pd.DataFrame(rows)
    assert len(ci) == 144
    return ci


def dominates(a: pd.Series, b: pd.Series) -> bool:
    # All three metrics are minimized; ties alone do not dominate.
    return all(a[m] <= b[m] for m in METRICS) and any(
        a[m] < b[m] for m in METRICS
    )


def make_pareto(df: pd.DataFrame) -> pd.DataFrame:
    candidates = df.groupby(
        ["logical_pages", "workload", "policy", "lambda"],
        as_index=False,

    ).agg(
        waf=("waf", "mean"),
        erase_cv=("erase_cv", "mean"),
        max_erases=("max_erases", "mean"),
    )
    assert len(candidates) == 64  # 4 logical counts × 2 workloads × 8 configs
    result = []
    # NEVER compare points from different capacities or workload types.
    for _, group in candidates.groupby(["logical_pages", "workload"]):
        for i, candidate in group.iterrows():
            if not any(i != j and dominates(other, candidate)
                       for j, other in group.iterrows()):
                result.append(candidate.to_dict())
    assert result, "Pareto frontier unexpectedly empty"
    return pd.DataFrame(result).sort_values(
        ["logical_pages", "workload", "waf", "lambda"]
    ).reset_index(drop=True)


def check_lambda_equivalence(wear: pd.DataFrame) -> None:
    random_subset = wear[
        (wear["workload"] == "Random") &
        (wear["lambda"].isin([0.01, 0.02, 0.05]))
    ]
    pivot = random_subset.pivot(
        index=["logical_pages", "seed"],
        columns="lambda",
        values=["waf", "erase_cv", "max_erases", "gc_cycles"],
    )
    print("\n=== Random lambda=0.01 vs 0.02/0.05 (exact CSV values) ===")
    for logical_pages in LOGICAL_COUNTS:
        for lam in (0.02, 0.05):
            equal = all(
                np.array_equal(
                    pivot.loc[logical_pages, (metric, 0.01)].to_numpy(),
                    pivot.loc[logical_pages, (metric, lam)].to_numpy(),
                )
                for metric in ("waf", "erase_cv", "max_erases", "gc_cycles")
            )
            print(f"logical_pages={logical_pages}, 0.01 vs {lam:.2f}: "
                  f"{'SAME' if equal else 'DIFFERENT'}")


def make_plots(df: pd.DataFrame, summary: pd.DataFrame) -> int:
    PLOT_DIR.mkdir(parents=True, exist_ok=True)
    count = 0
    for logical_pages in LOGICAL_COUNTS:
        for workload in WORKLOADS:
            subset = summary.loc[(logical_pages, workload)]
            mask = ((df["logical_pages"] == logical_pages) &
                    (df["workload"] == workload))
            greedy_waf = df.loc[mask & (df["policy"] == "Greedy"), "waf"].mean()
            fifo_waf = df.loc[mask & (df["policy"] == "FIFO"), "waf"].mean()
            fig, ax = plt.subplots(figsize=(8, 5))
            ax.errorbar(
                subset.index.to_numpy(), subset["mean_waf"].to_numpy(),
                yerr=subset["std_waf"].to_numpy(),
                marker="o", capsize=4, label="WearAwareGreedy (SD over 5 seeds)",
            )
            ax.axhline(greedy_waf, linestyle="--", label="Greedy mean")
            ax.axhline(fifo_waf, linestyle=":", label="FIFO mean")
            ax.set(xlabel="Wear weight (lambda)", ylabel="WAF",
                   title=f"{workload}: WAF sensitivity — logical_pages={logical_pages}")
            ax.legend()
            ax.grid(alpha=0.25)
            fig.tight_layout()
            fig.savefig(PLOT_DIR / f"waf_{workload}_{logical_pages}.png", dpi=160)
            plt.close(fig)
            count += 1

    # Two overview plots: show how WAF changes with logical address-space size.
    for workload in WORKLOADS:
        fig, ax = plt.subplots(figsize=(8, 5))
        workload_df = df[df["workload"] == workload]
        for name, policy, lam in (
            ("Greedy", "Greedy", 0.0),
            ("WearAware lambda=0.01", "WearAwareGreedy", 0.01),
            ("WearAware lambda=0.20", "WearAwareGreedy", 0.2),
            ("FIFO", "FIFO", 0.0),
        ):
            data = workload_df[
                (workload_df["policy"] == policy) &
                (workload_df["lambda"] == lam)
            ].groupby("logical_pages")["waf"].mean()
            ax.plot(data.index.to_numpy(), data.to_numpy(), marker="o", label=name)
        ax.set(xlabel="Logical page count", ylabel="Mean WAF (5 seeds)",
               title=f"{workload}: WAF across logical address-space sizes")
        ax.legend()
        ax.grid(alpha=0.25)
        fig.tight_layout()
        fig.savefig(PLOT_DIR / f"waf_utilization_{workload}.png", dpi=160)
        plt.close(fig)
        count += 1
    assert count == 10
    return count


def main() -> None:
    df = pd.read_csv(INPUT)
    greedy, wear = validate_input(df)
    summary = make_summary(wear)
    paired = make_paired(wear, greedy)
    comparison = make_comparison(paired)
    ci = make_ci(paired)
    pareto = make_pareto(df)

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    summary.to_csv(OUT_DIR / "wear_weight_summary_by_utilization.csv")
    comparison.to_csv(OUT_DIR / "wear_weight_paired_deltas.csv")
    ci.to_csv(OUT_DIR / "wear_weight_paired_ci95.csv", index=False)
    pareto.to_csv(OUT_DIR / "wear_weight_pareto_by_utilization.csv", index=False)

    print("\n=== Wear Weight Sensitivity (five-seed statistics) ===")
    print(summary.round(4).to_string())
    print("\n=== Paired differences vs Greedy ===")
    print(comparison.round(5).to_string())
    print("\n=== Paired 95% CI: lambda=0.01 (other rows in CSV) ===")
    print(ci.loc[ci["lambda"] == 0.01].round(5).to_string(index=False))
    print("\n=== Pareto Frontier (within each logical_pages/workload) ===")
    print(pareto.round(5).to_string(index=False))
    check_lambda_equivalence(wear)
    n_figures = make_plots(df, summary)

    print(f"\n[PASS] {len(summary)} summary groups; {len(paired)} paired runs; "
          f"{len(ci)} paired CIs")
    print(f"[PASS] Analysis CSV files saved to {OUT_DIR}/")
    print(f"[PASS] {n_figures} PNG plots saved to {PLOT_DIR}/")
    print("Note: confidence intervals are exploratory (five seeds, no multiple-testing correction).")


if __name__ == "__main__":
    main()
