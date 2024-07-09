add_library(CompilerLunacher
	src/compiler_luancher/lunacher.cpp
)

make_module(CompilerLunacher USES Base Clap)
