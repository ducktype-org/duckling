import sys
from dap_client import DAPTestClient

client = DAPTestClient(program_name="../examples/while_true.dbc")

try:
    # Initialization and startup phase
    client.send_request("initialize")
    client.send_request("configurationDone")
    launch_seq = client.send_request("launch", {"program": client.program_name})
    client.wait_for(responses=[launch_seq], outputs=["Running"])

    # First Pause Phase (expecting interleaved async events)
    pause_seq = client.send_request("pause", {"threadId": 1})
    client.wait_for(
        responses=[pause_seq],
        events=["stopped"],
        outputs=["Paused"]
    )
    sys.stderr.write("--> SUCCESS: VM successfully paused!\n")

    # Double pause attempt (expecting dynamic response rejection)
    double_pause_seq = client.send_request("pause")
    client.wait_for_response(double_pause_seq, "Expecting double pause to fail", expect_success=False)

    # Continue Phase (expecting clean async continue sequence)
    continue_seq = client.send_request("continue")
    client.wait_for(
        responses=[continue_seq],
        outputs=["Running"]
    )
    sys.stderr.write("--> SUCCESS: VM successfully resumed execution!\n")

finally:
    client.close()