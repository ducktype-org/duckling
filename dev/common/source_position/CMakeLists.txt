add_library(SourcePosition
		src/source_position/source_position.cpp)

make_module(SourcePosition USES Printer Filesystem)
