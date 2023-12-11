add_library(ErrorState
		src/diagnostic/error_state.cpp)

make_module(ErrorState USES SourcePosition Printer)
