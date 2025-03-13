add_library(MIR
	src/mir/mir_lowering/mir_lowering.cpp
	src/mir/mir_lowering/mir_lifetimes.cpp
	src/mir/mir_structure/mir_structure.cpp
)

make_module(MIR USES Base QueryFramework HELIOS)

duck_add_test(compiler mir tests/mir_test.cpp USES Base TypeSystemHigher PstParser HELIOS MIR)
