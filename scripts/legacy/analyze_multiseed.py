
from pathlib import Path

import pandas as pd

df = pd.read_csv(
    "benchmark_results_multiseed.csv"
)

# 1. 驗證 15 組實驗
assert len(df) == 15

expected_workloads = {
    "Sequential",
    "Random",
    "HotCold"
}

expected_seeds = {42, 43, 44, 45, 46}

expected_pairs = {
    (kind, seed)
    for kind in expected_workloads
    for seed in expected_seeds
}

actual_pairs = set(
    zip(df["workload"], df["seed"])
)

assert actual_pairs == expected_pairs

# 2. 驗證 WAF
assert (df["host_writes"] > 0).all()

expected_waf = (
    df["physical_programs"] /
    df["host_writes"]
)

assert (
    (df["waf"] - expected_waf).abs() <= 1e-9
).all()

# 3. GroupBy 統計
summary = df.groupby(
    "workload",
    sort=False
).agg(
    n=("waf", "count"),
    mean_waf=("waf", "mean"),
    std_waf=("waf", "std"),
    mean_gc_writes=("gc_writes", "mean"),
    mean_gc_cycles=("gc_cycles", "mean"),
    mean_success_rate=("success_rate", "mean")
)

summary["se_waf"] = summary["std_waf"] / summary["n"].pow(0.5)


t_critical = 2.7764451051977987

summary["ci95_lower"] = summary["mean_waf"] - t_critical * summary["se_waf"]
summary["ci95_upper"] = summary["mean_waf"] + t_critical * summary["se_waf"]


# 6. Sequential 是確定性序列，
# 不提供由重複 Seeds 推算的信賴區間
summary.loc[
    "Sequential",
    ["ci95_lower", "ci95_upper"]
] = float("nan")

# 7. 顯示結果
print(summary.round(6).to_string())

Path("results").mkdir(exist_ok=True)

summary.to_csv(
    "results/multiseed_summary.csv"
)

print("[PASS] Multi-seed statistical analysis")
