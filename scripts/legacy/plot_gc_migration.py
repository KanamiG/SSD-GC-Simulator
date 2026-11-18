
from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("gc_event_trace.csv")

# 先比較高使用率下的 Random / HotCold
high_util = df[
    df["logical_pages"] == 480
]

Path("results").mkdir(exist_ok=True)

for policy in ["Greedy", "FIFO"]:

    fig, ax = plt.subplots(figsize=(9, 5))

    for workload in ["Random", "HotCold"]:

        subset = high_util[
            (high_util["policy"] == policy) &
            (high_util["workload"] == workload)
        ]

        counts = subset["migrated_pages"].value_counts().reindex(range(17), fill_value=0)

        relative_frequency = counts / len(subset)

        ax.plot(
            relative_frequency.index,
            relative_frequency.values,
            marker="o",
            label=workload
        )

    ax.set_title(
        f"GC Migration Distribution - {policy}"
    )
    ax.set_xlabel("Migrated Pages per GC")
    ax.set_ylabel("Relative Frequency")
    ax.set_xticks(range(17))
    ax.legend()
    ax.grid(alpha=0.25)

    fig.tight_layout()
    fig.savefig(
        f"results/gc_migration_{policy.lower()}.png",
        dpi=200
    )

plt.show()
