import sys
import os
import subprocess

# --- Helpers ---

def format_dap(json_str):
    return f"Content-Length: {len(json_str)}\r\n\r\n{json_str}"

# returns only body of the message got from stdout
def read_dap_message(stdout):
    content_length = 0
    while True:
        line = stdout.readline()
        if not line:
            return None

        if line.startswith("Content-Length:"):
            content_length = int(line.split(":")[1].strip())

        if line == "\r\n" or line == "\n" or line.strip() == "":
            if content_length > 0:
                break

    body = stdout.read(content_length)

    return body

def start_vm(*args):
    if len(sys.argv) < 2:
        sys.stderr.write("ERROR: Build directory path was not provided as an argument!\n")
        sys.exit(1)

    build_dir = sys.argv[1]
    vm_binary_path = os.path.join(build_dir, "bin", "VM")

    vm_process = subprocess.Popen(
        [vm_binary_path, "debug_adapter"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        text=True,
        bufsize=1
    )

    return vm_process