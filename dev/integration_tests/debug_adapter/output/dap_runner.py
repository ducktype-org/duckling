from dap_client import DAPTestClient

client = DAPTestClient()

try:
    client.start_session()
    
    client.wait_for(outputs=["42"])

finally:
    client.close()