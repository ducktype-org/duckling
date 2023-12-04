add_library(ErrorState
		src/error_state/error_state.cpp)

make_module(ErrorState USES SourcePosition Printer)
