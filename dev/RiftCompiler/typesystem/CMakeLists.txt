add_library(TypeSystem
	src/typesystem/class_types.cpp
	src/typesystem/implicit_coercibility.cpp
	src/typesystem/queries.cpp
	src/typesystem/type_info.cpp
	src/typesystem/types.cpp
	src/typesystem/typesystem.cpp
	src/typesystem/value_category.cpp
	src/typesystem/internal/type_info_impl.cpp
	src/typesystem/internal/class_info_impl.cpp)

make_module(TypeSystem USES Base HIR Exec QueryFramework)

rift_add_test(compiler typesystem_test_simple tests/simple_test.cpp USES TypeSystem)
