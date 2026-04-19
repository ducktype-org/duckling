add_library(TokenSource STATIC
		src/token_source/decode.cpp
		src/token_source/source.cpp)

make_module(TokenSource USES Filesystem Lexer TPC)
