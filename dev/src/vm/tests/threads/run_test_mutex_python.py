import subprocess
import time
import re
import sys

PYTHON_SCRIPT = "./test_mutex_python.py"

x_values = [1, 2, 4, 6, 8, 10, 14, 18, 22, 24, 28, 32]

xs = []
times = []

RUNS = 3

print("Running benchmarks...\n")

for x in x_values:
    run_times = []

    for i in range(RUNS):
        start = time.perf_counter()

        proc = subprocess.run(
            ["python3", PYTHON_SCRIPT, str(x)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        elapsed = time.perf_counter() - start

        output = (proc.stdout or "").strip()
        err = (proc.stderr or "").strip()

        # ASSERT: program powinien zwrócić 100000
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
print("x_mutex_py =", xs)
print("times_mutex_py =", times)