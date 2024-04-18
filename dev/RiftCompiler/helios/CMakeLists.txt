add_library(HELIOS
	src/helios/symbols/symbols.cpp
	src/helios/scopes/scopes.cpp
	src/helios/scopes/scopes.hpp
	src/helios/pst_walkers.cpp
)

make_module(HELIOS USES Base QueryFramework PstParser ModuleTree)

