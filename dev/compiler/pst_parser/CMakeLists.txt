file(GLOB ElementImplementationFiles 
	"src/pst_parser/elements/implementations/statements/*.cpp"
	"src/pst_parser/elements/implementations/meta/*.cpp"
	"src/pst_parser/elements/implementations/not_statements/*.cpp"
	"src/pst_parser/elements/implementations/declarations/*.cpp"
)

add_library(PstParser
	src/pst_parser/parser.cpp
	src/pst_parser/duckling_parser_base.cpp
	${ElementImplementationFiles}
	src/pst_parser/pst_visitor.hpp
)

make_module(PstParser USES Lexer Printer TPC)

cmake_add_test(compiler parser_test_simple tests/test.cpp
	USES PstParser)
cmake_add_test(compiler parser_test_builder tests/builder_tests.cpp
	USES PstParser)
cmake_add_test(compiler parser_test_errors tests/error_tests.cpp
	USES PstParser)
