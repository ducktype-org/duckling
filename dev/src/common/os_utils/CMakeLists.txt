add_library(OSUtils
	src/os_utils/dynamic_library.cpp
	src/os_utils/exec_self.cpp
	src/os_utils/executable_path.cpp
	src/os_utils/memory.cpp
	src/os_utils/system_libraries.cpp
	src/os_utils/terminal.cpp
)

make_module(OSUtils USES Base Filesystem)

duck_add_test(common os_utils tests/os_utils_test.cpp USES OSUtils Tester)
