add_library(LIR
	src/lir/lir_structure/lir_structure.cpp
	src/lir/lir_lowering/lir_lowering.cpp
)

make_module(LIR USES Base QueryFramework MIR TypeSystemLower)

duck_add_test(compiler lir_test tests/lir_tests.cpp
	USES Base TypeSystemHigher PstParser HELIOS MIR LIR)
