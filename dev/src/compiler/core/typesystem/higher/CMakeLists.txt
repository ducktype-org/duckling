add_library(TypeSystemHigher STATIC
        src/typesystem/higher/queries/implicit_coercibility.cpp
        src/typesystem/higher/queries/types.cpp
        src/typesystem/higher/abstract_type.cpp
        src/typesystem/higher/types.cpp
        src/typesystem/higher/type_interface.cpp
        src/typesystem/higher/value_category.cpp
        src_private/typesystem/higher/abstract_type_impl.cpp
        src_private/typesystem/higher/queries.cpp
)

# @FIXME: adding HELIOS dependency for SymID introduces cyclic dependency (but everything compiles).
make_module(TypeSystemHigher USES Base HELIOS QueryFramework)

duck_add_test(compiler tsh_simple tests/tsh_simple_test.cpp USES TypeSystemHigher)

duck_add_test(compiler tsh_error tests/tsh_error_test.cpp USES TypeSystemHigher)

duck_add_test(compiler tsh_advanced tests/tsh_advanced_test.cpp USES TypeSystemHigher HELIOS)
