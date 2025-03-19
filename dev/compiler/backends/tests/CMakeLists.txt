
duck_add_test_custom(compiler dvm_backend dvm_backend_test.cpp
	USES Base Tester DVMBackend)

duck_add_test_custom(compiler llvm_backend llvm_backend_test.cpp
	USES Base Tester LLVMBackend)
