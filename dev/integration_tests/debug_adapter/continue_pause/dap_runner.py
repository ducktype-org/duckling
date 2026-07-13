import sys
from dap_client import DAPTestClient

client = DAPTestClient(program_name="../examples/while_true.dbc")

try:
    # Initialization and startup phase
    client.start_session()
    client.wait_for(outputs=["Running"])

    # First Pause Phase (expecting interleaved async events)
    pause_seq = client.send_pause()
    client.wait_for(
        responses=[pause_seq],
        events=["stopped"],
        outputs=["Paused"]
    )
    sys.stderr.write("--> SUCCESS: VM successfully paused!\n")

    # Double pause attempt (expecting dynamic response rejection)
    double_pause_seq = client.send_pause()
    client.wait_for_response(double_pause_seq, "Expecting double pause to fail", expect_success=False)

    # Continue Phase (expecting clean async continue sequence)
    continue_seq = client.send_continue()
    client.wait_for(
        responses=[continue_seq],
        outputs=["Running"]
    )
    sys.stderr.write("--> SUCCESS: VM successfully resumed execution!\n")

finally:
    client.close()