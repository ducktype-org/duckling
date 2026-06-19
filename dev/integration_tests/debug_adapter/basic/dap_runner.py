import sys
from dap_client import DAPTestClient

client = DAPTestClient(program_name="../examples/simple.dbc")

try:
    client.send_request("initialize")
    client.send_request("launch", {"program": client.program_name})

    client.wait_for_event("terminated", "terminated event at the end of execution")

    client.print_history()

finally:
    client.close()