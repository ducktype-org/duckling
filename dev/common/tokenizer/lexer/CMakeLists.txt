add_library(Lexer
	src/lexer/char.cpp
	src/lexer/token.cpp
	src/lexer/lexer.cpp
	src/lexer/classifications.cpp
	src/lexer/decode.cpp
	src/lexer/lexer_class.cpp
)

make_module(Lexer USES Filesystem Printer LexerDefinitions unicode Result::Result Diagnostic Init)

duck_add_test(common lexer USES Lexer LexerDefinitions)
duck_add_test(common token_position USES Lexer LexerDefinitions)
duck_add_test(common lexer_decode USES Lexer LexerDefinitions)
