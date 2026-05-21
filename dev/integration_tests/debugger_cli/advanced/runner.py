import subprocess
import sys
import os

# --- MAIN ---
if len(sys.argv) < 2:
    sys.stderr.write("ERROR: Build directory path was not provided as an argument!\n")
    sys.exit(1)

file = input()

build_dir = sys.argv[1]
vm_binary_path = os.path.join(build_dir, "bin", "VM")

vm_process = subprocess.Popen(
    [vm_binary_path, "debug", file],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    text=True,
    bufsize=1
)

full_output = ""

for i in range(3):
    line = vm_process.stdout.readline()
    if not line:
        break
    if line:
        full_output += line

vm_process.stdin.write("run\n")
vm_process.stdin.flush()

# waiting for terminated event
while True:
    msg = vm_process.stdout.readline()
    if msg is None:
        break
    full_output += msg
    if "returned" in msg:
        break

# -1 to not print additional "\n"
print(full_output[:-1])
vm_process.terminate()
print(vm_process.stdout.read()[:-1])
