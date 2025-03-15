
duck_add_test(compiler dvm_backend_tests dvm_backend_tests.cpp
	USES Base Tester DVMBackend)

duck_add_test(compiler llvm_backend_tests llvm_backend_tests.cpp
	USES Base Tester LLVMBackend)
