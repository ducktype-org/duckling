import sys
from dap_client import DAPTestClient

client = DAPTestClient()

try:
    # Startup phase
    client.start_session()
    client.wait_for(outputs=["Running"])

    # Try 'next' while running (expecting failure)
    client.send_next()
    client.wait_for_text("Failed", "Failed output - next while running")

    # Request Pause and wait for all interleaved facts
    pause_seq = client.send_pause()
    client.wait_for(
        responses=[pause_seq],
        events=["stopped"],
        outputs=["Paused"]
    )
    sys.stderr.write("--> SUCCESS: VM successfully paused (interleaving handled)!\n")
    sys.stderr.flush()

    frames = client.get_frames()

    step_out_steps = 0
    while frames[0].get("name") == "vm_start_function":
        step_out_steps += 1
        if step_out_steps > 100:
            client.fail_test("Did not step out of 'vm_start_function' into 'main'")
        step_out_seq = client.send_next()
        client.wait_for(responses=[step_out_seq])
        frames = client.get_frames()

    f0 = frames[0]
    line = f0.get("line")

    # Try 'next' while paused (expecting successful response acknowledgment)
    next_seq = client.send_next()
    client.wait_for(responses=[next_seq])
    sys.stderr.write("--> SUCCESS: Step 'next' successfully acknowledged!\n")
    sys.stderr.flush()

    frames = client.get_frames()
    f0 = frames[0]
    line2 = f0.get("line")
    assert(line2 == line + 1 or (line == 6 and line2 == 5))

finally:
    client.close()