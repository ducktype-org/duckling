add_library(Config
	src/config/config.cpp
)

make_module(Config USES Base Clap Lexer Diagnostic)

duck_add_test(compiler config_test tests/test.cpp
	USES Config Tester)

