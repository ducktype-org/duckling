add_library(Driver
	src_private/driver_private/backend_operations/compile_llvm.cpp
	src_private/driver_private/backend_operations/compile_dvm.cpp

	src_private/driver_private/operations.cpp
	src_private/driver_private/statistics_private/statistics.cpp
	
	src/driver/statistics/statistics.cpp
	src/driver/backend_type.cpp
	src/driver/operations/generic_operations.cpp
	src/driver/initialize.cpp
)

make_module(Driver USES Base QueryFramework SystemCommand HELIOS MIR LIR LLVMBackend DVMBackend LibVM Artifacts GlobalState Lexer Diagnostic Linker Timer)

duck_add_test(compiler driver tests/driver_test.cpp USES Driver Artifacts)
