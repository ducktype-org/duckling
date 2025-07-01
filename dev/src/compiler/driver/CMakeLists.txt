add_library(Driver
	src_private/driver_private/package_compilation_driver.cpp
	src_private/driver_private/backend_driver/backend_driver.cpp
	src_private/driver_private/backend_driver/llvm_driver.cpp
	src_private/driver_private/backend_driver/dvm_driver.cpp

	src_private/driver_private/hout_to_binary_driver.cpp

	src/driver/driver.cpp
)

make_module(Driver USES Base QueryFramework SystemCommand HELIOS MIR LIR LLVMBackend DVMBackend LibVM Artifacts GlobalState Lexer Diagnostic)

duck_add_test(compiler driver tests/driver_test.cpp USES Driver Artifacts)
