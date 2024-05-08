add_library(Operations
	src/operations/operation.cpp
	src/operations/create_default.cpp
	src/operations/internal/builtin_operations.cpp
)

make_module(Operations USES TypeSystem Exec Base)

rift_add_test(compiler operations_test_simple tests/simple_test.cpp
	USES Base Clap Operations TypeSystem)
rift_add_test(compiler operation_constructor tests/constructors_test.cpp
	USES Base Clap Operations TypeSystem)
