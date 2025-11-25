add_library(Logger
	src/logger/logger.cpp
	src/logger/configuration/configuration.cpp
)

make_module(Logger USES Base)
