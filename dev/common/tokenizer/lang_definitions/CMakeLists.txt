add_library(LexerDefinitions
	src/lang_definitions/key_spec_op.cpp
	src/lang_definitions/operator_precedence.cpp
)

make_module(LexerDefinitions USES Base)

duck_add_test(common lang_definitions USES LexerDefinitions)
