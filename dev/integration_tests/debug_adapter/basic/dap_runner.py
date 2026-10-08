# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from dap_client import DAPTestClient

client = DAPTestClient()

try:
    in_seq = client.send_initialize()
    client.wait_for(responses=[in_seq])

    cd_seq = client.send_configuration_done()
    client.wait_for(responses=[cd_seq])

    client.send_launch()

    client.wait_for_event("terminated", "terminated event at the end of execution")

finally:
    client.close()