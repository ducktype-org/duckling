add_library(Filesystem
	src/filesystem/file.cpp
	src/filesystem/file_path.cpp
	src_private/filesystem_private/vfs.cpp
	src/filesystem/encoding.hpp
)

make_module(Filesystem USES Base FilepathUtils)

duck_add_test(common filesystem tests/filesystem_test.cpp USES Filesystem
	INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src_private")

add_example(filesystem_temp_path_example examples/temp_path_creation.cpp
	USES Filesystem)
