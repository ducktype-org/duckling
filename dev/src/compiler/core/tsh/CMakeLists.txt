# This has to be static due to cyclic dependencies in helios->mir->lir->dvm for comp time, and helios<->TS cyclic dependency.
add_library(TypeSystemHigher STATIC
        src/tsh/queries/implicit_coercibility.cpp
        src/tsh/queries/types.cpp
        src/tsh/abstract_type.cpp
        src/tsh/deductions.cpp
        src/tsh/types.cpp
        src/tsh/type_interface.cpp
        src/tsh/value_category.cpp
        src_private/tsh/abstract_type_impl.cpp
        src_private/tsh/queries.cpp
)

# @TODO: #1392 See if we can relax this dependency.
# @TODO: #2331 Relax PST dependency
make_module(TypeSystemHigher USES Base PstParser HELIOS QueryFramework)

duck_add_test(compiler tsh_simple tests/tsh_simple_test.cpp USES TypeSystemHigher)

duck_add_test(compiler tsh_error tests/tsh_error_test.cpp USES TypeSystemHigher)

duck_add_test(compiler tsh_advanced tests/tsh_advanced_test.cpp USES TypeSystemHigher HELIOS)
