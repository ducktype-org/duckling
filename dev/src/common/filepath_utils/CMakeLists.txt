add_library(FilepathUtils
	src/filepath_utils/file_uri.cpp
)

make_module(FilepathUtils USES Base)

duck_add_test(common filepath_utils tests/filepath_utils_test.cpp USES FilepathUtils Tester)
