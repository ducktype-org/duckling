#!/usr/bin/env python3
"""Compare callgrind profiles: single-threaded vs sum of multi-threaded worker threads.
Shows functions whose % of total increased most in multi-threaded run (contention overhead)."""

import re
import sys
from collections import defaultdict


def parse_callgrind_exclusive(filename):
    """Parse callgrind_annotate exclusive (self) output and extract function -> instruction count."""
    funcs = {}
    total = 0
    with open(filename) as f:
        content = f.read()

    # Find PROGRAM TOTALS
    m = re.search(r'([\d,]+)\s+PROGRAM TOTALS', content)
    if m:
        total = int(m.group(1).replace(',', ''))

    # Find function lines: "  123,456 (1.23%)  file:function"
    for line in content.splitlines():
        m = re.match(r'\s*([\d,]+)\s+\([\d.]+%\)\s+(.+)', line)
        if m and 'PROGRAM TOTALS' not in line:
            count = int(m.group(1).replace(',', ''))
            func = m.group(2).strip()
            # Clean up function name - extract just the function part
            if '???' in func:
                func = func.split('???:')[-1].split('[')[0].strip()
            else:
                parts = func.split(':')
                if len(parts) >= 2:
                    func = ':'.join(parts[1:]).split('[')[0].strip()
            funcs[func] = funcs.get(func, 0) + count
    return funcs, total


def parse_callgrind_raw(filename):
    """Parse raw callgrind.out file directly."""
    funcs = {}
    total = 0
    current_func = None
    current_cost = 0

    with open(filename) as f:
        for line in f:
            line = line.strip()
            if line.startswith('summary:'):
                total = int(line.split(':')[1].strip())
            elif line.startswith('fn=') or line.startswith('cfn='):
                if current_func and current_cost > 0:
                    funcs[current_func] = funcs.get(current_func, 0) + current_cost
                if line.startswith('fn='):
                    current_func = line[3:].strip()
                    # Strip the (N) suffix/prefix callgrind adds
                    current_func = re.sub(r'\s*\(\d+\)\s*', '', current_func)
                    current_cost = 0
                else:
                    current_func = None
                    current_cost = 0
            elif current_func and re.match(r'^\d+\s+\d+', line):
                parts = line.split()
                if len(parts) >= 2:
                    current_cost += int(parts[1])

        if current_func and current_cost > 0:
            funcs[current_func] = funcs.get(current_func, 0) + current_cost

    return funcs, total


def shorten(name, maxlen=80):
    if len(name) <= maxlen:
        return name
    return name[:maxlen-3] + '...'


def main():
    import glob

    # -w 1: thread -02 is the single worker (thread -01 is main/hashing)
    single_file = "cg_1w.out-02"
    # -w 9: threads -03 to -10 are workers (-01 is main/hash, -02 is idle)
    # We pick one representative worker thread for comparison
    multi_file = "cg_9w.out-04"

    print(f"Single worker thread file: {single_file}")
    print(f"Multi worker thread file:  {multi_file}")
    print()

    # Parse single worker thread
    single_funcs, single_total = parse_callgrind_raw(single_file)
    print(f"Single worker total instructions: {single_total:,}")

    # Parse multi worker thread
    multi_funcs, multi_total = parse_callgrind_raw(multi_file)
    print(f"Multi worker total instructions:  {multi_total:,}")
    print(f"Overhead ratio: {multi_total / single_total:.2f}x" if single_total else "")
    print()

    # Compare: find functions with biggest % increase
    results = []
    for func in set(list(single_funcs.keys()) + list(multi_funcs.keys())):
        s_cost = single_funcs.get(func, 0)
        m_cost = multi_funcs.get(func, 0)
        s_pct = (s_cost / single_total * 100) if single_total else 0
        m_pct = (m_cost / multi_total * 100) if multi_total else 0
        pct_diff = m_pct - s_pct

        # Only care about functions with meaningful cost
        if max(s_cost, m_cost) < single_total * 0.001:  # min 0.1% in either
            continue

        results.append((func, s_cost, m_cost, s_pct, m_pct, pct_diff))

    # Sort by pct_diff descending (biggest increase in multi)
    results.sort(key=lambda x: x[5], reverse=True)

    print("=" * 120)
    print(f"{'Function':<72} {'1w %':>7} {'9w %':>7} {'Diff':>7} {'1w Ir':>14} {'9w Ir':>14}")
    print("=" * 120)

    for func, s_cost, m_cost, s_pct, m_pct, pct_diff in results[:40]:
        marker = " <<<" if pct_diff > 1.0 else ""
        print(f"{shorten(func, 71):<72} {s_pct:>6.2f}% {m_pct:>6.2f}% {pct_diff:>+6.2f}% {s_cost:>14,} {m_cost:>14,}{marker}")

    print()
    print("=" * 120)
    print("Functions that DECREASED (faster in multi, or not contended):")
    print("=" * 120)
    for func, s_cost, m_cost, s_pct, m_pct, pct_diff in reversed(results[-20:]):
        print(f"{shorten(func, 71):<72} {s_pct:>6.2f}% {m_pct:>6.2f}% {pct_diff:>+6.2f}% {s_cost:>14,} {m_cost:>14,}")


if __name__ == "__main__":
    main()
