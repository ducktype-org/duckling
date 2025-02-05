add_library(HELIOS
	src/helios/symbols/symbols.cpp
	src/helios/scopes/scopes.cpp
	src/helios/scopes/scopes.hpp
	src/helios/pst_walkers.cpp
	src/helios/queries.cpp
	src/helios/lookup_result.cpp
	src/helios/hout/hout.cpp
	src/helios/hout/elements/stmt.cpp
	src/helios/hout/elements/expr.cpp
	src/helios/hout/elements/query_hout_of_expr.cpp
	src/helios/comp_time/type_eval.cpp
	src/helios/comp_time/int_eval.cpp
	src/helios/helios_errors.hpp
	src/helios/test_utils/helios_test_utils.cpp
	src/helios/helios_result.hpp
)

make_module(HELIOS USES Base QueryFramework PstParser ModuleTree TypeSystemHigher)

duck_add_test(compiler helios_test tests/helios_tests.cpp
	USES Base TypeSystemHigher PstParser HELIOS)
