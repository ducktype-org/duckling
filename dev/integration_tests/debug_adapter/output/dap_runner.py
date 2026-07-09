import sys
from dap_client import DAPTestClient

client = DAPTestClient(program_name="../examples/output.dbc")

try:
    client.start_session()
    
    client.wait_for(outputs=["42"])

finally:
    client.close()