add_library(ModuleTree
		src/frontend/module_tree/module_tree.cpp
		src/frontend/module_tree/module_tree.hpp)

make_module(ModuleTree USES Filesystem)

add_example(module_tree_example ../../../docs/source-doc/source-docs/RiftCompiler/frontend/module_tree/module_tree_example.cpp USES ModuleTree)

rift_add_test(compiler frontend_test_module_tree tests/module_tree_tests.cpp
		USES ModuleTree)
