add_library(HELIOS
	src/symbols/symbols.cpp
	src/scopes/scopes.cpp
	src/scopes/scopes.hpp
	src/pst_walkers.cpp
)

make_module(HELIOS USES Base QueryFramework PstParser ModuleTree)

