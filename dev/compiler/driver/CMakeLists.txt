add_library(Driver
	src/driver/driver.cpp
	src/driver/llvm_driver.cpp
	src/driver/dvm_driver.cpp
)

make_module(Driver USES Base QueryFramework SystemCommand HELIOS MIR LIR LLVMBackend DVMBackend)

duck_add_test(compiler driver tests/driver_test.cpp USES Driver)
