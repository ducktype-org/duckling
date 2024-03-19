add_library(Filesystem
        src/filesystem/file.cpp
        src/filesystem/fs_parser.cpp
        src/filesystem/fs_parser.hpp
        src/filesystem/encoding.hpp)

make_module(Filesystem USES Base)

rift_add_test(common filesystem_test_simple tests/test.cpp
        USES Filesystem)
