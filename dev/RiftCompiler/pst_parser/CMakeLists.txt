file(GLOB ElementImplementationFiles CONFIGURE_DEPENDS 
	"src/pst_parser/elements/implementations/statements/*.cpp"
	"src/pst_parser/elements/implementations/meta/*.cpp"
	"src/pst_parser/elements/implementations/not_statements/*.cpp"
	"src/pst_parser/elements/implementations/declarations/*.cpp"
	"src/pst_parser/elements/implementations/class_elements/*.cpp"
)

add_library(PstParser
	src/pst_parser/parser.cpp
	src/pst_parser/rift_parser_base.cpp

	# LSP files are commented out for merge purposes:
	# src/pst_parser/lsp_elements/lsp_elements.cpp
	# src/pst_parser/lsp_elements/lsp_constructors.cpp


	${ElementImplementationFiles}
	src/pst_parser/pst_visitor.hpp
)

make_module(PstParser USES Lexer Printer TPC)

rift_add_test(compiler parser_test_simple tests/test.cpp
	USES PstParser)
rift_add_test(compiler parser_test_builder tests/builder_tests.cpp
	USES PstParser)
rift_add_test(compiler parser_test_errors tests/error_tests.cpp
	USES PstParser)
