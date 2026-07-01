import sys
from dap_client import DAPTestClient

client = DAPTestClient(program_name="../examples/input.dbc")

try:
    # Startup phase
    client.send_request("initialize")
    launch_seq = client.send_request("launch", {"program": client.program_name})
    client.wait_for(responses=[launch_seq])

    # Sending input in evaluate request
    number = 42
    input_seq = client.send_request("evaluate", {"expression": f"{number}", "context": "repl"})
    client.wait_for(
        outputs=[f"VM returned: [{number}]"]
    )
    

finally:
    client.close()