add_library(HELIOS
	src/symbols/symbols.cpp
	src/scopes/scopes.cpp
	src/scopes/scopes.hpp
)

make_module(HELIOS USES Base QueryFramework PstParser ModuleTree)

