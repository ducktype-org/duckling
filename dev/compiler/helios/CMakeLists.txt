add_library(HELIOS
	src_private/helios_private/symbols/symbols.cpp
	src_private/helios_private/scopes/scopes.cpp
	src_private/helios_private/utils/pst_walkers.cpp
	src_private/helios_private/lookup_utils/lookup_result.cpp
	src_private/helios_private/query_hout_of_expr.cpp
	src_private/helios_private/comp_time/type_eval.cpp
	src_private/helios_private/comp_time/int_eval.cpp
	
	src/helios/queries.cpp
	src/helios/hout/hout.cpp
	src/helios/hout/elements/stmt.cpp
	src/helios/hout/elements/expr.cpp
	src/helios/test_utils/helios_test_utils.cpp
	src/helios/symbols/query_type_of_symbol.cpp
	src/helios/symbols/query_class_symbol_data.cpp
	src/helios/helios_errors.hpp
	src/helios/helios_result.hpp
)

make_module(HELIOS USES Base QueryFramework PstParser ModuleTree TypeSystemHigher)

duck_add_test(compiler helios tests/helios_test.cpp 
	USES Base TypeSystemHigher PstParser HELIOS
	INCLUDES "src_private")
