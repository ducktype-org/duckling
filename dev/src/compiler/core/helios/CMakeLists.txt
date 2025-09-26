add_library(HELIOS STATIC
	src_private/helios_private/symbols/symbols.cpp
	src_private/helios_private/scopes/scopes.cpp
	src_private/helios_private/utils/pst_walkers.cpp
	src_private/helios_private/lookup/lookup_result.cpp
	src_private/helios_private/lookup/interface.cpp
	src_private/helios_private/lookup/lookup_chain.cpp
	src_private/helios_private/expressions/call_processing.cpp
	src_private/helios_private/expressions/chain_expr.cpp
	src_private/helios_private/expressions/query_hout_of_expr.cpp
	src_private/helios_private/expressions/builtin_operations.cpp
	src_private/helios_private/expressions/coercions.cpp
	src_private/helios_private/comp_time/comp_time.cpp
	src_private/helios_private/comp_time/vm_evaluator.cpp
	
	src/helios/queries.cpp
	src/helios/hout/hout.cpp
	src/helios/hout/elements/stmt.cpp
	src/helios/hout/elements/expr.cpp
	src/helios/mangler/mangler.cpp
	src/helios/test_utils/helios_test_utils.cpp
	src/helios/symbols/query_type_of_symbol.cpp
	src/helios/symbols/query_class_symbol_data.cpp
	src/helios/symbols/query_type_from_definition.cpp
	src/helios/symbols/symbol_abi.cpp
	src/helios/utils/go_to_definition.cpp
	src/helios/utils/get_expr_symid.cpp
	src/helios/utils/symbol_list.cpp
	src/helios/helios_errors.hpp
	src/helios/ctv/ctv.cpp
)

make_module(HELIOS USES Base QueryFramework PstParser ModuleTree TypeSystemHigher LibVM MIR LIR DVMBackend)

duck_add_test(compiler helios tests/helios_test.cpp 
	USES Base TypeSystemHigher PstParser HELIOS
	INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src_private")
