add_library(Filesystem
            src/filesystem/file.cpp
            src/filesystem/encoding.hpp)

make_module(Filesystem USES Base)

rift_add_test(common filesystem_test_simple tests/test.cpp
	USES Filesystem)
