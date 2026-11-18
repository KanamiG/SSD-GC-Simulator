
import pandas as pd

df = pd.read_csv("policy_comparison.csv")

# 驗證每組實驗都有兩種 GC Policy
assert len(df) == 30

assert not df.duplicated(
    ["workload", "seed", "policy"]
).any()

assert (df["success_rate"] == 1.0).all()

# 建立配對資料
comparison = df.pivot(
    index=["workload", "seed"],
    columns="policy",
    values="waf"
)

assert comparison[["Greedy", "FIFO"]].notna().all().all()

comparison["delta_waf"] = (
    comparison["FIFO"] -
    comparison["Greedy"]
)

# Sequential 是確定性 Workload
random_experiments = comparison.loc[
    comparison.index.get_level_values(
        "workload"
    ) != "Sequential"
]

summary = random_experiments.groupby(
    level="workload"
).agg(
    n=("delta_waf", "count"),
    mean_delta=("delta_waf", "mean"),
    std_delta=("delta_waf", "std")
)

summary["se_delta"] = summary["std_delta"] / summary["n"].pow(0.5)


t_critical = 2.7764451051977987

summary["ci95_lower"] = summary["mean_delta"] - t_critical * summary["se_delta"]
summary["ci95_upper"] = summary["mean_delta"] + t_critical * summary["se_delta"]


assert (summary["ci95_lower"] > 0).all()

print("[PASS] Both confidence intervals are positive")

print("\n=== Paired 95% Confidence Intervals ===")
print(summary.round(6).to_string())

print("\n=== Positive Delta Counts ===")
print(
    comparison.groupby(level="workload")[
        "delta_waf"
    ].apply(lambda x: (x > 0).sum())
)