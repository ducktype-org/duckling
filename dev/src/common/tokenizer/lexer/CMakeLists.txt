add_library(Lexer STATIC
	src/lexer/char.cpp
	src/lexer/token.cpp
	src/lexer/lexer.cpp
	src/lexer/token_common.cpp
	src/lexer/lexer_class.cpp
	src/lexer/lexer_class_handlers.cpp
	src/lexer/lexer_class_utils.cpp
	src/lexer/lexer_class_conditions.cpp
)

make_module(Lexer USES Filesystem Printer LexerDefinitions UnicodeClassification Diagnostic Init StringID Logger)

duck_add_test(common lexer tests/lexer_test.cpp USES Lexer LexerDefinitions)
duck_add_test(common token_position tests/token_position_test.cpp USES Lexer LexerDefinitions)
duck_add_test(common lexer_decode tests/lexer_decode_test.cpp USES Lexer LexerDefinitions)
duck_add_test(common lexer_errors tests/lexer_errors_test.cpp USES Lexer LexerDefinitions TokenSource)
