add_library(Driver
	src/driver/driver.cpp
)

make_module(Driver USES Base QueryFramework HELIOS MIR LIR LLVMBackend)

duck_add_test(compiler driver_test tests/driver_tests.cpp
	USES Base Driver)
