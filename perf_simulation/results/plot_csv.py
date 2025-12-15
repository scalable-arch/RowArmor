import pandas as pd
import numpy as np
import argparse
from scipy.stats import gmean

# -----------------------------
# Argument parsing
# -----------------------------
parser = argparse.ArgumentParser(description="Normalize CSV by baseline and compute GEOMEAN.")
parser.add_argument("-input", type=str, default="per_core_simulation_results.csv",
                    help="Input CSV file (default = per_core_simulation_results.csv)")
args = parser.parse_args()

INPUT = args.input
OUTPUT = "plot.csv"

print(f"[INFO] Reading input file: {INPUT}")

# -----------------------------
# 1. Load CSV
# -----------------------------
df = pd.read_csv(INPUT)

# Detect core columns
core_cols = [c for c in df.columns if c.startswith("core")]

# -----------------------------
# 2. Compute sum of all cores
# -----------------------------
df["sum_core"] = df[core_cols].sum(axis=1)

# -----------------------------
# 3. classify run_mode into 3 categories
# -----------------------------
def classify_mode(row):
    if row["run_mode"] == "rate":
        return "rate"

    if row["run_mode"] == "mix":
        if row["run_base"] == "mix.high":
            return "mix_high"
        else:
            return "mix"

    return row["run_mode"]

df["mode3"] = df.apply(classify_mode, axis=1)

# -----------------------------
# 4. Extract baseline rows
# -----------------------------
df_base = df[df["method"] == "baseline"]

baseline_dict = {row["run_base"]: row["sum_core"] for _, row in df_base.iterrows()}

# -----------------------------
# 5. Normalize using sum_core / baseline_sum_core
# -----------------------------
norm_rows = []

for idx, row in df.iterrows():
    if row["method"] == "baseline":
        continue

    rb = row["run_base"]

    if rb not in baseline_dict:
        continue

    base_sum = baseline_dict[rb]

    if base_sum == 0:
        norm_val = np.nan
    else:
        norm_val = row["sum_core"] / base_sum

    norm_rows.append({
        "method": row["method"],
        "mode3": row["mode3"],
        "config": row["config"],
        "run_base": rb,
        "normalized_sum": norm_val
    })

df_norm = pd.DataFrame(norm_rows)

# -----------------------------
# 6. GEOMEAN by (method, mode3, config)
# -----------------------------
group_cols = ["method", "mode3", "config"]
result_rows = []

for group, gdf in df_norm.groupby(group_cols):
    vals = gdf["normalized_sum"].dropna()
    gm = gmean(vals) if len(vals) > 0 else np.nan

    result_rows.append({
        "method": group[0],
        "mode3": group[1],
        "config": group[2],
        "geomean_normalized_sum": gm
    })

df_out = pd.DataFrame(result_rows)

# -----------------------------
# 7. Save output CSV
# -----------------------------
df_out.to_csv(OUTPUT, index=False)
print(f"[INFO] Saved output file: {OUTPUT}")
