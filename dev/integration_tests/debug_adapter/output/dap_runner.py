import sys
from dap_client import DAPTestClient

client = DAPTestClient(program_name="../examples/output.dbc")

try:
    # Startup phase
    client.send_request("initialize")
    launch_seq = client.send_request("launch", {"program": client.program_name})
    client.wait_for(responses=[launch_seq], outputs=["Running"])

    # Request Pause and wait for all interleaved facts
    client.wait_for(
        outputs=["42"]
    )
    sys.stderr.write("--> SUCCESS: VM successfully outputed!\n")
    sys.stderr.flush()

finally:
    client.close()