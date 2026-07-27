add_library(OSUtils
	src/os_utils/executable_path.cpp
	src/os_utils/exec_self.cpp
	src/os_utils/exec_self.hpp
	src/os_utils/memory.cpp
	src/os_utils/dynamic_library.cpp
)

make_module(OSUtils USES Base Filesystem)
