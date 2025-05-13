file(GLOB ElementImplementationFiles CONFIGURE_DEPENDS
	"src/pst_parser/elements/implementations/statements/*.cpp"
	"src/pst_parser/elements/implementations/meta/*.cpp"
	"src/pst_parser/elements/implementations/not_statements/*.cpp"
	"src/pst_parser/elements/implementations/declarations/*.cpp"
	"src/pst_parser/elements/implementations/class_elements/*.cpp"
	"src/pst_parser/elements/implementations/expressions/*.cpp"
	"src/pst_parser/elements/implementations/lists/*.cpp"
)

add_library(PstParser
	src/pst_parser/lang_parser_base.cpp
	src/pst_parser/test_utils/pst_test_utils.cpp
	src/pst_parser/access.cpp
	src/pst_parser/pst_query/code_dependency.cpp

	${ElementImplementationFiles}
	src/pst_parser/pst_visitor.hpp
)

make_module(PstParser USES Lexer Printer TPC QueryFramework)

duck_add_test(compiler parser tests/parser_test.cpp USES PstParser)
duck_add_test(compiler parser_builder tests/parser_builder_test.cpp USES PstParser)
duck_add_test(compiler parser_errors tests/parser_errors_test.cpp USES PstParser)
