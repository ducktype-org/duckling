add_library(TPC STATIC
	src/token_parser_core/parser_state.cpp
	src/token_parser_core/token_stream.cpp
	src/token_parser_core/base_element.cpp
	src/token_parser_core/debug_print.cpp
)

make_module(TPC USES Printer Base Lexer Diagnostic)
