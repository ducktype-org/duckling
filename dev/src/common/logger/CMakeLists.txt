add_library(Logger
	src/logger/logger.cpp
	src/logger/module_flags/module_flags.cpp
)

make_module(Logger USES Base Filesystem)
