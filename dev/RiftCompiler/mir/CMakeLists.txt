add_library(MIR
	src/mir/mir_lowering/mir_lowering.cpp
	src/mir/mir_structure/mir_structure.cpp
	src/mir/mir.cpp
)

make_module(MIR USES Base)
