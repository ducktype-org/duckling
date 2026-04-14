# This has to be static due to cyclic dependencies in helios->mir->lir->dvm for comp time.
add_library(LIR STATIC
	src/lir/lir_structure/lir_structure.cpp
	src/lir/lir_lowering/lir_lowering.cpp
)

make_module(LIR USES Base QueryFramework MIR TypeSystemLower)

duck_add_test(compiler lir tests/lir_test.cpp USES Base PstParser HELIOS MIR LIR)
