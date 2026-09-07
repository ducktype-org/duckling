import sys
from dap_client import DAPTestClient

client = DAPTestClient(program_name="../examples/simple/simple.dk")

try:
    init_seq = client.send_initialize()
    client.wait_for(responses=[init_seq])

    bp_seq = client.send_set_breakpoints(client.program_name, [4])
    client.wait_for(responses=[bp_seq])
    client.send_launch()
    client.send_configuration_done()
    client.wait_for(events=["stopped"])

    st_seq = client.send_stack_trace(thread_id=0, start_frame=0, levels=1)
    st_resp = client.wait_for_response(st_seq, "Getting stack trace", expect_success=True)
    
    frames = st_resp.get("body", {}).get("stackFrames", [])
    
    if len(frames) != 1:
        client.fail_test(f"Expected 1 stack frames, got {len(frames)}")
    
    f0 = frames[0]
    if "simple" not in f0.get("name"):
        client.fail_test(f"Expected top frame to have 'simple' in name, got '{f0.get('name')}'")
    if f0.get("line") != 4 or f0.get("column") != 5 or f0.get("endLine") != 4 or f0.get("endColumn") != 15:
        client.fail_test(f"Frame 0 position mismatch. Got line: {f0.get('line')}, col: {f0.get('column')}")
    if not f0.get("source", {}).get("path", "").endswith("simple.dk"):
        client.fail_test(f"Expected source path to end with 'simple.dk', got '{f0.get('source', {}).get('path')}'")

    next_seq = client.send_next()
    client.wait_for(responses=[next_seq])

    st_seq = client.send_stack_trace(thread_id=0, start_frame=0, levels=1)
    st_resp = client.wait_for_response(st_seq, "Getting stack trace", expect_success=True)
    
    frames = st_resp.get("body", {}).get("stackFrames", [])
    
    if len(frames) != 1:
        client.fail_test(f"Expected 1 stack frames, got {len(frames)}")
    
    f0 = frames[0]
    if "simple" not in f0.get("name"):
        client.fail_test(f"Expected top frame to have 'simple' in name, got '{f0.get('name')}'")
    if f0.get("line") != 5 or f0.get("column") != 5 or f0.get("endLine") != 5 or f0.get("endColumn") != 13:
        client.fail_test(f"Frame 0 position mismatch. Got line: {f0.get('line')}, col: {f0.get('column')}")
    if not f0.get("source", {}).get("path", "").endswith("simple.dk"):
        client.fail_test(f"Expected source path to end with 'simple.dk', got '{f0.get('source', {}).get('path')}'")



finally:
    client.close()