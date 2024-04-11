add_library(HELIOS
	src/symbols/symbols.cpp
	src/scopes/scopes.cpp
)

make_module(HELIOS USES Base)

