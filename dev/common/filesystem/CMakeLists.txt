add_library(Filesystem
	src/filesystem/file.cpp
	src/filesystem/fs_tree.cpp
	src/filesystem/fs_tree.hpp
	src/filesystem/encoding.hpp
)

make_module(Filesystem USES Base)

rift_add_test(common filesystem_test_simple tests/test.cpp
	USES Filesystem)
rift_add_test(common filesystem_test_fstree tests/fs_tree_test.cpp
	USES Filesystem)

add_example(filesystem_temp_path_example examples/temp_path_creation.cpp
	USES Filesystem)
