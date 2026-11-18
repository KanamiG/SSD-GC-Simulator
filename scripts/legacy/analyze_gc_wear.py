
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

Path("results").mkdir(exist_ok=True)

df = pd.read_csv("gc_event_trace.csv")

block_count = 32

high_util = df[
    df["logical_pages"] == 480
]

wear_rows = []

for (workload, policy), subset in high_util.groupby(
    ["workload", "policy"]
):

    erase_counts = (
        subset["victim_block"]
        .value_counts()
        .reindex(range(block_count), fill_value=0)
        .sort_index()
    )

    assert erase_counts.sum() == len(subset)

    mean_erases = erase_counts.sum() / erase_counts.size
    std_erases = np.std(erase_counts, ddof=0)

    max_erases = max(erase_counts)
    min_erases = min(erase_counts)

    cv_erases = (
        std_erases / mean_erases
        if mean_erases > 0
        else float("nan")
    )

    wear_rows.append({
        "workload": workload,
        "policy": policy,
        "total_erases": int(erase_counts.sum()),
        "mean_erases": mean_erases,
        "std_erases": std_erases,
        "cv_erases": cv_erases,
        "max_erases": max_erases
    })

    print(f"\n{workload} / {policy}")
    print(f"Total GC: {len(subset)}")
    print(f"Mean Erases: {mean_erases:.3f}")
    print(f"Std Erases: {std_erases:.3f}")
    print(f"Max Erases: {max_erases}")
    print(f"Min Erases: {min_erases}")
    print(f"CV Erases: {cv_erases:.3f}")


    fig, ax = plt.subplots(figsize=(10, 4))

    ax.bar(
        erase_counts.index,
        erase_counts.values
    )

    ax.set_title(f"{workload} / {policy} - Block Erases")
    ax.set_xlabel("Physical Block Index")
    ax.set_ylabel("Erase Count")
    ax.set_xticks(range(0, block_count, 2))
    ax.grid(axis="y", alpha=0.2)

    fig.tight_layout()
    plt.show()

wear_df = pd.DataFrame(wear_rows)

benchmark = pd.read_csv(
    "utilization_comparison.csv"
)

benchmark = benchmark[
    (benchmark["logical_pages"] == 480) &
    (benchmark["seed"] == 42)
]

benchmark = benchmark[
    ["workload", "policy", "waf", "success_rate"]
]

tradeoff = wear_df.merge(
    benchmark,
    on=["workload", "policy"],
    validate="one_to_one"
)

assert len(tradeoff) == 4
assert (tradeoff["success_rate"] == 1.0).all()

print(tradeoff.round(4).to_string(index=False))

fig, ax = plt.subplots(figsize=(8, 5))

for _, row in tradeoff.iterrows():

    x = row["waf"]

    # Y = Erase CV (百分比)
    y = row["cv_erases"] * 100


    ax.scatter(
        x,  # WAF
        y,  # CV * 100
        s=80
    )

    label = f'{row["workload"]} / {row["policy"]}'

    ax.annotate(
        label,
        xy=(x, y),
        xytext=(8, 8),
        textcoords="offset points",
        fontsize=9
    )


ax.set_xlabel("Write Amplification Factor")
ax.set_ylabel("Erase Count CV (%)")
ax.set_title("GC Policy: WAF vs Wear Distribution")

ax.grid(alpha=0.25)
fig.tight_layout()

plt.show()