
import pandas as pd

df = pd.read_csv("gc_event_trace.csv")

assert (
    df["migrated_pages"] ==
    df["valid_pages"]
).all()

assert (
    df["valid_pages"] +
    df["invalid_pages"] == 16
).all()

summary = df.groupby(
    ["workload", "logical_pages", "policy"]
).agg(
    gc_count=("gc_cycle", "count"),
    mean_migrated=("migrated_pages", "mean"),
    std_migrated=("migrated_pages", "std"),
    max_migrated=("migrated_pages", "max"),
    total_migrated=("migrated_pages", "sum")
)

print(summary.round(4).to_string())
