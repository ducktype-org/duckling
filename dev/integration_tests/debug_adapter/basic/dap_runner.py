import sys
from dap_client import DAPTestClient

client = DAPTestClient(program_name="../examples/simple.dbc")

try:
    in_seq = client.send_initialize()
    client.wait_for(responses=[in_seq])

    cd_seq = client.send_configuration_done()
    client.wait_for(responses=[cd_seq])

    client.send_launch()

    client.wait_for_event("terminated", "terminated event at the end of execution")

    client.print_history()

finally:
    client.close()