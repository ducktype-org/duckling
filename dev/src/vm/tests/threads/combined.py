import subprocess

print("\nRunning python non-mutex test")
subprocess.run(
    ["python3", "./run_test_python.py"],
)

print("\nRunning python mutex test")
subprocess.run(
    ["python3", "./run_test_mutex_python.py"],
)

print("Running DVM non-mutex test")
subprocess.run(
    ["python3", "./test.py"],
    text=True
)

print("\nRunning DVM mutex test")
subprocess.run(
    ["python3", "./test_mutex.py"],
)