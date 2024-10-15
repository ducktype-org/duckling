add_library(Lexer
	src/lexer/char.cpp
	src/lexer/token.cpp
	src/lexer/lexer.cpp
	src/lexer/classifications.cpp
	src/lexer/decode.cpp
	src/lexer/lexer_class.cpp
)

make_module(Lexer USES Filesystem Printer LexerDefinitions unicode Result::Result Diagnostic)

duck_add_test(common lexer_test_simple tests/test.cpp
	USES Lexer LexerDefinitions)
duck_add_test(common token_test_position tests/token_position_test.cpp
	USES Lexer LexerDefinitions)
duck_add_test(common lexer_test_decode tests/decode_test.cpp
	USES Lexer LexerDefinitions)
