add_library(TypeSystemHigher
        src/typesystem/higher/queries/implicit_coercibility.cpp
        src/typesystem/higher/queries/types.cpp
        src/typesystem/higher/type_info.cpp
        src/typesystem/higher/types.cpp
        src/typesystem/higher/type_interface.cpp
        src/typesystem/higher/value_category.cpp
        src/typesystem/higher/all.cpp
        src/typesystem/higher/internal/type_info_impl.cpp
        src/typesystem/higher/internal/queries.cpp
)

# @FIXME: adding HELIOS dependency for SymID introduces cyclic dependency (but everything compiles).
make_module(TypeSystemHigher USES Base HELIOS QueryFramework)

rift_add_test(compiler tsh_simple_test tests/simple_test.cpp USES TypeSystemHigher)

rift_add_test(compiler tsh_error_test tests/error_test.cpp USES TypeSystemHigher)

rift_add_test(compiler tsh_overload_resolution_test tests/overload_resolution_test.cpp USES TypeSystemHigher HELIOS)
