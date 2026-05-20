from helpers import *

# --- MAIN ---
vm_process = start_vm(*sys.argv)

vm_process.stdin.write(format_dap('{"seq":1,"type":"request","command":"initialize","arguments":{}}'))
vm_process.stdin.write(format_dap('{"seq":2,"type":"request","command":"launch","arguments":{"program":"while_true.dmf"}}'))
vm_process.stdin.flush()

full_output = ""

# waiting for Running output
while True:
    msg = read_dap_message(vm_process.stdout)
    if msg is None:
        break
    
    full_output += msg + "\n"
    
    if "Running" in msg:
        break

vm_process.stdin.write(format_dap('{"seq":3,"type":"request","command":"pause","arguments":{}}'))

# waiting for Paused event
while True:
    msg = read_dap_message(vm_process.stdout)
    if msg is None:
        break
    full_output += msg + '\n'
    if "Paused" in msg:
        break

vm_process.stdin.write(format_dap('{"seq":4,"type":"request","command":"continue","arguments":{}}'))

# waiting for Running output
while True:
    msg = read_dap_message(vm_process.stdout)
    if msg is None:
        break
    
    full_output += msg + "\n"
    
    if "Running" in msg:
        break

# -1 to not print additional "\n"
print(full_output[:-1])

vm_process.terminate()