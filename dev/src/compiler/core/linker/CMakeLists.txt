add_library(Linker
	src/linker/link.cpp
)

make_module(Linker USES Base Filesystem SystemCommand HELIOS Artifacts Logger GlobalState CompilerTimeStats)
