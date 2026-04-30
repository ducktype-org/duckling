#!/usr/bin/env python3
import json, os, sys
import matplotlib.pyplot as plt
import pandas as pd
import seaborn

ROW_LABELS = {"calc": "initial calculation", "update": "initial + update"}
COLUMN_LABEL = "threads={}"
X_LABEL = "workload units"
Y_LABEL = "% of updates"
CBAR_LABEL = "\nmean Query / mean PSAC"

try:
    path = sys.argv[1]
    print(f"loading {path}")
    d = json.load(open(path))
except IndexError:
    print(f"usage: {sys.argv[0]} <path.json>")
    exit(1)

TITLE = path

print("building dataframe")
df = pd.DataFrame(
    [
        {
            "framework": "psac"
                if "psac" in os.path.basename(r["command"])
                else "query",
            "mean_time": r["mean"],
            **r["parameters"],
        }
        for r in d["results"]
    ]
)
df[["threads", "up", "workload"]] = df[["threads", "up", "workload"]].astype(int)

print("building pivot table")
piv = df.pivot_table(
    index=["mode", "threads", "up", "workload"],
    columns="framework",
    values="mean_time",
).reset_index()
piv["ratio"] = piv["query"] / piv["psac"]

modes = ["calc", "update"]
threads = sorted(piv.threads.unique())

print("plotting data")
fig = plt.figure(
    layout="compressed",
    figsize=(4 * len(threads), 4 * len(modes)),  # (W, H)
)
fig.suptitle(TITLE, fontsize=16, y=0.97)
fig.get_layout_engine().set(w_pad=0.2, h_pad=0.2, rect=(0, 0, 0.95, 0.95))
subfigs = fig.subfigures(len(modes), 1)

for i, (mode, sf) in enumerate(zip(modes, subfigs)):
    sf.supylabel(ROW_LABELS[mode])
    axes_row = sf.subplots(1, len(threads))
    for j, t in enumerate(threads):
        ax = axes_row[j]
        if i == 0:
            ax.set_title(COLUMN_LABEL.format(t), pad=14)
        else:
            ax.set_title(" ")
        g = piv[(piv["mode"] == mode) & (piv.threads == t)].pivot(
            index="up",
            columns="workload",
            values=["ratio", "query", "psac"],
        )

        labels = (
            g["ratio"].map("{:.2f}".format)
            + "\n"
            + g["query"].map("{:.2f}".format)
            + "/"
            + g["psac"].map("{:.2f}".format)
        )

        seaborn.heatmap(
            g["ratio"],
            annot=labels,
            fmt="",  # formatting is done above
            cmap="RdBu_r",
            center=1,
            vmin=0,
            vmax=piv.ratio.max(),
            ax=ax,
            cbar=(j == len(threads) - 1),
            cbar_kws={"label": CBAR_LABEL, "pad": 0.2},
        )
        ax.set_xlabel(X_LABEL)
        ax.set_ylabel(Y_LABEL)  # if j == 0 else "")

        print(f"mode={mode} threads={t}")

res = sys.argv[1].replace(".json", ".png")
print(f"writing {res}")
plt.savefig(res)
plt.show()
