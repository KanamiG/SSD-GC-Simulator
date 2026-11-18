import pandas as pd

df = pd.read_csv(
    "benchmark_results_multiseed.csv"
)

assert len(df) == 15
assert df.groupby("workload").size().eq(5).all()
assert df["seed"].nunique() == 5

print("[PASS] Multi-seed experiments")