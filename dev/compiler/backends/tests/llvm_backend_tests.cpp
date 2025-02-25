#define TESTER_CLASS LLVMBackendTest
#define LLVM_BACKEND_TEST

#include <backends/llvm/llvm_backend.hpp>
#include "backend_tests.hpp"

void LLVMBackendTest::testWithLir(CRef<compiler::lir::Function> lir_function) {
	auto llvm_module = compiler::backend_llvm::lirFunctionToModule(lir_function);

	// debug print for coverage only:
	llvm_module.debugPrint();

	// this is were the main part ot test is:
	assertTrue(llvm_module.verify(), "LLVM module verification failed");
}
