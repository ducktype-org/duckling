# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import sys
from dap_client import DAPTestClient

client = DAPTestClient()

try:
    # Initialization and startup phase
    client.start_session()
    client.wait_for(outputs=["Running"])

    # First Pause Phase (expecting interleaved async events)
    pause_seq = client.send_pause()
    client.wait_for(
        responses=[pause_seq],
        events=["stopped"],
        outputs=["Paused"]
    )
    sys.stderr.write("--> SUCCESS: VM successfully paused!\n")

    # Double pause attempt (expecting dynamic response rejection)
    double_pause_seq = client.send_pause()
    client.wait_for_response(double_pause_seq, "Expecting double pause to fail", expect_success=False)

    # Continue Phase (expecting clean async continue sequence)
    continue_seq = client.send_continue()
    client.wait_for(
        responses=[continue_seq],
        outputs=["Running"]
    )
    sys.stderr.write("--> SUCCESS: VM successfully resumed execution!\n")

finally:
    client.close()