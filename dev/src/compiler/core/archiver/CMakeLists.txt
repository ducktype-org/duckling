add_library(Archiver
	src/archiver/archive.cpp
)

make_module(Archiver USES Base SystemCommand HELIOS Artifacts Logger GlobalState CompilerTimeStats)