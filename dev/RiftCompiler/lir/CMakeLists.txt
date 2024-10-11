add_library(LIR
	src/lir/lir_structure/lir_structure.cpp
	src/lir/lir_lowering/lir_lowering.cpp
)

make_module(LIR USES Base QueryFramework MIR TypeSystemLower)
