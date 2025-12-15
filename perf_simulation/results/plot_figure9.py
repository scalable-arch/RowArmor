#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# -----------------------------
# Plot style
# -----------------------------
plt.rcParams.update({
    "font.size": 12,
    "axes.labelsize": 13,
    "axes.titlesize": 14,
    "legend.fontsize": 11,
    "lines.linewidth": 2,
})

scheme_style = {
    "para":        ("#e69f00", "^"),
    "srs":         ("#a65628", "o"),
    "graphene":    ("#999999", "x"),
    "abacus":      ("#fdb462", "*"),
    "prac":        ("#dede00", "X"),
    "cube_para":   ("#56b4e9", "v"),
    "cube_srs":    ("#0072b2", "D"),
    "rampart":     ("#e377c2", "s"),
    "rowarmor":    ("#228b22", "P"),
}

# -----------------------------
# Workload / axis definition
# -----------------------------
workload_map = {
    "mix": "Rate",
    "mix_high": "Mix-High",
    "mix_rand": "Mix-Rand",
}

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
df = pd.read_csv("input.csv")

df = df[df["config"].isin(x_values)]

# -----------------------------
# Plot
# -----------------------------
fig, axes = plt.subplots(1, 3, figsize=(16, 5), sharey=True)

for idx, (mode, title) in enumerate(workload_map.items()):
    ax = axes[idx]
    sub = df[df["mode3"] == mode]

    for method, (color, marker) in scheme_style.items():
        msub = sub[sub["method"] == method]
        if msub.empty:
            continue

        perf = []
        for v in x_values:
            val = msub[msub["config"] == v]["geomean_normalized_sum"]
            perf.append(float(val.iloc[0]) if not val.empty else np.nan)

        ax.plot(
            x_positions,
            perf,
            label=method.upper(),
            color=color,
            marker=marker
        )

    ax.set_title(title)
    ax.set_xticks(x_positions)
    ax.set_xticklabels(x_labels)
    ax.grid(True, linestyle="--", alpha=0.4)

    if idx == 1:
        ax.set_xlabel("RowHammer Threshold (N_RH)")

axes[0].set_ylabel("Normalized Performance")
axes[0].set_ylim(0.4, 1.05)

# -----------------------------
# Legend (top-center, paper-style)
# -----------------------------
fig.legend(
    handles=[
        plt.Line2D([], [], color=c, marker=m, linestyle='-', label=k.upper())
        for k, (c, m) in scheme_style.items()
    ],
    loc="upper center",
    bbox_to_anchor=(0.5, 1.20),
    ncol=5,
    frameon=False
)

plt.tight_layout(rect=[0, 0, 1, 0.95])
plt.savefig("rh_performance_final.png", dpi=300, bbox_inches="tight")
plt.show()
