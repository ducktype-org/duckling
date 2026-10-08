add_library(Filesystem
	src/filesystem/file.cpp
	src/filesystem/file_path.cpp
	src/filesystem/vfs.cpp
	src/filesystem/encoding.hpp
)

make_module(Filesystem USES Base FilepathUtils)

duck_add_test(common filesystem tests/filesystem_test.cpp USES Filesystem)

add_example(filesystem_temp_path_example examples/temp_path_creation.cpp
	USES Filesystem)
