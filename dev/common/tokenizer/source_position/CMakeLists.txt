add_library(SourcePosition
		src/diagnostic/source_position.cpp)

make_module(SourcePosition USES Printer Filesystem)
