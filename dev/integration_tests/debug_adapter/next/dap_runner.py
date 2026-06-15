import sys
from dap_client import DAPTestClient

client = DAPTestClient(program_name="../examples/while_true.dbc")

try:
    # Startup phase
    client.send_request("initialize")
    launch_seq = client.send_request("launch", {"program": client.program_name})
    client.wait_for(responses=[launch_seq], outputs=["Running"])

    # Try 'next' while running (expecting failure)
    client.send_request("next")
    client.wait_for_text("Failed", "Failed output - next while running")

    # Request Pause and wait for all interleaved facts
    pause_seq = client.send_request("pause", {"threadId": 1})
    client.wait_for(
        responses=[pause_seq],
        events=["stopped"],
        outputs=["Paused"]
    )
    sys.stderr.write("--> SUCCESS: VM successfully paused (interleaving handled)!\n")
    sys.stderr.flush()

    # Try 'next' while paused (expecting successful response acknowledgment)
    next_seq = client.send_request("next")
    client.wait_for(responses=[next_seq])
    sys.stderr.write("--> SUCCESS: Step 'next' successfully acknowledged!\n")
    sys.stderr.flush()

    # @TODO: #2558 Add check if the step was really done by stackTrace request
    # Implemented when handling DAP memory requests

finally:
    client.close()