import subprocess
import time
import os
import re
import sys

VM_PATH = "/home/mily/Desktop/repos/duckling-zpp-4.1/dev/build/bin/VM"
ORIGINAL_DBC = "/home/mily/Desktop/repos/duckling-zpp-4.1/dev/src/vm/tests/threads/test_mutex.dbc"

OUTPUT_DIR = "./generated_dbc"
os.makedirs(OUTPUT_DIR, exist_ok=True)

x_values = [1, 2, 4, 6, 8, 10, 14, 18, 22, 24, 28, 32]

xs = []
times = []

RUNS = 3

with open(ORIGINAL_DBC, "r") as f:
    original_content = f.read()

print("Running benchmarks (3 runs each)...\n")

for x in x_values:
    new_content = original_content.replace(" 10 ", f" {x} ")

    new_file_path = os.path.join(OUTPUT_DIR, f"test_{x}.dbc")

    with open(new_file_path, "w") as f:
        f.write(new_content)

    run_times = []

    for i in range(RUNS):
        start = time.perf_counter()

        proc = subprocess.run(
            [VM_PATH, "run", new_file_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        elapsed = time.perf_counter() - start

        output = (proc.stdout or "").strip()
        err = (proc.stderr or "").strip()

        # ASSERT: program musi zwrócić dokładnie 10000
        if output != str((1000000//x) * x):
            print(
                f"\n[WARN] oczekiwano 100000, ale otrzymano '{output}' "
                f"(x={x}, run={i+1})\nSTDERR: {err}",
                file=sys.stderr
            )


        if not re.fullmatch(r"\d+", output):
            print(f"\n[WARN] x={x}, run={i+1} nietypowy output:", file=sys.stderr)
            if output:
                print(output, file=sys.stderr)
            if err:
                print(err, file=sys.stderr)

        run_times.append(elapsed)

    avg_time = sum(run_times) / RUNS

    xs.append(x)
    times.append(avg_time)

    print(f"x={x} -> avg {avg_time:.2f}s (runs: {[f'{t:.2f}' for t in run_times]})")

print("\n=== RESULT ARRAYS ===")
print("x_mutex =", xs)
print("times_mutex =", times)