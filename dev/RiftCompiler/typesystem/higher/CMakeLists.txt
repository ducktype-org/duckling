add_library(TypeSystem
        src/typesystem/higher/queries/implicit_coercibility.cpp
        src/typesystem/higher/queries/types.cpp
        src/typesystem/higher/type_info.cpp
        src/typesystem/higher/types.cpp
        src/typesystem/higher/type_interface.cpp
        src/typesystem/higher/typesystem.cpp
        src/typesystem/higher/value_category.cpp
        src/typesystem/higher/internal/type_info_impl.cpp
        src/typesystem/higher/internal/queries.cpp
)

# @FIXME: adding HELIOS dependency for SymID introduces cyclic dependency (but everything compiles).
make_module(TypeSystem USES Base HELIOS QueryFramework)
