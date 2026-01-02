#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

plt.rcParams.update({
    "font.size": 12,
    "axes.labelsize": 13,
    "axes.titlesize": 14,
    "legend.fontsize": 11,
    "lines.linewidth": 2,
})


scheme_style = {
    "para":        ("#e69f00", "^"),
    "para.cube":  ("#56b4e9", "v"),
    "srs":         ("#a65628", "o"),
    "srs.cube":   ("#0072b2", "D"),
    "graphene":    ("#999999", "x"),
    "abacus":      ("#fdb462", "*"),
    "prac4":       ("#dede00", "X"),
    "rampart":     ("#e377c2", "s"),
    "rowarmor":    ("#228b22", None), 
}

# -----------------------------
# Workloads / subplots
# -----------------------------
workload_map = {
    "rate": "Rate",
    "mix_high": "Mix-High",
    "mix": "Mix-Rand",
}

# -----------------------------
# X-axis (RowHammer threshold)
# -----------------------------
x_order = [
    ("2K",   2048),
    ("1K",   1024),
    ("512",   512),
    ("256",   256),
    ("128",   128),
]

x_labels = [x[0] for x in x_order]
x_values = [x[1] for x in x_order]
x_positions = np.arange(len(x_labels))

# -----------------------------
# Load CSV
# -----------------------------
df_all = pd.read_csv("plot.csv")

# RowArmor: threshold-independent (config = ooc)
rowarmor = df_all[df_all["method"] == "rowarmor"]

# Other schemes: numeric thresholds only
df_all["config_num"] = pd.to_numeric(df_all["config"], errors="coerce")

# df = df_all[df_all["config"].isin(x_values)]
df = df_all[df_all["config_num"].isin(x_values)]


# -----------------------------
# Plot
# -----------------------------
fig, axes = plt.subplots(1, 3, figsize=(16, 5), sharey=True)

for idx, (mode, title) in enumerate(workload_map.items()):
    ax = axes[idx]
    sub = df[df["mode3"] == mode]

    # ---- threshold-dependent schemes ----
    for method, (color, marker) in scheme_style.items():
        if method == "rowarmor":
            continue

        msub = sub[sub["method"] == method]
        if msub.empty:
            continue

        perf = []
        for v in x_values:
            # val = msub[msub["config"] == v]["geomean_normalized_sum"]
            val = msub[msub["config_num"] == v]["geomean_normalized_sum"]
            perf.append(float(val.iloc[0]) if not val.empty else np.nan)

        ax.plot(
            x_positions,
            perf,
            label=method.upper(),
            color=color,
            marker=marker,
            zorder=2,
        )

    
    ra = rowarmor[rowarmor["mode3"] == mode]
    if not ra.empty:
        y = float(ra["geomean_normalized_sum"].iloc[0])
        ax.axhline(
            y=y,
            color=scheme_style["rowarmor"][0],
            linestyle="--",
            linewidth=2.5,
            zorder=1,
        )

    # ---- Axes formatting ----
    ax.set_title(title)
    ax.set_xticks(x_positions)
    ax.set_xticklabels(x_labels)
    ax.grid(True, linestyle="--", alpha=0.4)

    if idx == 1:
        ax.set_xlabel("RowHammer Threshold ($N_{RH}$)")

# -----------------------------
# Y-axis
# -----------------------------
axes[0].set_ylabel("Normalized Performance")
axes[0].set_ylim(0.4, 1.05)

# -----------------------------
# Legend (top-center)
# -----------------------------
handles = [
    plt.Line2D([], [], color=c, marker=m, linestyle='-', label=k.upper())
    for k, (c, m) in scheme_style.items()
    if k != "rowarmor"
]

handles.append(
    plt.Line2D([], [], color=scheme_style["rowarmor"][0],
               linestyle="--", linewidth=2.5,
               label="ROWARMOR")
)

fig.legend(
    handles=handles,
    loc="upper center",
    bbox_to_anchor=(0.5, 1.20),
    ncol=5,
    frameon=False
)

# -----------------------------
# Save
# -----------------------------
plt.tight_layout(rect=[0, 0, 1, 0.95])
plt.savefig("perf_example_reulsts.pdf", dpi=300, bbox_inches="tight")
