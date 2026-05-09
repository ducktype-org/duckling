add_library(ModuleTree
	src/frontend/module_tree/access.cpp
	src/frontend/module_tree/module_flags/module_flags.cpp
	src/frontend/module_tree/module_tree.cpp
	src/frontend/module_tree/queries.cpp
	src/frontend/module_tree/source_file.cpp
	src/frontend/module_tree/file_id.cpp
	src/frontend/module_tree/module_id.cpp
)

make_module(ModuleTree USES Filesystem PstParser QueryFramework GlobalState Packages)

add_example(module_tree_example examples/module_tree_example.cpp USES ModuleTree)

duck_add_test(compiler module_tree tests/module_tree_test.cpp USES ModuleTree)
duck_add_test(compiler source_file tests/source_file_test.cpp USES ModuleTree)
