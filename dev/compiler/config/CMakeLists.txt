add_library(Config
	src/config/config.cpp
)

make_module(Config USES Base Clap Lexer Diagnostic)

duck_add_test(compiler config USES Config Tester)

