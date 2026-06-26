import sys
import time
from dap_client import DAPTestClient

if len(sys.argv) < 3:
    sys.stderr.write("Error: Missing scenario argument\n")
    sys.exit(1)

scenario = sys.argv[2]
client = DAPTestClient(program_name="../examples/simple.dbc")

try:
    client.send_initialize()

    # =========================================================================
    # Scenario 1: Launch -> SetBreakpoints -> ConfigurationDone
    # =========================================================================
    if scenario == "launch_set_conf":
        launch_seq = client.send_launch()
        
        bp_seq = client.send_set_breakpoints(client.program_name, [4])
        client.wait_for(responses=[bp_seq])

        config_seq = client.send_configuration_done()
        client.wait_for(responses=[launch_seq, config_seq], events=["stopped"])

    # =========================================================================
    # Scenario 2: SetBreakpoints -> Launch -> ConfigurationDone
    # =========================================================================
    elif scenario == "set_launch_conf":
        bp_seq = client.send_set_breakpoints(client.program_name, [4])

        client.wait_for_response(bp_seq, "verified: false", True)

        launch_seq = client.send_launch()
        client.wait_for_text("\"verified\": true", "Breakpoint has not been verified")

        config_seq = client.send_configuration_done()
        client.wait_for(responses=[launch_seq, config_seq], events=["stopped"])

    # =========================================================================
    # Scenario 3: SetBreakpoints -> ConfigurationDone -> Launch
    # =========================================================================
    elif scenario == "set_conf_launch":
        bp_seq = client.send_set_breakpoints(client.program_name, [4])

        client.wait_for(responses=[bp_seq])

        config_seq = client.send_configuration_done()
        client.wait_for(responses=[config_seq])

        launch_seq = client.send_launch()

        client.wait_for(responses=[launch_seq], events=["stopped"])

    # =========================================================================
    # Sceanrio 4: Adding new breakpoints after run
    # =========================================================================
    elif scenario == "add":
        bp_seq1 = client.send_set_breakpoints(client.program_name, [4])

        client.wait_for(responses=[bp_seq1])
        client.send_launch()
        client.send_configuration_done()

        client.wait_for(events=["stopped"]) # Stop - line 4

        # Add breakpoints at line 5, 8
        bp_seq2 = client.send_set_breakpoints(client.program_name, [4, 5, 8])

        client.wait_for(responses=[bp_seq2])
        client.send_continue()

        client.wait_for(events=["stopped"]) # Stop - line 5
        client.send_continue()

        client.wait_for(events=["stopped"]) # Stop - line 8

    # =========================================================================
    # Scenario 5: Removing breakpoints after run
    # =========================================================================
    elif scenario == "remove":
        bp_seq1 = client.send_set_breakpoints(client.program_name, [4, 5, 8])

        client.wait_for(responses=[bp_seq1])
        client.send_launch()
        client.send_configuration_done()
        
        client.wait_for(events=["stopped"]) # Stop - line 4

        # Remove breakpoint at line 5
        bp_seq2 = client.send_set_breakpoints(client.program_name, [4, 8])

        client.wait_for(responses=[bp_seq2])
        client.send_continue()

        client.wait_for(events=["stopped"]) # Stop - line 8

    else:
        sys.stderr.write(f"Error: Unknown scenario '{scenario}'\n")
        sys.exit(1)

finally:
    client.close()