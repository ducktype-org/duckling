#!/usr/bin/env python3
import json, os, sys
import matplotlib.pyplot as plt
import pandas as pd
import seaborn

ROW_LABELS = {"calc": "obliczenie początkowe", "update": "obliczenie + aktualizacja"}
COLUMN_LABEL = "wątki={}"
X_LABEL = "jednostki symulowanej pracy"
Y_LABEL = "% zaktualizowanych wejść"
CBAR_LABEL = "\nśredni czas Query / średni czas PSAC"
# Modes where the update percentage does not influence the measurement, so all
# its rows carry the same value and are collapsed into one.
COLLAPSE_UP_MODES = {"calc"}
COLLAPSED_UP_LABEL = "n/d"

try:
    path = sys.argv[1]
    print(f"loading {path}")
    d = json.load(open(path))
except IndexError:
    print(f"usage: {sys.argv[0]} <path.json> [ext=png]")
    exit(1)

TITLE = os.environ.get("PLOT_TITLE", path)

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
height_ratios = [1 if m in COLLAPSE_UP_MODES else 2 for m in modes]
try:
    subfigs = fig.subfigures(len(modes), 1, height_ratios=height_ratios)
except TypeError:  # matplotlib older than 3.6
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
        sub = piv[(piv["mode"] == mode) & (piv.threads == t)]
        if mode in COLLAPSE_UP_MODES:
            sub = sub.assign(up=COLLAPSED_UP_LABEL)
        g = sub.pivot_table(
            index="up",
            columns="workload",
            values=["query", "psac"],
            aggfunc="mean",
        )
        g = pd.concat(
            {"query": g["query"], "psac": g["psac"], "ratio": g["query"] / g["psac"]},
            axis=1,
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
        ax.set_ylabel("" if mode in COLLAPSE_UP_MODES else Y_LABEL)

        print(f"mode={mode} threads={t}")

ext = sys.argv[2] if len(sys.argv) > 2 else "png"
res = sys.argv[1].replace(".json", f".{ext}")
print(f"writing {res}")
plt.savefig(res)
plt.show()
