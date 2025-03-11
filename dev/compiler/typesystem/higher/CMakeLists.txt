add_library(TypeSystemHigher
        src/typesystem/higher/queries/implicit_coercibility.cpp
        src/typesystem/higher/queries/types.cpp
        src/typesystem/higher/abstract_type.cpp
        src/typesystem/higher/types.cpp
        src/typesystem/higher/type_interface.cpp
        src/typesystem/higher/value_category.cpp
        src/typesystem/higher/all.cpp
        src/typesystem/higher/internal/abstract_type_impl.cpp
        src/typesystem/higher/internal/queries.cpp
)

# @FIXME: adding HELIOS dependency for SymID introduces cyclic dependency (but everything compiles).
make_module(TypeSystemHigher USES Base HELIOS QueryFramework)

duck_add_test(compiler tsh_simple USES TypeSystemHigher)

duck_add_test(compiler tsh_error USES TypeSystemHigher)

duck_add_test(compiler tsh_overload_resolution USES TypeSystemHigher HELIOS)
