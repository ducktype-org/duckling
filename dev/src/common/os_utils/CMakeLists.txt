add_library(OSUtils
	src/os_utils/executable_path.cpp
	src/os_utils/exec_self.cpp
	src/os_utils/exec_self.hpp
	src/os_utils/system_libraries.cpp
	src/os_utils/system_libraries.hpp
)

make_module(OSUtils USES Base Filesystem)
