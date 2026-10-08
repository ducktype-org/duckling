# Here we find all the source files of elements implementations
# and group them into unity build groups.
# See https://cmake.org/cmake/help/latest/prop_tgt/UNITY_BUILD.html for more details.

set(PST_SRC "src/frontend/pst_parser")

file(GLOB StatementElementsImplementationFiles CONFIGURE_DEPENDS
	"${PST_SRC}/elements/implementations/statements/*.cpp"	
)
set_source_files_properties(${StatementElementsImplementationFiles}
	PROPERTIES UNITY_GROUP "pst_statements"
)

file(GLOB DeclarationsElementsImplementationFiles CONFIGURE_DEPENDS
	"${PST_SRC}/elements/implementations/declarations/*.cpp"
)
set_source_files_properties(${DeclarationsElementsImplementationFiles}
	PROPERTIES UNITY_GROUP "pst_declarations"
)

file(GLOB ExpressionElementsImplementationFiles CONFIGURE_DEPENDS
	"${PST_SRC}/elements/implementations/expressions/*.cpp"
)
set_source_files_properties(${ExpressionElementsImplementationFiles}
	PROPERTIES UNITY_GROUP "pst_expressions"
)

file(GLOB ClassElementsImplementationFiles CONFIGURE_DEPENDS
	"${PST_SRC}/elements/implementations/class_elements/*.cpp"
)
set_source_files_properties(${ClassElementsImplementationFiles}
	PROPERTIES UNITY_GROUP "pst_class_elements"
)

file(GLOB OtherElementsImplementationFiles CONFIGURE_DEPENDS
	"${PST_SRC}/elements/implementations/expr_holders.cpp"
	"${PST_SRC}/elements/implementations/meta/*.cpp"
	"${PST_SRC}/elements/implementations/not_statements/*.cpp"
	"${PST_SRC}/elements/implementations/not_statements/synthetic_elements/*.cpp"
	"${PST_SRC}/elements/implementations/not_statements/patterns/*.cpp"
	"${PST_SRC}/elements/implementations/not_statements/wrapper_elements/*.cpp"
	"${PST_SRC}/elements/implementations/not_statements/format_string_sub_elements/*.cpp"
	"${PST_SRC}/elements/implementations/lists/*.cpp"
)
set_source_files_properties(${OtherElementsImplementationFiles}
	PROPERTIES UNITY_GROUP "pst_other_elements"
)

add_library(PstParser
	${PST_SRC}/lang_parser_element.cpp
	${PST_SRC}/lang_parser_state.cpp
	${PST_SRC}/lang_parser_element_construction_data.cpp
	${PST_SRC}/source_position_locked.cpp
	${PST_SRC}/test_utils/pst_test_utils.cpp
	${PST_SRC}/access.cpp
	${PST_SRC}/utility.cpp
	${PST_SRC}/pst.cpp
	${PST_SRC}/pst_automatic.cpp
	${PST_SRC}/pst_query/code_dependency.cpp
	${PST_SRC}/elements/elements_common.cpp
	${PST_SRC}/elements/lang_state_unmethods.cpp
	${StatementElementsImplementationFiles}
	${DeclarationsElementsImplementationFiles}
	${ExpressionElementsImplementationFiles}
	${ClassElementsImplementationFiles}
	${OtherElementsImplementationFiles}
	${PST_SRC}/pst_visitor.hpp
)

if (NOT DISABLE_UNITY_COMPILATION)
	message("-- PstParser will use UNITY compilation")
	set_target_properties(PstParser PROPERTIES UNITY_BUILD ON)
	set_target_properties(PstParser PROPERTIES UNITY_BUILD_MODE GROUP)
else() # Unity compilation is disabled by default.
	message("-- PstParser will NOT use UNITY compilation, compilation will be longer")
endif()

make_module(PstParser USES Diagnostic Hashing Lexer UnicodeClassification Printer TPC QueryFramework CompilerTimeStats)

duck_add_test(compiler parser tests/parser_test.cpp USES PstParser)
duck_add_test(compiler parser_builder tests/parser_builder_test.cpp USES PstParser)
duck_add_test(compiler parser_errors tests/parser_errors_test.cpp USES PstParser)

duck_add_test(compiler extract_single_expression tests/extract_single_expression_test.cpp USES PstParser)
duck_add_test(compiler extract_single_instruction tests/extract_single_instruction_test.cpp USES PstParser)
duck_add_test(compiler extract_single_definition tests/extract_single_definition_test.cpp USES PstParser)
