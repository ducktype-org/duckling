# This has to be static due to cyclic dependencies in helios->mir->lir->dvm for comp time, and helios<->TS cyclic dependency.
add_library(TypeSystemLower STATIC
        src/tsl/type_layout.cpp
        src/tsl/queries.cpp
        src/tsl/c_abi_converter.cpp
        src/tsl/c_abi_target.cpp
)

make_module(TypeSystemLower USES Base QueryFramework HELIOS ForeignAbiUtils)

duck_add_test(compiler tsl_simple tests/tsl_simple_test.cpp USES HELIOS)

duck_add_test(compiler c_abi_converter tests/c_abi_converter_test.cpp USES HELIOS TypeSystemLower)
