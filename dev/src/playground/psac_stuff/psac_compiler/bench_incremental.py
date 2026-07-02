#!/usr/bin/env python3
"""Measures warm incremental update latency of psacklinger vs querylinger.

Drives both compilers' interactive (-i) mode: after the initial compile, applies a
single-line edit to the source, requests a recompile and times the round trip
(update = re-parse + re-analyze + propagate + relink). Scenarios:

  noop   identical content rewritten (no work expected)
  body   one numeric literal inside the first function body changed
  gvar   one top-level global variable initializer changed

Usage: bench_incremental.py [input.dmf ...]
Tunables: BUILD_DIR, THREADS (default 8), RUNS (default 5).
Results are written to results_incremental_<name>.md next to this script.
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEV_ROOT = SCRIPT_DIR.parents[3]
BUILD_DIR = Path(os.environ.get("BUILD_DIR", DEV_ROOT / "build-release"))
THREADS = os.environ.get("THREADS", "8")
RUNS = int(os.environ.get("RUNS", "5"))
BENCH_DATA = Path("/home/mateusz/code/benchmark/data/input")

COMPILERS = {
	"psacklinger": BUILD_DIR / "bin" / "psacklinger",
	"querylinger": BUILD_DIR / "bin" / "querylinger",
}


def make_edits(source: str):
	"""Returns {scenario: edited_source} for single-line edits of the corpus."""
	edits = {"noop": source}

	# body: bump the first i64 literal after the first function opening.
	body_start = source.index("= {")
	match = re.search(r"(\d+)i64", source[body_start:])
	number, at = match.group(1), body_start + match.start(1)
	edits["body"] = source[:at] + str(int(number) + 1) + source[at + len(number):]

	# gvar: bump the initializer of the first top-level `var NAME: i64 = N...;`.
	match = re.search(r"^var \w+: i64 = (\d+)", source, flags=re.MULTILINE)
	if match:
		number, at = match.group(1), match.start(1)
		edits["gvar"] = source[:at] + str(int(number) + 1) + source[at + len(number):]

	return edits


def read_reply(proc):
	line = proc.stdout.readline().strip()
	if not line:
		raise RuntimeError("compiler exited unexpectedly")
	return line


def bench_compiler(name, binary, input_path, edits, work_dir):
	prog = work_dir / f"{name}_prog.dmf"
	original = input_path.read_text()
	prog.write_text(original)

	proc = subprocess.Popen(
		[str(binary), str(prog), "-o", str(work_dir / f"{name}.out"), "-j", THREADS, "-i"],
		stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1,
	)
	initial = read_reply(proc)

	results = {}
	for scenario, edited in edits.items():
		samples, replies = [], []
		for run in range(RUNS):
			# Alternate edited/original so every run is a real single-line change
			# (for noop both are the original).
			for content in (edited, original):
				prog.write_text(content)
				start = time.perf_counter()
				proc.stdin.write("\n")
				proc.stdin.flush()
				replies.append(read_reply(proc))
				samples.append((time.perf_counter() - start) * 1000.0)
		results[scenario] = (min(samples), sum(samples) / len(samples), replies[-2])

	proc.stdin.write("quit\n")
	proc.stdin.flush()
	proc.wait(timeout=60)
	return initial, results


def main():
	inputs = [Path(p) for p in sys.argv[1:]] or [
		BENCH_DATA / "simple1" / "duck1.dmf",
		BENCH_DATA / "simple2" / "duck2.dmf",
	]

	for input_path in inputs:
		name = input_path.stem
		lines = [
			f"# Incremental update latency — {name} (-j {THREADS}, {RUNS} runs/scenario)",
			"",
			"| compiler | scenario | min (ms) | mean (ms) | recompiled |",
			"|:---|:---|---:|---:|:---|",
		]
		print(f"=== {name} ===")
		work_dir = Path(tempfile.mkdtemp(prefix="linger_incr_bench."))
		try:
			for comp_name, binary in COMPILERS.items():
				initial, results = bench_compiler(
					comp_name, binary, input_path, make_edits(input_path.read_text()), work_dir
				)
				print(f"  {comp_name}: initial {initial}")
				for scenario, (best, mean, reply) in results.items():
					lines.append(
						f"| {comp_name} | {scenario} | {best:.1f} | {mean:.1f} | {reply} |"
					)
					print(f"    {scenario:6} min {best:8.1f} ms  mean {mean:8.1f} ms   {reply}")
		finally:
			shutil.rmtree(work_dir, ignore_errors=True)

		out = SCRIPT_DIR / f"results_incremental_{name}.md"
		out.write_text("\n".join(lines) + "\n")
		print(f"  wrote {out}")


if __name__ == "__main__":
	main()
