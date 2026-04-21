# This has to be static due to cyclic dependencies in helios->mir->lir->dvm for comp time, and helios<->TS cyclic dependency.
add_library(HELIOS STATIC
	src_private/helios_private/symbols/symbol_data.cpp
	src_private/helios_private/symbols/symbols.cpp
	src_private/helios_private/scopes/scopes.cpp
	src_private/helios_private/scopes/scope_data.cpp
	src_private/helios_private/pst_layer/stmts_from_aggregate.cpp
	src_private/helios_private/pst_layer/macros.cpp
	src_private/helios_private/lookup/lookup_result.cpp
	src_private/helios_private/lookup/interface.cpp
	src_private/helios_private/lookup/lookup_chain.cpp
	src_private/helios_private/hout_creation/expressions/function_calls/call_processing.cpp
	src_private/helios_private/hout_creation/expressions/function_calls/square_call_processing.cpp
	src_private/helios_private/hout_creation/expressions/function_calls/errors.cpp
	src_private/helios_private/hout_creation/expressions/chain_expr.cpp
	src_private/helios_private/hout_creation/expressions/query_hout_of_expr.cpp
	src_private/helios_private/hout_creation/expressions/builtin_operators.cpp
	src_private/helios_private/hout_creation/expressions/numeric_literals.cpp
	src_private/helios_private/hout_creation/expressions/coercions.cpp
	src_private/helios_private/comp_time/comp_time.cpp
	src_private/helios_private/comp_time/vm_evaluator.cpp
	src_private/helios_private/comp_time/comptime_type_operations.cpp
	src_private/helios_private/hout_creation/definition_generation/class_constructors.cpp
	src_private/helios_private/hout_creation/definition_generation/default_constructors.cpp
	src_private/helios_private/hout_creation/hout_stmt_compilation.cpp
	src_private/helios_private/errors/dia_interactive_elements.cpp
	src_private/helios_private/tsh/abstract_type_impl.cpp
	src_private/helios_private/tsh/queries.cpp

	src/helios/queries/queries.cpp
	src/helios/queries/function_queries.cpp
	src/helios/queries/global_data_queries.cpp
    src/helios/scope_id.cpp
	src/helios/hout/hout.cpp
	src/helios/hout/elements/stmt.cpp
	src/helios/hout/elements/expr.cpp
	src/helios/hout/origin.cpp
	src/helios/mangler/mangler.cpp
	src/helios/test_utils/helios_test_utils.cpp
	src/helios/symbols/query_type_of_symbol.cpp
	src/helios/symbols/query_class_symbol_data.cpp
	src/helios/symbols/query_type_from_definition.cpp
	src/helios/symbols/query_class_of_member.cpp
	src/helios/symbols/symbol_abi.cpp
	src/helios/symbols/symbol_id.cpp
	src/helios/utils/get_expr_symid.cpp
	src/helios/utils/symbol_list.cpp
	src/helios/repl_utils/repl_queries.cpp
	src/helios/repl_utils/script_helpers.cpp
	src/helios/ls_utils/ls_utils.cpp
	src/helios/utils/go_to_definition.cpp
	src/helios/tsh/queries/implicit_coercibility.cpp
	src/helios/tsh/queries/types.cpp
	src/helios/tsh/abstract_type.cpp
	src/helios/tsh/deductions.cpp
	src/helios/tsh/types.cpp
	src/helios/tsh/type_interface.cpp
	src/helios/tsh/value_category.cpp
)

make_module(HELIOS USES Base QueryFramework PstParser ModuleTree Hashing CTV LibVM MIR LIR DVMBackend)

duck_add_test(compiler helios tests/helios_test.cpp
	USES Base PstParser HELIOS
	INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src_private")


duck_add_test(compiler helios_errors tests/helios_errors_test.cpp
	USES Base PstParser HELIOS
	INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src_private")

duck_add_test(compiler helios_stmt_compilation tests/helios_stmt_compilation_test.cpp
	USES Base PstParser HELIOS
	INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src_private")

duck_add_test(compiler helios_repl_instruction_wrapper tests/helios_repl_instruction_wrapper_test.cpp
	USES Base PstParser HELIOS
	INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src_private")

duck_add_test(compiler helios_repl_expression_wrapper tests/helios_repl_expression_wrapper_test.cpp
	USES Base PstParser HELIOS
	INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src_private")

duck_add_test(compiler helios_pst_layer tests/helios_pst_layer_test.cpp
	USES Base PstParser HELIOS
	INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src_private")

duck_add_test(compiler tsh_simple tests/tsh_simple_test.cpp USES HELIOS)

duck_add_test(compiler tsh_error tests/tsh_error_test.cpp USES HELIOS)

duck_add_test(compiler tsh_advanced tests/tsh_advanced_test.cpp USES HELIOS)
