add_library(ModuleTree
	src/frontend/packages/packages.cpp
)

make_module(Packages USES GlobalState ModuleTree Diagnostic JSON Base)

add_example(packages_example examples/packages_example.cpp USES Packages)

duck_add_test(compiler packages tests/packages_test.cpp USES Packages)
