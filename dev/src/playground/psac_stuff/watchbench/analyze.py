#!/usr/bin/env python3
"""
analyze.py - statistics and plots for watchbench.py results.

Usage:
    python3 analyze.py results.jsonl [more.jsonl ...] --reference psacklinger --out-dir analysis

Needs numpy; matplotlib only for plots (--no-plots skips them).

Data model:
  * Every "meta" record starts a RUN. A run's workload label is config["workload"] (or the
    project directory name) and its optional numeric size is config["size"]; add both keys
    to config.json when one run per program size is made.
  * A SESSION is (run, compiler, session index). Sessions with a session_abort record or
    any timed-out trial are dropped whole. Warm-up trials are dropped (--include-warmup
    keeps them).
  * A CELL is workload x compiler x edit x kind (edit | revert).
  * Compiler names ending in -j<N> are split into engine + thread count; ratios compare
    each engine with the reference engine at the same thread count.

Statistics:
  * Hierarchical bootstrap, B resamples: draw sessions with replacement, then the trials of
    each drawn session with replacement; the statistic is computed on the pooled draw.
    95% percentile intervals.
  * Ratios pair sessions by (run, session index): session i ran the same edit sequence for
    every compiler, so both sides are drawn with the same session indices.
  * p95 is only reported for cells with >= 100 trials.
  * Variance components on log latency (between vs within sessions) and the cost-optimal
    number of measured iterations per session for two-stage sampling (Kalibera & Jones).
  * Drift: per-session OLS slope of normalised latency and of RSS over the iteration index.

Outputs (in --out-dir): cells.csv, ratios.csv, initial.csv, variance.csv, drift.csv,
scaling.csv and the plots; a markdown summary goes to stdout and to summary.md.
"""

import argparse
import csv
import json
import math
import re
import sys
from collections import defaultdict
from pathlib import Path

import numpy as np

KINDS = ("edit", "revert")
CELL_METRICS = [
    ("latency_ms", "median"),
    ("latency_ms", "p95"),
    ("quiescence_ms", "median"),
    ("cpu_s", "median"),
    ("instructions", "median"),
    ("cycles", "median"),
    ("eff_cores", "median"),
    ("rss_mb", "median"),
    ("cg_mem_peak_mb", "median"),
]
INITIAL_METRICS = [
    "latency_ms",
    "quiescence_ms",
    "cpu_s",
    "instructions",
    "cycles",
    "rss_mb",
    "cg_mem_peak_mb",
]
RATIO_METRICS = ["latency_ms", "cpu_s", "instructions"]
P95_MIN_TRIALS = 100

# Reference palette (dataviz skill, light mode); colour follows the engine, never its rank.
SERIES = [
    "#2a78d6",
    "#eb6834",
    "#1baf7a",
    "#eda100",
    "#e87ba4",
    "#008300",
    "#4a3aa7",
    "#e34948",
]
KNOWN_ENGINES = ["querylinger", "psacklinger", "duckc"]
INK, INK_2, GRID, SURFACE = "#0b0b0b", "#52514e", "#e6e5e0", "#ffffff"
THREAD_MARKERS = ["o", "s", "^", "D", "v", "P"]


# ---------------------------------------------------------------- loading
def split_name(name):
    m = re.fullmatch(r"(.+)-j(\d+)", name)
    return (m.group(1), int(m.group(2))) if m else (name, None)


def workload_of(meta, run):
    if meta is None:
        return f"run{run}", None
    cfg = meta.get("config", {})
    label = cfg.get("workload") or Path(cfg.get("project", f"run{run}")).name
    size = cfg.get("size")
    return label, float(size) if isinstance(size, (int, float)) else None


def load(paths, include_warmup):
    runs, recs = [], []
    for p in paths:
        run = None
        for line in Path(p).read_text().splitlines():
            if not line.strip():
                continue
            r = json.loads(line)
            if r.get("kind") == "meta" or run is None:
                run = len(runs)
                runs.append(r if r.get("kind") == "meta" else None)
                if r.get("kind") == "meta":
                    continue
            r["run"] = run
            recs.append(r)

    bad = set()
    for r in recs:
        if r["kind"] == "session_abort" or r.get("timed_out"):
            bad.add((r["run"], r["compiler"], r["session"]))

    trials, initials = [], []
    for r in recs:
        if (r["run"], r["compiler"], r["session"]) in bad:
            continue
        r["workload"], r["size"] = workload_of(runs[r["run"]], r["run"])
        r["engine"], r["threads"] = split_name(r["compiler"])
        if r.get("cpu_s") is not None and r.get("latency_ms"):
            r["eff_cores"] = r["cpu_s"] / (r["latency_ms"] / 1e3)
        if r["kind"] == "initial":
            initials.append(r)
        elif r["kind"] in KINDS and (include_warmup or not r.get("warmup")):
            trials.append(r)
    return runs, trials, initials, bad, recs


# ---------------------------------------------------------------- bootstrap
def _stat(x, stat):
    if stat == "median":
        return np.nanmedian(x, axis=-1)
    if stat == "p95":
        return np.nanquantile(x, 0.95, axis=-1)
    if stat == "mean":
        return np.nanmean(x, axis=-1)
    raise ValueError(stat)


def _padded(groups):
    n = np.array([len(g) for g in groups])
    data = np.full((len(groups), n.max()), np.nan)
    for i, g in enumerate(groups):
        data[i, : len(g)] = g
    return data, n


def _draw(data, n, si, rng):
    """Trials of the sessions si (shape b x S), each resampled within its session."""
    b, s = si.shape
    m = data.shape[1]
    ns = n[si]
    ti = (rng.random((b, s, m)) * ns[..., None]).astype(np.int64)
    vals = data[si[..., None], ti]
    vals[np.arange(m)[None, None, :] >= ns[..., None]] = np.nan
    return vals.reshape(b, -1)


def _chunks(boot, cells):
    step = max(1, int(4e6 // max(cells, 1)))
    for lo in range(0, boot, step):
        yield min(step, boot - lo)


def hboot(groups, stat, boot, rng):
    """(point, lo, hi) of stat over sessions -> trials; no interval with < 2 sessions."""
    groups = [np.asarray(g, float) for g in groups if len(g)]
    if not groups:
        return (math.nan,) * 3
    point = float(_stat(np.concatenate(groups), stat))
    if len(groups) < 2:
        return point, math.nan, math.nan
    data, n = _padded(groups)
    out = []
    for b in _chunks(boot, data.size):
        si = rng.integers(0, len(groups), size=(b, len(groups)))
        out.append(_stat(_draw(data, n, si, rng), stat))
    lo, hi = np.nanpercentile(np.concatenate(out), [2.5, 97.5])
    return point, float(lo), float(hi)


def hboot_ratio(groups_a, groups_b, stat, boot, rng):
    """stat(A) / stat(B) with sessions paired by position (both lists aligned)."""
    pairs = [(a, b) for a, b in zip(groups_a, groups_b) if len(a) and len(b)]
    if not pairs:
        return (math.nan,) * 3
    ga = [np.asarray(a, float) for a, _ in pairs]
    gb = [np.asarray(b, float) for _, b in pairs]
    point = float(_stat(np.concatenate(ga), stat) / _stat(np.concatenate(gb), stat))
    if len(pairs) < 2:
        return point, math.nan, math.nan
    da, na = _padded(ga)
    db, nb = _padded(gb)
    out = []
    for b in _chunks(boot, da.size + db.size):
        si = rng.integers(0, len(pairs), size=(b, len(pairs)))
        out.append(
            _stat(_draw(da, na, si, rng), stat) / _stat(_draw(db, nb, si, rng), stat)
        )
    lo, hi = np.nanpercentile(np.concatenate(out), [2.5, 97.5])
    return point, float(lo), float(hi)


# ---------------------------------------------------------------- grouping
def by_session(rows, metric):
    """{(run, session): [values]} for rows that have the metric."""
    out = defaultdict(list)
    for r in rows:
        v = r.get(metric)
        if v is not None:
            out[(r["run"], r["session"])].append(float(v))
    return out


def cell_key(r):
    return (r["workload"], r["compiler"], r["edit"], r["kind"])


def group_cells(trials):
    cells = defaultdict(list)
    for r in trials:
        cells[cell_key(r)].append(r)
    return cells


def paired(rows_a, rows_b, metric):
    a, b = by_session(rows_a, metric), by_session(rows_b, metric)
    keys = sorted(set(a) & set(b))
    return [a[k] for k in keys], [b[k] for k in keys]


# ---------------------------------------------------------------- analyses
def analyse_cells(cells, boot, rng):
    out = []
    for key in sorted(cells):
        rows = cells[key]
        sessions = {(r["run"], r["session"]) for r in rows}
        rec = {
            "workload": key[0],
            "compiler": key[1],
            "edit": key[2],
            "kind": key[3],
            "sessions": len(sessions),
            "trials": len(rows),
            "errors": sum(bool(r.get("error")) for r in rows),
            "multiplexed": sum(bool(r.get("perf_multiplexed")) for r in rows),
        }
        rec_vals = [r["recompiled"] for r in rows if r.get("recompiled") is not None]
        rec["recompiled_median"] = float(np.median(rec_vals)) if rec_vals else None
        for metric, stat in CELL_METRICS:
            if stat == "p95" and len(rows) < P95_MIN_TRIALS:
                continue
            groups = list(by_session(rows, metric).values())
            if groups:
                p, lo, hi = hboot(groups, stat, boot, rng)
                rec[f"{metric}_{stat}"], rec[f"{metric}_{stat}_lo"] = p, lo
                rec[f"{metric}_{stat}_hi"] = hi
        out.append(rec)
    return out


def analyse_ratios(cells, reference, boot, rng):
    out = []
    for (wl, comp, edit, kind), rows in sorted(cells.items()):
        engine, threads = split_name(comp)
        if engine == reference:
            continue
        ref_name = reference if threads is None else f"{reference}-j{threads}"
        ref_rows = cells.get((wl, ref_name, edit, kind))
        if not ref_rows:
            continue
        rec = {
            "workload": wl,
            "compiler": comp,
            "reference": ref_name,
            "edit": edit,
            "kind": kind,
        }
        for metric in RATIO_METRICS:
            ga, gb = paired(rows, ref_rows, metric)
            if ga:
                p, lo, hi = hboot_ratio(ga, gb, "median", boot, rng)
                rec[metric], rec[f"{metric}_lo"], rec[f"{metric}_hi"] = p, lo, hi
                rec["paired_sessions"] = len(ga)
        out.append(rec)
    return out


def analyse_initial(initials, boot, rng):
    groups = defaultdict(list)
    for r in initials:
        groups[(r["workload"], r["compiler"])].append(r)
    out = []
    for (wl, comp), rows in sorted(groups.items()):
        rec = {"workload": wl, "compiler": comp, "sessions": len(rows)}
        for metric in INITIAL_METRICS:
            vals = [[r[metric]] for r in rows if r.get(metric) is not None]
            if vals:
                p, lo, hi = hboot(vals, "median", boot, rng)
                rec[metric], rec[f"{metric}_lo"], rec[f"{metric}_hi"] = p, lo, hi
        out.append(rec)
    return out


def trial_cost_ms(rows, runs):
    """Wall time one trial costs the harness: quiescence + idle window + gap."""
    q = np.median(
        [r["quiescence_ms"] for r in rows if r.get("quiescence_ms") is not None]
    )
    gap = (runs[rows[0]["run"]] or {}).get("args", {}).get("gap", 0.2)
    return q + 300.0 + gap * 1e3


def analyse_variance(cells, initials, runs):
    init_q = defaultdict(list)
    for r in initials:
        if r.get("quiescence_ms") is not None:
            init_q[(r["workload"], r["compiler"])].append(r["quiescence_ms"])
    out = []
    for (wl, comp, edit, kind), rows in sorted(cells.items()):
        groups = [
            np.log(np.asarray(g))
            for g in by_session(rows, "latency_ms").values()
            if len(g) >= 2 and min(g) > 0
        ]
        if len(groups) < 2:
            continue
        within = float(np.mean([g.var(ddof=1) for g in groups]))
        n_bar = float(np.mean([len(g) for g in groups]))
        between = max(
            0.0, float(np.var([g.mean() for g in groups], ddof=1)) - within / n_bar
        )
        args = (runs[rows[0]["run"]] or {}).get("args", {})
        c_trial = trial_cost_ms(rows, runs)
        c_session = (
            float(np.median(init_q.get((wl, comp), [0.0])))
            + 300.0
            + 2 * args.get("warmup", 0) * c_trial
        )
        n_opt = (
            math.sqrt(c_session / c_trial * within / between)
            if between > 0
            else math.inf
        )
        out.append(
            {
                "workload": wl,
                "compiler": comp,
                "edit": edit,
                "kind": kind,
                "sessions": len(groups),
                "var_within_log": within,
                "var_between_log": between,
                "icc": (
                    between / (between + within) if between + within > 0 else math.nan
                ),
                "trial_cost_ms": c_trial,
                "session_cost_ms": c_session,
                "opt_trials_per_session": n_opt,
            }
        )
    return out


def normalised(cells, trials):
    """latency / median latency of the trial's cell, for comparing across categories."""
    med = {
        k: float(np.median([r["latency_ms"] for r in rows if r.get("latency_ms")]))
        for k, rows in cells.items()
        if any(r.get("latency_ms") for r in rows)
    }
    for r in trials:
        m = med.get(cell_key(r))
        if m and r.get("latency_ms"):
            r["norm_latency"] = r["latency_ms"] / m
    return med


def analyse_drift(trials, boot, rng):
    sessions = defaultdict(list)
    for r in trials:
        if not r.get("warmup"):
            sessions[(r["workload"], r["compiler"], r["run"], r["session"])].append(r)
    slopes = defaultdict(lambda: {"lat": [], "rss": []})
    for (wl, comp, _, _), rows in sessions.items():
        it = np.array([r["iter"] for r in rows], float)
        if np.ptp(it) == 0:
            continue
        lat = [(r["iter"], r["norm_latency"]) for r in rows if r.get("norm_latency")]
        rss = [(r["iter"], r["rss_mb"]) for r in rows if r.get("rss_mb") is not None]
        if len(lat) >= 3:
            x, y = np.array(lat).T
            slopes[(wl, comp)]["lat"].append(np.polyfit(x, y, 1)[0] * 100 * 100)
        if len(rss) >= 3:
            x, y = np.array(rss).T
            slopes[(wl, comp)]["rss"].append(np.polyfit(x, y, 1)[0])
    out = []
    for (wl, comp), s in sorted(slopes.items()):
        rec = {"workload": wl, "compiler": comp, "sessions": len(s["lat"])}
        for name, vals, unit in (
            ("latency_pct_per_100_iters", s["lat"], None),
            ("rss_mb_per_iter", s["rss"], None),
        ):
            if vals:
                p, lo, hi = hboot([[v] for v in vals], "mean", boot, rng)
                rec[name], rec[f"{name}_lo"], rec[f"{name}_hi"] = p, lo, hi
        out.append(rec)
    return out


def analyse_scaling(cells, initials, boot, rng):
    """Speedup of every -j<N> compiler over the fastest lowest-thread compiler."""
    series = defaultdict(dict)  # (workload, edit, kind) -> {compiler: rows}
    for (wl, comp, edit, kind), rows in cells.items():
        if kind == "edit" and split_name(comp)[1] is not None:
            series[(wl, edit, kind)][comp] = rows
    for r in initials:
        if split_name(r["compiler"])[1] is not None:
            series[(r["workload"], "initial build", "initial")].setdefault(
                r["compiler"], []
            ).append(r)
    out = []
    for (wl, edit, kind), comps in sorted(series.items()):
        threads = {c: split_name(c)[1] for c in comps}
        if len(set(threads.values())) < 2:
            continue
        lowest = min(threads.values())
        base = min(
            (c for c in comps if threads[c] == lowest),
            key=lambda c: np.median(
                [r["latency_ms"] for r in comps[c] if r.get("latency_ms")]
            ),
        )
        for comp, rows in sorted(comps.items()):
            gb, ga = paired(comps[base], rows, "latency_ms")
            if not ga:
                continue
            p, lo, hi = hboot_ratio(gb, ga, "median", boot, rng)
            out.append(
                {
                    "workload": wl,
                    "edit": edit,
                    "kind": kind,
                    "compiler": comp,
                    "engine": split_name(comp)[0],
                    "threads": threads[comp],
                    "baseline": base,
                    "speedup": p,
                    "speedup_lo": lo,
                    "speedup_hi": hi,
                }
            )
    return out


# ---------------------------------------------------------------- output
def write_csv(path, rows):
    if not rows:
        return
    keys = list(dict.fromkeys(k for r in rows for k in r))
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        w.writerows(rows)


def fmt(v, unit=""):
    if v is None or (isinstance(v, float) and math.isnan(v)):
        return "-"
    if unit == "count":
        for div, suf in ((1e9, "G"), (1e6, "M"), (1e3, "k")):
            if abs(v) >= div:
                return f"{v / div:.2f}{suf}"
        return f"{v:.0f}"
    if unit == "x":
        return f"{v:.3f}"
    return f"{v:.1f}" if abs(v) >= 10 else f"{v:.2f}"


def ci(rec, key, unit=""):
    p = rec.get(key)
    if p is None:
        return "-"
    lo, hi = rec.get(f"{key}_lo"), rec.get(f"{key}_hi")
    if lo is None or math.isnan(lo):
        return fmt(p, unit)
    return f"{fmt(p, unit)} [{fmt(lo, unit)}, {fmt(hi, unit)}]"


def md_table(header, rows):
    lines = ["| " + " | ".join(header) + " |", "|" + "---|" * len(header)]
    lines += ["| " + " | ".join(str(c) for c in row) + " |" for row in rows]
    return "\n".join(lines)


def summary(
    runs, bad, cell_rows, ratio_rows, init_rows, var_rows, drift_rows, scale_rows, ref
):
    out = [
        "# watchbench analysis",
        "",
        f"Runs: {len(runs)}. Sessions dropped (abort or timeout): {len(bad)}. "
        "Values are medians with 95% hierarchical-bootstrap intervals.",
        "",
        "## Initial builds",
        "",
        md_table(
            [
                "workload",
                "compiler",
                "n",
                "latency ms",
                "CPU s",
                "instructions",
                "RSS MB",
            ],
            [
                [
                    r["workload"],
                    r["compiler"],
                    r["sessions"],
                    ci(r, "latency_ms"),
                    ci(r, "cpu_s"),
                    ci(r, "instructions", "count"),
                    ci(r, "rss_mb"),
                ]
                for r in init_rows
            ],
        ),
        "",
        "## Edits per cell",
        "",
        md_table(
            [
                "workload",
                "compiler",
                "edit",
                "kind",
                "sessions/trials",
                "latency ms",
                "p95 ms",
                "CPU s",
                "instructions",
                "eff. cores",
                "recompiled",
                "errors",
            ],
            [
                [
                    r["workload"],
                    r["compiler"],
                    r["edit"],
                    r["kind"],
                    f"{r['sessions']}/{r['trials']}",
                    ci(r, "latency_ms_median"),
                    ci(r, "latency_ms_p95"),
                    ci(r, "cpu_s_median"),
                    ci(r, "instructions_median", "count"),
                    ci(r, "eff_cores_median"),
                    fmt(r.get("recompiled_median")),
                    r["errors"],
                ]
                for r in cell_rows
            ],
        ),
    ]
    if ratio_rows:
        out += [
            "",
            f"## Ratios vs {ref} (< 1 = less than {ref}; sessions paired)",
            "",
            md_table(
                [
                    "workload",
                    "compiler",
                    "edit",
                    "kind",
                    "pairs",
                    "latency",
                    "CPU",
                    "instructions",
                ],
                [
                    [
                        r["workload"],
                        r["compiler"],
                        r["edit"],
                        r["kind"],
                        r.get("paired_sessions", "-"),
                        ci(r, "latency_ms", "x"),
                        ci(r, "cpu_s", "x"),
                        ci(r, "instructions", "x"),
                    ]
                    for r in ratio_rows
                ],
            ),
        ]
    if scale_rows:
        out += [
            "",
            "## Thread scaling (speedup over the fastest lowest-thread compiler)",
            "",
            md_table(
                ["workload", "edit", "compiler", "threads", "speedup"],
                [
                    [
                        r["workload"],
                        r["edit"],
                        r["compiler"],
                        r["threads"],
                        ci(r, "speedup", "x"),
                    ]
                    for r in scale_rows
                ],
            ),
        ]
    if not var_rows:
        out += [
            "",
            "## Variance components",
            "",
            "Not estimated: needs >= 2 sessions with >= 2 measured trials in a cell.",
        ]
    else:
        per_comp = defaultdict(list)
        for r in var_rows:
            per_comp[(r["workload"], r["compiler"])].append(r)
        out += [
            "",
            "## Variance components (log latency; medians over cells)",
            "",
            md_table(
                [
                    "workload",
                    "compiler",
                    "cells",
                    "ICC",
                    "optimal measured iters/session",
                ],
                [
                    [
                        wl,
                        comp,
                        len(rs),
                        fmt(float(np.median([r["icc"] for r in rs])), "x"),
                        fmt(
                            float(np.median([r["opt_trials_per_session"] for r in rs]))
                        ),
                    ]
                    for (wl, comp), rs in sorted(per_comp.items())
                ],
            ),
        ]
    if drift_rows:
        out += [
            "",
            "## Drift after warm-up (mean per-session slope)",
            "",
            md_table(
                [
                    "workload",
                    "compiler",
                    "sessions",
                    "latency %/100 iters",
                    "RSS MB/iter",
                ],
                [
                    [
                        r["workload"],
                        r["compiler"],
                        r["sessions"],
                        ci(r, "latency_pct_per_100_iters"),
                        ci(r, "rss_mb_per_iter"),
                    ]
                    for r in drift_rows
                ],
            ),
        ]
    return "\n".join(out) + "\n"


# ---------------------------------------------------------------- plots
def engine_colors(names):
    engines = sorted({split_name(n)[0] for n in names})
    order = [e for e in KNOWN_ENGINES if e in engines] + [
        e for e in engines if e not in KNOWN_ENGINES
    ]
    slots = {e: KNOWN_ENGINES.index(e) for e in order if e in KNOWN_ENGINES}
    free = [i for i in range(len(SERIES)) if i not in slots.values()]
    for e in order:
        if e not in slots:
            slots[e] = free.pop(0) if free else len(SERIES) - 1
    return {e: SERIES[s] for e, s in slots.items()}


def setup_mpl():
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    plt.rcParams.update(
        {
            "figure.facecolor": SURFACE,
            "axes.facecolor": SURFACE,
            "savefig.facecolor": SURFACE,
            "font.size": 9,
            "text.color": INK,
            "axes.labelcolor": INK_2,
            "axes.titlesize": 9,
            "axes.titlecolor": INK,
            "axes.edgecolor": INK_2,
            "axes.linewidth": 0.6,
            "axes.spines.top": False,
            "axes.spines.right": False,
            "axes.grid": True,
            "grid.color": GRID,
            "grid.linewidth": 0.6,
            "xtick.color": INK_2,
            "ytick.color": INK_2,
            "legend.frameon": False,
            "lines.linewidth": 1.5,
        }
    )
    return plt


def save(plt, fig, out_dir, name, formats):
    for ext in formats:
        fig.savefig(out_dir / f"{name}.{ext}", dpi=200, bbox_inches="tight")
    plt.close(fig)


def slug(s):
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", str(s))


def grid(plt, n, w=3.0, h=2.3):
    cols = min(n, 3)
    rows = math.ceil(n / cols)
    fig, axes = plt.subplots(rows, cols, figsize=(w * cols, h * rows), squeeze=False)
    for ax in axes.flat[n:]:
        ax.set_visible(False)
    return fig, list(axes.flat[:n])


def thread_groups(names):
    g = defaultdict(list)
    for n in names:
        g[split_name(n)[1]].append(n)
    return g


def set_scale(ax, axis, values):
    """Log scale with plain tick labels when the data spans >= 4x, linear otherwise."""
    from matplotlib.ticker import FuncFormatter, NullFormatter

    vals = [v for v in values if v and v > 0 and not math.isnan(v)]
    if not vals or max(vals) / min(vals) < 4:
        return
    getattr(ax, f"set_{axis}scale")("log")
    a = ax.xaxis if axis == "x" else ax.yaxis
    a.set_major_formatter(FuncFormatter(lambda v, _: f"{v:g}"))
    a.set_minor_formatter(NullFormatter())


def thread_markers(names):
    threads = sorted({split_name(n)[1] for n in names}, key=lambda t: (t is None, t))
    return {t: THREAD_MARKERS[i % len(THREAD_MARKERS)] for i, t in enumerate(threads)}


def legend_top(fig, axes):
    handles, labels = axes[0].get_legend_handles_labels()
    if len(labels) >= 2:
        fig.legend(handles, labels, loc="lower right", ncol=min(len(labels), 4),
                   bbox_to_anchor=(1.0, 1.0))  # fmt: skip


def plot_ecdf(plt, cells, colors, out_dir, formats):
    # (workload, threads, kind) -> edit -> compiler -> rows
    by = defaultdict(lambda: defaultdict(dict))
    for (wl, comp, edit, kind), rows in cells.items():
        by[(wl, split_name(comp)[1], kind)][edit][comp] = rows
    for (wl, threads, kind), edits in sorted(by.items(), key=lambda kv: str(kv[0])):
        fig, axes = grid(plt, len(edits))
        for ax, edit in zip(axes, sorted(edits)):
            xs = []
            for comp, rows in sorted(edits[edit].items()):
                x = [r["latency_ms"] for r in rows if r.get("latency_ms")]
                if x:
                    ax.ecdf(x, color=colors[split_name(comp)[0]], label=comp)
                    xs += x
            set_scale(ax, "x", xs)
            ax.set_title(edit, loc="left")
            ax.set_xlabel("latency (ms)")
            ax.set_ylabel("fraction of trials")
            ax.set_ylim(0, 1.02)
        fig.tight_layout()
        legend_top(fig, axes)
        fig.suptitle(f"{wl} · {kind}s" + (f" · {threads} thread{'s' * (threads != 1)}" if threads else ""),
                     x=0.01, y=1.0, ha="left", va="bottom", fontsize=10)  # fmt: skip
        tag = f"_j{threads}" if threads else ""
        save(plt, fig, out_dir, f"ecdf_{slug(wl)}{tag}_{kind}", formats)


def plot_ratios(plt, ratio_rows, colors, ref, out_dir, formats):
    by = defaultdict(list)
    for r in ratio_rows:
        if r.get("latency_ms") is not None:
            by[r["workload"]].append(r)
    for wl, rows in sorted(by.items()):
        labels = sorted({(r["edit"], r["kind"]) for r in rows})
        comps = sorted({r["compiler"] for r in rows})
        markers = thread_markers(comps)
        fig, ax = plt.subplots(figsize=(6, 0.35 * len(labels) + 1.2))
        step = 0.6 / max(len(comps), 1)
        for i, comp in enumerate(comps):
            for r in rows:
                if r["compiler"] != comp:
                    continue
                y = (
                    labels.index((r["edit"], r["kind"]))
                    + (i - (len(comps) - 1) / 2) * step
                )
                lo, hi = r.get("latency_ms_lo"), r.get("latency_ms_hi")
                err = None
                if lo is not None and not math.isnan(lo):
                    err = [[r["latency_ms"] - lo], [hi - r["latency_ms"]]]
                ax.errorbar(r["latency_ms"], y, xerr=err, fmt=markers[split_name(comp)[1]], ms=5,
                            capsize=0, elinewidth=1.5,
                            color=colors[split_name(comp)[0]], mec=SURFACE, mew=1)  # fmt: skip
            ax.plot([], [], markers[split_name(comp)[1]], color=colors[split_name(comp)[0]],
                    label=comp)  # fmt: skip
        ax.axvline(1, color=INK_2, lw=0.8, ls="--")
        set_scale(
            ax,
            "x",
            [v for r in rows for v in (r.get("latency_ms_lo"), r.get("latency_ms_hi"))],
        )
        ax.set_yticks(range(len(labels)), [f"{e} · {k}" for e, k in labels])
        ax.invert_yaxis()
        ax.grid(axis="y", visible=False)
        ax.set_xlabel(f"median latency ÷ {ref} (paired; < 1 = faster)")
        ax.set_title(f"{wl}: latency relative to {ref}", loc="left")
        if len(comps) >= 2:
            ax.legend(
                loc="upper center", bbox_to_anchor=(0.5, -0.25), ncol=min(len(comps), 4)
            )
        fig.tight_layout()
        save(plt, fig, out_dir, f"ratios_{slug(wl)}", formats)


def plot_scaling(plt, scale_rows, colors, out_dir, formats):
    by = defaultdict(lambda: defaultdict(list))
    for r in scale_rows:
        by[r["workload"]][r["edit"]].append(r)
    for wl, edits in sorted(by.items()):
        names = sorted(edits, key=lambda e: (e != "initial build", e))
        fig, axes = grid(plt, len(names))
        for ax, edit in zip(axes, names):
            rows = edits[edit]
            tmax = max(r["threads"] for r in rows)
            ax.plot([1, tmax], [1, tmax], color=GRID, lw=1.2, ls="--", label="ideal")
            for eng in sorted({r["engine"] for r in rows}):
                pts = sorted(
                    (r for r in rows if r["engine"] == eng), key=lambda r: r["threads"]
                )
                t = [r["threads"] for r in pts]
                s = [r["speedup"] for r in pts]
                ax.plot(t, s, "o-", ms=4, color=colors[eng], label=eng)
                lo = [r["speedup_lo"] for r in pts]
                hi = [r["speedup_hi"] for r in pts]
                if not any(math.isnan(v) for v in lo + hi):
                    ax.fill_between(t, lo, hi, color=colors[eng], alpha=0.15, lw=0)
            ax.set_title(edit, loc="left")
            ax.set_xlabel("threads")
            ax.set_ylabel("speedup")
            ax.set_xticks(sorted({r["threads"] for r in rows}))
        fig.tight_layout()
        legend_top(fig, axes)
        save(plt, fig, out_dir, f"scaling_{slug(wl)}", formats)


def plot_size(plt, cell_rows, sizes, colors, out_dir, formats):
    rows = [r for r in cell_rows if r["kind"] == "edit" and sizes.get(r["workload"])]
    if len({sizes[r["workload"]] for r in rows}) < 2:
        return
    for threads, comps in sorted(
        thread_groups({r["compiler"] for r in rows}).items(), key=str
    ):
        edits = sorted({r["edit"] for r in rows})
        fig, axes = grid(plt, len(edits))
        ys = [r["latency_ms_median"] for r in rows]
        for ax, edit in zip(axes, edits):
            for comp in sorted(comps):
                pts = sorted(
                    (r for r in rows if r["compiler"] == comp and r["edit"] == edit),
                    key=lambda r: sizes[r["workload"]],
                )
                if not pts:
                    continue
                x = [sizes[r["workload"]] for r in pts]
                y = [r["latency_ms_median"] for r in pts]
                lo = [r["latency_ms_median_lo"] for r in pts]
                hi = [r["latency_ms_median_hi"] for r in pts]
                err = None if any(math.isnan(v) for v in lo) else [
                    [a - b for a, b in zip(y, lo)], [b - a for a, b in zip(y, hi)]]  # fmt: skip
                ax.errorbar(x, y, yerr=err, fmt="o-", ms=4, capsize=0,
                            color=colors[split_name(comp)[0]], label=comp)  # fmt: skip
            set_scale(ax, "x", sizes.values())
            set_scale(ax, "y", ys)
            ax.set_title(edit, loc="left")
            ax.set_xlabel("program size")
            ax.set_ylabel("median latency (ms)")
        fig.tight_layout()
        legend_top(fig, axes)
        save(plt, fig, out_dir, "size" + (f"_j{threads}" if threads else ""), formats)


def plot_sessions(plt, trials, runs, colors, out_dir, formats, metric, ylabel, name):
    """One panel per compiler, one thin line per session over the iteration index."""
    by = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))
    for r in trials:
        if r.get(metric) is not None:
            by[r["workload"]][r["compiler"]][(r["run"], r["session"])].append(r)
    for wl, comps in sorted(by.items()):
        fig, axes = grid(plt, len(comps))
        for ax, comp in zip(axes, sorted(comps)):
            color = colors[split_name(comp)[0]]
            for rows in comps[comp].values():
                rows.sort(key=lambda r: (r["iter"], r["kind"] != "edit"))
                x = [r["iter"] + (0.5 if r["kind"] == "revert" else 0) for r in rows]
                ax.plot(x, [r[metric] for r in rows], color=color, lw=0.8, alpha=0.45)
            warm = max(((runs[k[0]] or {}).get("args", {}).get("warmup", 0) for k in comps[comp]),
                       default=0)  # fmt: skip
            if warm and any(r.get("warmup") for rs in comps[comp].values() for r in rs):
                ax.axvline(warm - 0.25, color=INK_2, lw=0.8, ls="--")
            ax.set_title(comp, loc="left")
            ax.set_xlabel("iteration")
            ax.set_ylabel(ylabel)
        fig.tight_layout()
        save(plt, fig, out_dir, f"{name}_{slug(wl)}", formats)


# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("results", nargs="+")
    ap.add_argument(
        "--reference", help="engine the ratios divide by (default: first in config)"
    )
    ap.add_argument("--boot", type=int, default=10000, help="bootstrap resamples")
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--include-warmup", action="store_true")
    ap.add_argument("--out-dir", default="analysis")
    ap.add_argument("--formats", default="png,pdf")
    ap.add_argument("--no-plots", action="store_true")
    args = ap.parse_args()

    rng = np.random.default_rng(args.seed)
    runs, trials, initials, bad, _ = load(args.results, include_warmup=True)
    measured = [r for r in trials if args.include_warmup or not r.get("warmup")]
    if not measured:
        raise SystemExit("no measured trials")
    ref = args.reference
    if ref is None:
        first = next((m for m in runs if m), None)
        names = [c["name"] for c in first["config"]["compilers"]] if first else []
        ref = split_name(names[0] if names else measured[0]["compiler"])[0]

    cells = group_cells(measured)
    normalised(cells, trials)
    cell_rows = analyse_cells(cells, args.boot, rng)
    ratio_rows = analyse_ratios(cells, ref, args.boot, rng)
    init_rows = analyse_initial(initials, args.boot, rng)
    var_rows = analyse_variance(cells, initials, runs)
    drift_rows = analyse_drift(measured, args.boot, rng)
    scale_rows = analyse_scaling(cells, initials, args.boot, rng)

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, rows in (
        ("cells", cell_rows),
        ("ratios", ratio_rows),
        ("initial", init_rows),
        ("variance", var_rows),
        ("drift", drift_rows),
        ("scaling", scale_rows),
    ):
        write_csv(out_dir / f"{name}.csv", rows)
    text = summary(
        runs,
        bad,
        cell_rows,
        ratio_rows,
        init_rows,
        var_rows,
        drift_rows,
        scale_rows,
        ref,
    )
    (out_dir / "summary.md").write_text(text)
    print(text)

    if args.no_plots:
        return
    try:
        plt = setup_mpl()
    except ImportError:
        print("matplotlib not available; skipping plots", file=sys.stderr)
        return
    formats = [f for f in args.formats.split(",") if f]
    colors = engine_colors(
        {r["compiler"] for r in trials} | {r["compiler"] for r in initials}
    )
    sizes = {r["workload"]: r["size"] for r in trials if r.get("size")}
    plot_ecdf(plt, cells, colors, out_dir, formats)
    plot_ratios(plt, ratio_rows, colors, ref, out_dir, formats)
    plot_scaling(plt, scale_rows, colors, out_dir, formats)
    plot_size(plt, cell_rows, sizes, colors, out_dir, formats)
    plot_sessions(
        plt, trials, runs, colors, out_dir, formats, "rss_mb", "RSS (MB)", "memory"
    )
    plot_sessions(plt, trials, runs, colors, out_dir, formats, "norm_latency",
                  "latency ÷ cell median", "warmup")  # fmt: skip
    print(f"plots and CSVs in {out_dir}/", file=sys.stderr)


if __name__ == "__main__":
    main()
