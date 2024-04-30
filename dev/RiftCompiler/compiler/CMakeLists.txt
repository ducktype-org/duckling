add_library(Compiler
	src/compiler/compiler_config.cpp
	src/compiler/compilation_handler.cpp
)

make_module(Compiler USES Base Printer Clap PstParser ModuleSystem HIR TypeSystem)

rift_add_test(compiler compiler_test_simple tests/test.cpp
	USES Printer ModuleSystem PstParser Lexer Compiler)
