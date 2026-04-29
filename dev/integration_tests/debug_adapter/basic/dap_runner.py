import subprocess
import sys
import os

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

# --- MAIN ---
if len(sys.argv) < 2:
    sys.stderr.write("ERROR: Build directory path was not provided as an argument!\n")
    sys.exit(1)

build_dir = sys.argv[1]
vm_binary_path = os.path.join(build_dir, "bin", "VM")

vm_process = subprocess.Popen(
    [vm_binary_path, "debug_adapter", "simple.dmf"],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    text=True,
    bufsize=1
)

vm_process.stdin.write(format_dap('{"seq":1,"type":"request","command":"initialize","arguments":{}}'))
vm_process.stdin.write(format_dap('{"seq":2,"type":"request","command":"launch","arguments":{"program":"simple.dmf"}}'))
vm_process.stdin.flush()

full_output = ""

# waiting for ExecutionCompleted output
while True:
    msg = read_dap_message(vm_process.stdout)
    if msg is None:
        break
    
    full_output += msg + "\n"
    
    if "ExecutionCompleted" in msg:
        sys.stderr.write("--> ZNALEZIONO: ExecutionCompleted! Kończę...\n")
        sys.stderr.flush()
        break

vm_process.stdin.write(format_dap('{"seq":3,"type":"request","command":"threads","arguments":{}}'))
vm_process.stdin.write(format_dap('{"seq":4,"type":"request","command":"disconnect","arguments":{}}'))
vm_process.stdin.flush()

# waiting for terminated event
while True:
    msg = read_dap_message(vm_process.stdout)
    if msg is None:
        break
    full_output += msg + '\n'
    if "terminated" in msg:
        break

# -1 to not print additional "\n"
print(full_output[:-1])

vm_process.terminate()