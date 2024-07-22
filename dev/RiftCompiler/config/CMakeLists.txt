add_library(Config
	src/config/config.cpp
)

make_module(Config USES Base Clap Lexer Diagnostic)

# @TODO: add test