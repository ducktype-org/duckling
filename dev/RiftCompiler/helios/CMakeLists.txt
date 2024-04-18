add_library(HELIOS
        src/symbols/symbols.cpp
        src/scopes/scopes.cpp
        src/scopes/scopes.hpp
)

make_module(HELIOS USES Base QueryFramework PstParser ModuleTree)

rift_add_test(compiler HeliosTests tests/helios_tests.cpp
        USES Base TypeSystem PstParser HELIOS)
