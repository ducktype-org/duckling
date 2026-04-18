add_library(Linker
	src/linker/link.cpp
)

make_module(Linker USES Base SystemCommand HELIOS Artifacts Logger GlobalState CompilerTimeStats)
