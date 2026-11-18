
from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("utilization_comparison.csv")

summary = df.groupby(
    ["workload", "logical_pages", "policy"]
).agg(
    mean_waf=("waf", "mean"),
    std_waf=("waf", "std"),
    mean_success_rate=("success_rate", "mean")
).reset_index()

Path("results").mkdir(exist_ok=True)

for workload in ["Random", "HotCold"]:

    fig, ax = plt.subplots(figsize=(8, 5))

    for policy in ["Greedy", "FIFO"]:

        subset = summary[
            (summary["workload"] == workload) &
            (summary["policy"] == policy)
        ].sort_values("logical_pages")

        # 設定使用率百分比
        x = subset["logical_pages"] / 496 * 100

        y = subset["mean_waf"]
        yerr = subset["std_waf"]

        ax.errorbar(
            x, y,
            yerr,
            marker="o",
            capsize=4,
            label=policy
        )


    ax.set_title(f"{workload} - WAF vs Utilization")
    ax.set_xlabel("Configured Logical Utilization (%)")
    ax.set_ylabel("Write Amplification Factor")
    ax.set_xticks([100/496*100, 200/496*100,
                   350/496*100, 480/496*100])

    ax.legend()
    ax.grid(alpha=0.25)

    fig.tight_layout()
    fig.savefig(
        f"results/{workload.lower()}_utilization.png",
        dpi=200
    )

comparison = df.pivot(
    index=["workload", "logical_pages", "seed"],
    columns="policy",
    values="waf"
)

# 保證每組實驗同時具有兩種 Policy
assert comparison[["Greedy", "FIFO"]].notna().all().all()

comparison["delta_waf"] = (
    comparison["FIFO"] -
    comparison["Greedy"]
)

delta_summary = comparison.groupby(
    level=["workload", "logical_pages"]
)["delta_waf"].agg(
    mean="mean",
    std="std",
    n="count"
)

print("\n=== WAF Policy Difference ===")
print(delta_summary.round(6).to_string())

plt.show()