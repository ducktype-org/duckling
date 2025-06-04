add_library(Driver
	src/driver/hout_to_binary_driver.cpp
	src/driver/package_compilation_driver.cpp
	src/driver/backend_driver/backend_driver.cpp
	src/driver/backend_driver/llvm_driver.cpp
	src/driver/backend_driver/dvm_driver.cpp
)

make_module(Driver USES Base QueryFramework SystemCommand HELIOS MIR LIR LLVMBackend DVMBackend LibVM Artifacts)

duck_add_test(compiler driver tests/driver_test.cpp USES Driver)
