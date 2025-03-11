add_library(ModuleTree
	src/frontend/module_tree/module_tree.cpp
	src/frontend/module_tree/queries.cpp
)

make_module(ModuleTree USES Filesystem PstParser QueryFramework)

add_example(module_tree_example examples/module_tree_example.cpp USES ModuleTree)

duck_add_test(compiler module_tree USES ModuleTree)
