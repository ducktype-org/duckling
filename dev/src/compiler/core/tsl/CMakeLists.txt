# This has to be static due to cyclic dependencies in helios->mir->lir->dvm for comp time, and helios<->TS cyclic dependency.
add_library(TypeSystemLower STATIC
        src/tsl/type_layout.cpp
        src/tsl/queries.cpp
)

make_module(TypeSystemLower USES Base QueryFramework HELIOS)

duck_add_test(compiler tsl_simple tests/tsl_simple_test.cpp USES HELIOS)
