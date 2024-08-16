add_library(Config
	src/config/config.cpp
)

make_module(Config USES Base Clap Lexer Diagnostic)

rift_add_test(compiler config_test tests/test.cpp
	USES Config Tester)

