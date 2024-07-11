add_library(MIR
	src/mir/mir_lowering/mir_lowering.cpp
	src/mir/mir_structure/mir.cpp
)

make_module(MIR USES Base)
