add_library(OSUtils
	src/os_utils/executable_path.cpp
)

make_module(OSUtils USES Base Filesystem)
