add_library(Packages
	src/frontend/packages/access.cpp
	src/frontend/packages/packages.cpp
	src/frontend/packages/standard_packages.cpp
	src_private/queries.cpp
)

make_module(Packages USES ModuleTree JSON Base OSUtils)

duck_add_test(compiler packages tests/packages_test.cpp USES Packages)
