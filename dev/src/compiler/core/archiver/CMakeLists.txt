add_library(Archiver
	src/archiver/archive.cpp
)

make_module(Archiver USES Base Filesystem SystemCommand HELIOS Artifacts Logger GlobalState CompilerTimeStats)