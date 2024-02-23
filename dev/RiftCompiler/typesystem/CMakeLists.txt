add_library(TypeSystem
		src/typesystem/class_types.cpp
		src/typesystem/type_info.cpp
		src/typesystem/implicit_coercibility.cpp
		src/typesystem/typesystem.cpp
		src/typesystem/internal/type_info_impl.cpp
		src/typesystem/internal/class_info_impl.cpp)

make_module(TypeSystem USES Base HIR Exec)

rift_add_test(compiler typesystem_test_simple tests/simple_test.cpp
		USES Base Clap HIR TypeSystem)
rift_add_test(compiler typesystem_test_new tests/new_test.cpp
		USES Base Clap HIR TypeSystem)
