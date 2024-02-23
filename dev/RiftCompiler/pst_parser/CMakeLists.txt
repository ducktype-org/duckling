add_library(PstParser
		src/pst_parser/parser.cpp
		src/pst_parser/elements/elements.cpp
		src/pst_parser/elements/elements_impl.cpp
		src/pst_parser/pst.cpp
		src/pst_parser/rift_parser_base.cpp)

make_module(PstParser USES Lexer Printer TPC)

rift_add_test(compiler parser_test_simple tests/test.cpp
		USES PstParser)
