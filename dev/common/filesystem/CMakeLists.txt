add_library(Filesystem
	src/filesystem/file.cpp
	src/filesystem/fs_tree.cpp
	src/filesystem/fs_tree.hpp
	src/filesystem/encoding.hpp
)

make_module(Filesystem USES Base)

duck_add_test(common filesystem USES Filesystem)
duck_add_test(common filesystem_tree USES Filesystem)

add_example(filesystem_temp_path_example examples/temp_path_creation.cpp
	USES Filesystem)
