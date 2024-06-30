add_library(Compiler
	src/compiler/compiler_config.cpp
)

make_module(Compiler USES Base Printer Clap PstParser ModuleSystem TypeSystem)
