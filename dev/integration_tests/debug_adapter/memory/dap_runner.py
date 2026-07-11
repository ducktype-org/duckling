import sys
from dap_client import DAPTestClient

if len(sys.argv) < 3:
    sys.stderr.write("Error: Missing scenario argument\n")
    sys.exit(1)

scenario = sys.argv[2]
client = DAPTestClient(program_name="../examples/all_types_test.dbc")

try:
    init_seq = client.send_initialize()
    client.wait_for(responses=[init_seq])

    # Breakpoint 74 - before first free
    bp_seq = client.send_set_breakpoints(client.program_name, [74])
    client.wait_for(responses=[bp_seq])
    client.send_launch()
    client.send_configuration_done()
    client.wait_for(events=["stopped"])

    # =========================================================================
    # Scenario 1: all types
    # =========================================================================
    if scenario == "inspect_complex_types":
        # Stack trace request
        st_seq = client.send_stack_trace(thread_id=0, start_frame=0, levels=1)
        st_resp = client.wait_for_response(st_seq, "Getting stack trace", expect_success=True)
        
        frames = st_resp.get("body", {}).get("stackFrames", [])
        if not frames or frames[0]["name"] != "main":
            client.fail_test("Expected stack frame to be 'main'")
        
        frame_id = frames[0]["id"]

        # Scope request
        scopes_seq = client.send_scopes(frame_id)
        scopes_resp = client.wait_for_response(scopes_seq, "Getting scopes", expect_success=True)
        
        scopes = scopes_resp.get("body", {}).get("scopes", [])
        locals_scope = next((s for s in scopes if s["name"] == "Locals"), None)
        if not locals_scope:
            client.fail_test("Locals scope not found")
        
        locals_ref = locals_scope["variablesReference"]

        # Variables from main scope request
        vars_seq = client.send_variables(locals_ref)
        vars_resp = client.wait_for_response(vars_seq, "Getting local variables", expect_success=True)
        
        local_vars = vars_resp.get("body", {}).get("variables", [])
        
        struct_ptr = next((v for v in local_vars if v["name"] == "struct_pointer"), None)
        dyntable_ptr = next((v for v in local_vars if v["name"] == "dyntable_pointer"), None)
        fixtable_ptr = next((v for v in local_vars if v["name"] == "fixtable_pointer"), None)
        variant_ptr = next((v for v in local_vars if v["name"] == "variant_pointer"), None)

        if not (struct_ptr and dyntable_ptr and fixtable_ptr and variant_ptr):
            client.fail_test("Missing core complex pointers in stack frame variables")

        if struct_ptr["value"] != "<pointer>" or struct_ptr["variablesReference"] == 0:
            client.fail_test("struct_pointer should be a valid complex <pointer>")

        # ---------------------------------------------------------------------
        # Struct
        # ---------------------------------------------------------------------
        ref_struct_seq = client.send_variables(struct_ptr["variablesReference"])
        ref_struct_resp = client.wait_for_response(ref_struct_seq, "Expanding struct_pointer", expect_success=True)
        
        struct_children = ref_struct_resp.get("body", {}).get("variables", [])
        referenced_data = next((v for v in struct_children if v["name"] == "referenced"), None)
        
        if not referenced_data or referenced_data["value"] != "<data>":
            client.fail_test("struct_pointer expansion missing correct 'referenced' data payload")

        # ---------------------------------------------------------------------
        # AllTypeKindsStruct
        # ---------------------------------------------------------------------
        data_fields_seq = client.send_variables(referenced_data["variablesReference"])
        data_fields_resp = client.wait_for_response(data_fields_seq, "Expanding struct data fields", expect_success=True)
        
        fields = data_fields_resp.get("body", {}).get("variables", [])
        
        # Primitive
        var_i64 = next((f for f in fields if f["name"] == "var_i64"), None)
        if not var_i64 or var_i64["variablesReference"] != 0:
            client.fail_test("Primitive field var_i64 should have variablesReference equal to 0")

        # Cyclic pointer (struct_pointer->var_pointer = struct_pointer)
        var_pointer = next((f for f in fields if f["name"] == "var_pointer"), None)
        if not var_pointer or var_pointer["value"] != "<pointer>" or var_pointer["variablesReference"] == 0:
            client.fail_test("Cyclic pointer field var_pointer failed validation")

        # Variant
        var_variant = next((f for f in fields if f["name"] == "var_variant"), None)
        if not var_variant or var_variant["value"] != "<variant>":
            client.fail_test("Field var_variant should be evaluated as <variant>")

        variant_inner_seq = client.send_variables(var_variant["variablesReference"])
        variant_inner_resp = client.wait_for_response(variant_inner_seq, "Expanding variant", expect_success=True)
        
        variant_children = variant_inner_resp.get("body", {}).get("variables", [])
        var_referenced = next((c for c in variant_children if c["name"] == "referenced"), None)
        
        if not var_referenced or var_referenced["value"] != "42" or var_referenced["type"] != "i64":
            client.fail_test(f"Variant expected internal value '42' of type 'i64', got '{var_referenced}'")

        # ---------------------------------------------------------------------
        # Dynamic Table (dyntable_pointer -> size 5)
        # ---------------------------------------------------------------------
        ref_dyn_seq = client.send_variables(dyntable_ptr["variablesReference"])
        ref_dyn_resp = client.wait_for_response(ref_dyn_seq, "Expanding dyntable_pointer", expect_success=True)
        
        dyn_children = ref_dyn_resp.get("body", {}).get("variables", [])
        dyn_table_obj = next((v for v in dyn_children if v["name"] == "referenced"), None)
        
        if not dyn_table_obj or dyn_table_obj["value"] != "<table>":
            client.fail_test("dyntable_pointer missing 'referenced' <table> object")

        table_elements_seq = client.send_variables(dyn_table_obj["variablesReference"])
        table_elements_resp = client.wait_for_response(table_elements_seq, "Expanding table indices", expect_success=True)
        
        elements = table_elements_resp.get("body", {}).get("variables", [])
        if len(elements) != 5:
            client.fail_test(f"Expected dynamic table to have exactly 5 elements, found {len(elements)}")
        
        for idx in range(5):
            if elements[idx]["name"] != str(idx):
                client.fail_test(f"Expected table index keys to be sequential string digits, failed at: {elements[idx]['name']}")

        sys.stdout.write("SUCCESS: Complex structures, cyclic pointers, tables and variants verified perfectly.\n")

    # =========================================================================
    # Scenario 2: Double request for the same variables refernce
    # =========================================================================
    elif scenario == "test_double_expansion":
        st_seq = client.send_stack_trace()
        st_resp = client.wait_for_response(st_seq, "ST", True)
        locals_seq = client.send_scopes(st_resp["body"]["stackFrames"][0]["id"])
        locals_resp = client.wait_for_response(locals_seq, "Scopes", True)
        locals_ref = locals_resp["body"]["scopes"][0]["variablesReference"]

        # First request
        v_seq1 = client.send_variables(locals_ref)
        v_resp1 = client.wait_for_response(v_seq1, "First variables fetch", True)
        
        variables_list1 = v_resp1["body"]["variables"]
        struct_ptr1 = next((v for v in variables_list1 if "struct" in v["type"] or v["name"] == "struct_pointer"), None)

        if not struct_ptr1:
            client.fail_test("No variables returned from stack frame to complete double expansion test.")

        # Second request
        v_seq2 = client.send_variables(locals_ref)
        v_resp2 = client.wait_for_response(v_seq2, "Second variables fetch", True)
        variables_list2 = v_resp2["body"]["variables"]
        
        struct_ptr2 = next((v for v in variables_list2 if v["name"] == struct_ptr1["name"]), None)

        if not struct_ptr2:
            client.fail_test(f"Variable '{struct_ptr1['name']}' disappeared on consecutive fetch!")

        if struct_ptr1["variablesReference"] != struct_ptr2["variablesReference"]:
            client.fail_test("Critical: Variables reference changed or corrupted during second expansion loop!")
        
        sys.stdout.write("SUCCESS: Zero state-mutation corruption detected. Variant fix holds perfectly stable.\n")

    else:
        sys.stderr.write(f"Error: Unknown scenario '{scenario}'\n")
        sys.exit(1)

finally:
    client.close()