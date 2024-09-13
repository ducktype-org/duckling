add_library(Exec
	src/exec/exec.cpp
	src/exec/ctv.cpp
	src/exec/operators/builtinoperators.cpp
	src/exec/vtable_creation.cpp
	src/exec/exec_default.cpp
)

make_module(Exec USES Base Operations)

rift_add_test(compiler exec_operators_test_simple tests/operators_test.cpp
	USES Exec Base Clap Operations TypeSystemHigher)

rift_add_test(compiler exec_ctv_test_simple tests/simple_ctv_test.cpp
	USES Exec Clap TypeSystemHigher)
rift_add_test(compiler exec_defaults_test_simple tests/simple_defaults_test.cpp
	USES Exec Clap TypeSystemHigher)
rift_add_test(compiler exec_constructor_test_simple tests/simple_constructor_test.cpp
	USES Exec Clap TypeSystemHigher)
