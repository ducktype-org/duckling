add_library(HELIOS
	src/helios/symbols/symbols.cpp
	src/helios/scopes/scopes.cpp
	src/helios/scopes/scopes.hpp
	src/helios/pst_walkers.cpp
	src/helios/queries.cpp
	src/helios/lookup_result.cpp
	src/helios/ts/ts.cpp
)

make_module(HELIOS USES Base QueryFramework PstParser ModuleTree TypeSystem)

rift_add_test(compiler helios_test tests/helios_tests.cpp
	USES Base TypeSystem PstParser HELIOS)
