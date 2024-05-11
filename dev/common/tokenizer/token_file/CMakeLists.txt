add_library(TokenFile
		src/token_file/file.cpp)

make_module(TokenFile USES Filesystem Lexer TPC)
