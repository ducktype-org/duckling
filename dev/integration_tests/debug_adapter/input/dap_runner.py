# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

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