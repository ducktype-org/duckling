#!/usr/bin/env python3
"""Turn a query graph JSON dump into a Graphviz graph, and optionally render it.

The JSON comes from `duckc experimental_compile_package_dump_graph ... --graph-output <dir>`,
which writes `query_graph_pre_opt.json` and `query_graph_post_opt.json` into `<dir>`.
Add `--no-other-input` (drops every input node except the source code inputs, before the other
passes), `--rename-pass` (readable input names, unstable nodes shown as `Unstable Node`),
`--simplify-pass` (also drops duplicated edges and submodule nodes, and merges source code inputs
used by only one node) and/or `--remove-dead-nodes` (drops non-input nodes without dependencies,
recursively) to that command to get a much smaller graph.

Node shape shows what kind of node it is:
  - box      input node (Input / SideInput query)
  - ellipse  stable node (stable hash, can survive across compilations)
  - diamond  unstable node (unstable hash)
Nodes preserved by the graph optimization are drawn with a bold border. Each label ends with the
node's `#<index>` in the dump, unless `--no-hashes` is given. The label of an unstable node is
wrapped onto several lines (see `--unstable-wrap`), so the diamond grows taller instead of wider.

The graph has few levels but hundreds of nodes per level, so a plain Graphviz layout is a very
tall and thin picture. To use more width and less height, the dependencies of each node are
spread over several columns (`--stagger`), nodes and edges in one column are packed closely
(`--nodesep`), and parallel edges are merged (`--no-concentrate` turns that off).

Examples:
  query_graph_to_dot.py out/query_graph_post_opt.json -o post.dot
  query_graph_to_dot.py out/query_graph_post_opt.json --render svg      # writes post_opt.svg
  query_graph_to_dot.py out/query_graph_pre_opt.json --no-inputs --render png
  query_graph_to_dot.py out/query_graph_pre_opt.json --no-hashes --unstable-wrap 6 --render svg
  query_graph_to_dot.py out/query_graph_post_opt.json --stagger 1 --render svg  # no staggering: narrower, taller
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

SHAPES = {"input": "box", "stable": "ellipse", "unstable": "diamond"}
COLORS = {"input": "#9ecae1", "stable": "#a1d99b", "unstable": "#fdae6b"}
DEFAULT_UNSTABLE_WRAP = 10
DEFAULT_STAGGER = 4
DEFAULT_NODESEP = 0.02  # inches, the smallest Graphviz allows; its own default is 0.25
NEWLINE = "\\n"  # a line break inside a DOT label


def load_graph(path: Path) -> dict:
    with path.open() as f:
        graph = json.load(f)
    if "nodes" not in graph:
        raise ValueError(f"{path}: not a query graph dump (no 'nodes' field)")
    return graph


def wrap_label(name: str, width: int) -> list[str]:
    """Split `name` into lines of about `width` characters, at spaces and CamelCase boundaries.

    A piece longer than `width` stays whole on its own line. `width` <= 0 disables wrapping.
    """
    if width <= 0:
        return [name]
    lines: list[str] = []
    for word in name.split():
        # "QueryTypeCheck" -> "Query", "Type", "Check"; pieces of one word join without a space.
        for i, piece in enumerate(re.findall(r"[A-Z]?[^A-Z]+|[A-Z]+(?![a-z])", word) or [word]):
            joiner = "" if i > 0 else " "
            if lines and len(lines[-1]) + len(joiner) + len(piece) <= width:
                lines[-1] += joiner + piece
            else:
                lines.append(piece)
    return lines


def stagger_levels(nodes: list, kept: set, stagger: int) -> dict:
    """Return `{node index: minlen}` that spreads the dependencies of each node over `stagger` columns.

    Graphviz puts every dependency of a node into the column right after it, so the graph gets only
    a few columns, each hundreds of nodes tall. Here the dependencies of each node take turns: the
    1st one goes 1 column further, the 2nd 2 columns, ..., the `stagger`-th `stagger` columns, and
    then it starts again (like Graphviz' `unflatten`, but for every node, not only the leaves).
    A node used by several nodes is placed by the first of them. `stagger` <= 1 disables it.
    """
    if stagger <= 1:
        return {}
    levels: dict = {}
    for node in nodes:
        if node["index"] not in kept:
            continue
        count = 0
        for dep in dict.fromkeys(node["deps"]):  # a duplicated edge must not skip a column
            if dep in kept and dep not in levels:
                levels[dep] = count % stagger + 1
                count += 1
    return levels


def to_dot(
    graph: dict,
    include_inputs: bool = True,
    show_index: bool = True,
    unstable_wrap: int = DEFAULT_UNSTABLE_WRAP,
    stagger: int = DEFAULT_STAGGER,
    nodesep: float = DEFAULT_NODESEP,
    concentrate: bool = True,
) -> str:
    """Return the DOT source for a loaded query graph dump."""
    nodes = graph["nodes"]
    kept = {n["index"] for n in nodes if include_inputs or n["category"] != "input"}
    levels = stagger_levels(nodes, kept, stagger)

    lines = [
        "digraph query_graph {",
        # Left to right: the graph has few levels but many nodes per level, so top to bottom would
        # be even wider than this is tall. Most of the height is edges that pass through a column,
        # and each of them takes `nodesep` of room, so a small `nodesep` and merging parallel edges
        # (`concentrate`) make the picture much less tall. A lower `mclimit` spends less time on
        # edge crossings: several times faster on big graphs, and here even a bit less tall.
        f"  rankdir=LR; nodesep={nodesep}; concentrate={str(concentrate).lower()}; mclimit=0.2;",
        '  node [style=filled, fontname="monospace", fontsize=10];',
    ]
    for node in nodes:
        if node["index"] not in kept:
            continue
        category = node["category"]
        label = wrap_label(node["name"], unstable_wrap) if category == "unstable" else [node["name"]]
        if show_index:
            label.append(f"#{node['index']}")
        attrs = [
            f'label="{NEWLINE.join(label)}"',
            f"shape={SHAPES.get(category, 'hexagon')}",
            f'fillcolor="{COLORS.get(category, "white")}"',
        ]
        if category == "unstable":
            attrs.append("margin=0")
        if node.get("preserved"):
            attrs.append("penwidth=2.5")
        lines.append(f"  n{node['index']} [{', '.join(attrs)}];")
    for node in nodes:
        if node["index"] not in kept:
            continue
        for dep in node["deps"]:
            if dep in kept:
                minlen = f" [minlen={levels[dep]}]" if levels.get(dep, 1) > 1 else ""
                lines.append(f"  n{node['index']} -> n{dep}{minlen};")
    lines.append("}")
    return "\n".join(lines) + "\n"


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("graph", type=Path, help="query graph JSON dump")
    parser.add_argument("-o", "--output", type=Path, help="DOT output file (default: <graph>.dot)")
    parser.add_argument("--no-inputs", action="store_true", help="leave out input nodes (they are often most of the graph)")
    parser.add_argument("--no-hashes", action="store_true", help="leave the `#<index>` numbers out of the node labels")
    parser.add_argument(
        "--unstable-wrap",
        type=int,
        default=DEFAULT_UNSTABLE_WRAP,
        metavar="CHARS",
        help=f"wrap unstable node labels after about CHARS characters, so the diamonds are narrower "
        f"and taller (default: {DEFAULT_UNSTABLE_WRAP}, 0 = no wrapping)",
    )
    parser.add_argument(
        "--stagger",
        type=int,
        default=DEFAULT_STAGGER,
        metavar="N",
        help=f"spread the dependencies of each node over N columns, so the picture gets wider and "
        f"less tall (default: {DEFAULT_STAGGER}, 1 = off, bigger = wider but slower to render)",
    )
    parser.add_argument(
        "--nodesep",
        type=float,
        default=DEFAULT_NODESEP,
        metavar="INCHES",
        help=f"room between nodes and edges in one column (default: {DEFAULT_NODESEP}, Graphviz: 0.25)",
    )
    parser.add_argument("--no-concentrate", action="store_true", help="do not merge parallel edges (more edges, taller picture)")
    parser.add_argument("--render", metavar="FORMAT", help="also call Graphviz `dot` to render, e.g. svg or png")
    args = parser.parse_args(argv)

    try:
        graph = load_graph(args.graph)
    except (OSError, ValueError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1

    dot_path = args.output or args.graph.with_suffix(".dot")
    dot_path.write_text(
        to_dot(
            graph,
            include_inputs=not args.no_inputs,
            show_index=not args.no_hashes,
            unstable_wrap=args.unstable_wrap,
            stagger=args.stagger,
            nodesep=args.nodesep,
            concentrate=not args.no_concentrate,
        )
    )
    print(f"Wrote {dot_path}")

    if args.render:
        if shutil.which("dot") is None:
            print("error: Graphviz `dot` not found on PATH", file=sys.stderr)
            return 1
        out_path = dot_path.with_suffix("." + args.render)
        subprocess.run(["dot", f"-T{args.render}", str(dot_path), "-o", str(out_path)], check=True)
        print(f"Wrote {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
