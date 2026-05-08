add_library(Packages
	src/frontend/packages/packages.cpp
)

make_module(Packages USES ModuleTree JSON Base)
