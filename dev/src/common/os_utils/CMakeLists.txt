add_library(OSUtils
	src/os_utils/executable_path.cpp
	src/os_utils/exec_self.cpp
	src/os_utils/system_libraries.cpp
)

make_module(OSUtils USES Base Filesystem)
