from dap_client import DAPTestClient

client = DAPTestClient()

try:
    # Startup phase
    client.start_session()
    client.wait_for(outputs=["Running"])

    # Sending input in evaluate request
    number = 42
    input_seq = client.send_evaluate(number)
    client.wait_for(
        outputs=[f"VM returned: [{number}]"]
    )
    

finally:
    client.close()