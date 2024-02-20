add_library(ModuleSystem
		src/module_system/import_to_path.cpp)

make_module(ModuleSystem USES Base PstParser)

rift_add_test(compiler module_system_test_simple tests/test.cpp
		USES Printer ModuleSystem PstParser Lexer)
