add_library(TypeSystem
	src/typesystem/queries/implicit_coercibility.cpp
	src/typesystem/queries/types.cpp
	src/typesystem/type_info.cpp
	src/typesystem/types.cpp
	src/typesystem/type_interface.cpp
	src/typesystem/typesystem.cpp
	src/typesystem/value_category.cpp
	src/typesystem/internal/type_info_impl.cpp
	src/typesystem/internal/queries.cpp
)

# @FIXME: adding HELIOS dependency for SymID introduces cyclic dependency (but everything compiles).
make_module(TypeSystem USES Base HELIOS QueryFramework)

duckling_add_test(compiler typesystem_test_simple tests/simple_test.cpp USES TypeSystem)

duckling_add_test(compiler typesystem_test_error tests/error_test.cpp USES TypeSystem)
