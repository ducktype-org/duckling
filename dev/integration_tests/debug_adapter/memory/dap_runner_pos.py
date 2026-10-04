from dap_client import DAPTestClient

client = DAPTestClient()
scenario = client.scenario

init_seq = client.send_initialize()
client.wait_for(responses=[init_seq])

# Breakpoint 6 - first line of function check
bp_seq = client.send_set_breakpoints(client.program_name, [6])
client.wait_for(responses=[bp_seq])
client.send_launch()
client.send_configuration_done()
client.wait_for(events=["stopped"])

# Stack trace request
st_seq = client.send_stack_trace(thread_id=0, start_frame=0, levels=2)
st_resp = client.wait_for_response(st_seq, "Getting stack trace", expect_success=True)

frames = st_resp.get("body", {}).get("stackFrames", [])
if len(frames) != 2:
    client.fail_test(f"Expected 2 stack frames, got {len(frames)}")

# Frame 0 (check)
f0 = frames[0]
if f0.get("name") != "check":
    client.fail_test(f"Expected top frame to be 'check', got '{f0.get('name')}'")
if f0.get("line") != 6 or f0.get("column") != 2 or f0.get("endLine") != 6 or f0.get("endColumn") != 25:
    client.fail_test(f"Frame 'check' position mismatch. Got line: {f0.get('line')}, col: {f0.get('column')}")

# Frame 1 (main)
f1 = frames[1]
if f1.get("name") != "main":
    client.fail_test(f"Expected second frame to be 'main', got '{f1.get('name')}'")
if f1.get("line") != 17 or f1.get("column") != 2 or f1.get("endLine") != 17 or f1.get("endColumn") != 26:
    client.fail_test(f"Frame 'main' position mismatch. Got line: {f1.get('line')}, col: {f1.get('column')}")
