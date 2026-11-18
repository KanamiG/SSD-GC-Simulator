
import pandas as pd

df = pd.read_csv("utilization_comparison.csv")

assert len(df) == 80

# 檢查實驗組合不可重複
assert not df.duplicated(
    ["workload", "logical_pages", "seed", "policy"]
).any()

# 每組 Workload + Utilization + Seed
# 必須同時具有兩種 Policy
assert (
    df.groupby(
        ["workload", "logical_pages", "seed"]
    )["policy"].nunique() == 2
).all()

expected_utilization = (
    df["logical_pages"] / 496
)

assert (
    (df["utilization"] - expected_utilization)
    .abs() < 1e-8
).all()


summary = df.groupby(
    ["workload", "logical_pages", "policy"],
).agg(
    mean_waf=("waf", "mean"),
    mean_success_rate=("success_rate", "mean"),
    mean_gc_writes=("gc_writes", "mean"),
    mean_gc_cycles=("gc_cycles", "mean")
)

print(summary)
