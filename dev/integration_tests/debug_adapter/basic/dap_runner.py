import sys
from dap_client import DAPTestClient

client = DAPTestClient(program_name="../examples/simple.dbc")

try:
    in_seq = client.send_request("initialize")
    client.wait_for(responses=[in_seq])

    cd_seq = client.send_request("configurationDone")
    client.wait_for(responses=[cd_seq])

    client.send_request("launch", {"program": client.program_name})

    client.wait_for_event("terminated", "terminated event at the end of execution")

    client.print_history()

finally:
    client.close()