add_library(Frontend
        src/frontend/frontend.h
        src/frontend/fs_parser/fs_parser.cpp
        src/frontend/fs_parser/fs_parser.hpp
        src/frontend/module_tree/module_tree.cpp
        src/frontend/module_tree/module_tree.hpp)

make_module(Frontend USES Base Filesystem)
