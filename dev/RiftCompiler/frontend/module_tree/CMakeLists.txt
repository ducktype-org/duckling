add_library(ModuleTree
        src/frontend/module_tree/module_tree.cpp
        src/frontend/module_tree/module_tree.hpp)

make_module(ModuleTree USES Base Filesystem)
