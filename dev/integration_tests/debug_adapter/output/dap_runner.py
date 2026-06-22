import sys
from dap_client import DAPTestClient

client = DAPTestClient(program_name="../examples/output.dbc")

try:
    # Startup phase
    client.send_request("initialize")
    launch_seq = client.send_request("launch", {"program": client.program_name})
    client.wait_for(responses=[launch_seq], outputs=["Running"])

    # Wait for output
    client.wait_for(
        outputs=["42"]
    )

finally:
    client.close()