add_library(LIR
	src/lir/lir_structure.cpp
)

make_module(LIR USES Base QueryFramework MIR TypeSystemLower)
