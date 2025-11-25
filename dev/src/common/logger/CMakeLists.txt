add_library(Logger
	src/printer/logger.cpp
)

make_module(Logger USES Base)
