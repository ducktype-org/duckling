add_library(Driver
	src/driver/driver.cpp
)

make_module(Driver USES Base QueryFramework SystemCommand HELIOS MIR LIR LLVMBackend)

duck_add_test(compiler driver USES Driver)
