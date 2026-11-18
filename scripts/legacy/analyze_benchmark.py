
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("benchmark_results.csv")

df["avg_gc_migration"] = df["gc_writes"] / df["gc_cycles"].replace(0, float("nan"))

columns = [
    "workload",
    "waf",
    "success_rate",
    "gc_writes",
    "gc_cycles",
    "block_erases",
    "avg_gc_migration",
]

print(df[columns].to_string(index=False))

assert (
    df["attempted"] ==
    df["successful"] + df["failed"]
).all()

assert (
    df["host_writes"] == df["successful"]
).all()

# Host Writes > 0 時 WAF 才有定義
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

fig, ax = plt.subplots(figsize=(8, 5))

bars = ax.bar(df["workload"], df["waf"])

ax.set_title("SSD Garbage Collection - Write Amplification")
ax.set_xlabel("Workload")
ax.set_ylabel("Write Amplification Factor")

ax.bar_label(bars,
             fmt="%.4f",
             padding=3)

ax.grid(axis="y", alpha=0.25)

Path("results").mkdir(exist_ok=True)

fig.tight_layout()
fig.savefig(
    "results/waf_comparison.png",
    dpi=200
)

plt.show()