import json
from helpers import *

# --- MAIN ---
vm_process = start_vm(*sys.argv)

vm_process.stdin.write(format_dap('{"seq":1,"type":"request","command":"initialize","arguments":{}}'))
vm_process.stdin.write(format_dap('{"seq":2,"type":"request","command":"launch","arguments":{"program":"while_true.dbc"}}'))
vm_process.stdin.flush()


# waiting for Running output
while True:
    msg = read_dap_message(vm_process.stdout)
    if "Running" in msg:
        break

vm_process.stdin.write(format_dap('{"seq":3,"type":"request","command":"pause","arguments":{"threadId":1}}'))
vm_process.stdin.flush()

expected_pause_facts = {
    "pause_response_success",
    "event_stopped_pause",
    "console_output_paused"
}
recorded_facts = set()

while True:
    raw_msg = read_dap_message(vm_process.stdout)
    if raw_msg is None:
        break

    try:
        msg = json.loads(raw_msg)
    except json.JSONDecodeError:
        continue

    # Fact A: Received successful response for the pause request
    if msg.get("type") == "response" and msg.get("request_seq") == 3 and msg.get("success") is True:
        recorded_facts.add("pause_response_success")

    # Fact B: Received the official DAP "stopped" event
    if msg.get("type") == "event" and msg.get("event") == "stopped" and msg.get("body", {}).get("reason") == "pause":
        recorded_facts.add("event_stopped_pause")

    # Fact C: Your status_change_listener printed "Paused" to the console
    if msg.get("type") == "event" and msg.get("event") == "output" and "Paused" in msg.get("body", {}).get("output", ""):
        recorded_facts.add("console_output_paused")

    # Check if the pause sequence completed
    if expected_pause_facts.issubset(recorded_facts):
        sys.stderr.write("--> SUCCESS: VM successfully paused (interleaving handled)!\n")
        sys.stderr.flush()
        break

vm_process.stdin.write(format_dap('{"seq":4,"type":"request","command":"continue","arguments":{}}'))

expected_continue_facts = {
    "continue_response_success",
    "console_output_running"
}
recorded_facts.clear()

# waiting for Running output
while True:
    raw_msg = read_dap_message(vm_process.stdout)
    if raw_msg is None:
        break

    try:
        msg = json.loads(raw_msg)
    except json.JSONDecodeError:
        continue

    # Fact A: Received successful response for the continue request
    if msg.get("type") == "response" and msg.get("request_seq") == 4 and msg.get("success") is True:
        recorded_facts.add("continue_response_success")

    # Fact B: Your status_change_listener printed "Running" to the console again
    if msg.get("type") == "event" and msg.get("event") == "output" and "Running" in msg.get("body", {}).get("output", ""):
        recorded_facts.add("console_output_running")

    # Check if the continue sequence completed
    if expected_continue_facts.issubset(recorded_facts):
        sys.stderr.write("--> SUCCESS: VM successfully resumed execution!\n")
        sys.stderr.flush()
        break

print("SUCCESS")

vm_process.terminate()