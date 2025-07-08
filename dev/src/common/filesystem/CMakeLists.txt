add_library(Filesystem
	src/filesystem/file.cpp
	src_private/filesystem_private/vfs.cpp
	src/filesystem/fs_tree.cpp
	src/filesystem/fs_tree.hpp
	src/filesystem/encoding.hpp
)

make_module(Filesystem USES Base)

duck_add_test(common filesystem tests/filesystem_test.cpp USES Filesystem
	INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src_private")
duck_add_test(common filesystem_tree tests/filesystem_tree_test.cpp USES Filesystem)

add_example(filesystem_temp_path_example examples/temp_path_creation.cpp
	USES Filesystem)
