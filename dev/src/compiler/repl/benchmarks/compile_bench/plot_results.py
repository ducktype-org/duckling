#!/usr/bin/env python3
"""
Plot the benchmark CSV produced by `compile_bench.py`.

The output is a single PNG with two subplots: `unique` and `repeated`.
Each subplot compares the four compilation scenarios across the tested
function counts and saves the figure as `bench_results_combined.png`.

Requires matplotlib.
"""

import argparse
import csv
from collections import defaultdict
from pathlib import Path


def load_results(csv_path: Path):
    data = defaultdict(lambda: defaultdict(dict))

    with csv_path.open(encoding="ascii") as f:
        r = csv.DictReader(f)
        for row in r:
            try:
                n = int(row.get("function_count", row.get("functions", "0")))
            except Exception:
                n = int(row.get("function_count", 0))
            gen = row.get("generator", "")
            scen = row.get("scenario", "")
            try:
                avg = float(row.get("avg_s", row.get("avg", 0.0)))
            except Exception:
                avg = float(row.get("avg_s", 0.0))
            data[scen][gen][n] = avg
    return data


def plot_combined(data, out_dir: Path, plt):

    # Create a single combined PNG with two subplots: one for `unique` and one
    # for `repeated`. Each subplot shows the four scenario lines (if present).
    gens_to_plot = ["unique", "repeated"]
    scenarios = sorted(data.keys())
    if not scenarios:
        print("No data to plot")
        return

    fig, axes = plt.subplots(2, 1, figsize=(10, 8), sharex=True)

    # Color cycle for consistent coloring across subplots
    colors = plt.rcParams["axes.prop_cycle"].by_key().get("color", None)
    all_function_counts = sorted(
        {
            function_count
            for scenario_name in scenarios
            for generator_name in data[scenario_name].keys()
            for function_count in data[scenario_name][generator_name].keys()
        }
    )

    for ax, gen in zip(axes, gens_to_plot):
        for i, scen in enumerate(scenarios):
            points = data.get(scen, {}).get(gen, {})
            if not points:
                continue
            items = sorted(points.items())
            xs = [x for x, _ in items]
            ys = [y for _, y in items]
            color = colors[i] if colors and i < len(colors) else None
            ax.plot(xs, ys, marker="o", label=scen, color=color)

        ax.set_title(f"Generator: {gen}")
        ax.set_ylabel("avg time (s)")
        ax.grid(True, linestyle="--", alpha=0.4)
        ax.legend()
        ax.set_xticks(all_function_counts)
        ax.set_xticklabels([str(value) for value in all_function_counts])
        ax.tick_params(axis="x", labelrotation=30)

    axes[-1].set_xlabel("number of functions")
    plt.tight_layout()
    out_path = out_dir / "bench_results_combined.png"
    plt.savefig(out_path)
    print("Saved:", out_path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--csv",
        type=Path,
        default=Path(__file__).parent / "results" / "bench_results.csv",
    )
    args = parser.parse_args()

    if not args.csv.exists():
        print("CSV not found:", args.csv)
        raise SystemExit(1)

    try:
        data = load_results(args.csv)
    except Exception as exc:
        print("Failed to read CSV:", exc)
        raise SystemExit(1)

    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        print(
            "matplotlib is required to plot results. Install it and rerun the script."
        )
        raise SystemExit(1) from exc

    out_dir = args.csv.parent
    plot_combined(data, out_dir, plt)
