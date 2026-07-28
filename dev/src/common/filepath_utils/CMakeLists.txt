add_library(FilepathUtils
	src/filepath_utils/file_uri.cpp
)

make_module(FilepathUtils USES Base)
