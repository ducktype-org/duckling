add_library(OSUtils
	src/os_utils/executable_path.cpp
	src/os_utils/executable_path.hpp
	src/os_utils/exec_self.cpp
	src/os_utils/exec_self.hpp
	src/os_utils/memory.cpp
	src/os_utils/memory.hpp
	src/os_utils/dynamic_library.cpp
	src/os_utils/dynamic_library.hpp
	src/os_utils/terminal.cpp
	src/os_utils/terminal.hpp
	src/os_utils/timed_mutex_recovery.hpp
)

make_module(OSUtils USES Base Filesystem)
