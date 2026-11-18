
import pandas as pd

df = pd.read_csv("policy_comparison.csv")

assert len(df) == 30

assert (
    df.groupby(["workload", "seed"])["policy"]
      .nunique()
      .eq(2)
      .all()
)

# 確保沒有重複的實驗組合
assert not df.duplicated(
    ["workload", "seed", "policy"]
).any()

mask = df["host_writes"] > 0

expected_waf = (
    df.loc[mask, "physical_programs"] /
    df.loc[mask, "host_writes"] 
)

actual_waf = df.loc[mask, "waf"]

assert (
    (actual_waf - expected_waf).abs() <= 1e-9
).all()

assert df.loc[~mask, "waf"].isna().all()

comparison = df.pivot(
    index=["workload", "seed"],
    columns="policy",
    values="waf"
)

comparison["delta_waf"] = comparison["FIFO"] - comparison["Greedy"]

print("=== Paired Comparison ===")
print(comparison.round(6).to_string())

summary = comparison.groupby(
    level="workload",
    sort=False
).agg(
    mean_greedy=("Greedy", "mean"),
    mean_fifo=("FIFO", "mean"),
    mean_delta_waf=("delta_waf", "mean"),
    mean_std_waf=("delta_waf", "std"),
    n=("delta_waf", "count")
)

print("\n=== Policy Summary ===")
print(summary.round(6).to_string())

metrics = df.groupby(
    ["workload", "policy"]
).agg(
    mean_gc_writes=("gc_writes", "mean"),
    mean_gc_cycles=("gc_cycles", "mean"),
    mean_success_rate=("success_rate", "mean")
)

print("\n=== GC Metrics ===")
print(metrics.round(6).to_string())