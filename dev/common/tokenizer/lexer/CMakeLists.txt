add_library(Lexer
	src/lexer/char.cpp
	src/lexer/token.cpp
	src/lexer/lexer.cpp
	src/lexer/classifications.cpp
	src/lexer/token_common.cpp
	src/lexer/decode.cpp
	src/lexer/lexer_class.cpp
)

make_module(Lexer USES Filesystem Printer LexerDefinitions unicode Diagnostic Init)

duck_add_test(common lexer tests/lexer_test.cpp USES Lexer LexerDefinitions)
duck_add_test(common token_position tests/token_position_test.cpp USES Lexer LexerDefinitions)
duck_add_test(common lexer_decode tests/lexer_decode_test.cpp USES Lexer LexerDefinitions)
