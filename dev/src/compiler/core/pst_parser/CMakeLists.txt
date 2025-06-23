# Here we find all the source files of elements implementations
# and group them into unity build groups.
# See https://cmake.org/cmake/help/latest/prop_tgt/UNITY_BUILD.html for more details.

file(GLOB StatementElementsImplementationFiles CONFIGURE_DEPENDS
	"src/pst_parser/elements/implementations/statements/*.cpp"	
)
set_source_files_properties(${StatementElementsImplementationFiles}
	PROPERTIES UNITY_GROUP "pst_statements"
)

file(GLOB DeclarationsElementsImplementationFiles CONFIGURE_DEPENDS
	"src/pst_parser/elements/implementations/declarations/*.cpp"
)
set_source_files_properties(${DeclarationsElementsImplementationFiles}
	PROPERTIES UNITY_GROUP "pst_declarations"
)

file(GLOB ExpressionElementsImplementationFiles CONFIGURE_DEPENDS
	"src/pst_parser/elements/implementations/expressions/*.cpp"
)
set_source_files_properties(${ExpressionElementsImplementationFiles}
	PROPERTIES UNITY_GROUP "pst_expressions"
)

file(GLOB ClassElementsImplementationFiles CONFIGURE_DEPENDS
	"src/pst_parser/elements/implementations/class_elements/*.cpp"
)
set_source_files_properties(${ClassElementsImplementationFiles}
	PROPERTIES UNITY_GROUP "pst_class_elements"
)

file(GLOB OtherElementsImplementationFiles CONFIGURE_DEPENDS
	"src/pst_parser/elements/implementations/meta/*.cpp"
	"src/pst_parser/elements/implementations/not_statements/*.cpp"
	"src/pst_parser/elements/implementations/lists/*.cpp"
)
set_source_files_properties(${OtherElementsImplementationFiles}
	PROPERTIES UNITY_GROUP "pst_other_elements"
)

add_library(PstParser
	src/pst_parser/lang_parser_base.cpp
	src/pst_parser/test_utils/pst_test_utils.cpp
	src/pst_parser/access.cpp
	src/pst_parser/pst.cpp
	src/pst_parser/pst_query/code_dependency.cpp
	src/pst_parser/elements/elements_common.cpp
	${StatementElementsImplementationFiles}
	${DeclarationsElementsImplementationFiles}
	${ExpressionElementsImplementationFiles}
	${ClassElementsImplementationFiles}
	${OtherElementsImplementationFiles}
	src/pst_parser/pst_visitor.hpp
)

set_target_properties(PstParser PROPERTIES UNITY_BUILD ON)
set_target_properties(PstParser PROPERTIES UNITY_BUILD_MODE GROUP)

make_module(PstParser USES Lexer Printer TPC QueryFramework)

duck_add_test(compiler parser tests/parser_test.cpp USES PstParser)
duck_add_test(compiler parser_builder tests/parser_builder_test.cpp USES PstParser)
duck_add_test(compiler parser_errors tests/parser_errors_test.cpp USES PstParser)
