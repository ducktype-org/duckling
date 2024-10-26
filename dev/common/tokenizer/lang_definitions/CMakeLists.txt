add_library(LexerDefinitions
	src/lang_definitions/key_spec_op.cpp
	src/lang_definitions/operator_precedence.cpp
)

make_module(LexerDefinitions USES Base)

duck_add_test(common lang_def_test_simple tests/test.cpp
	USES LexerDefinitions)
