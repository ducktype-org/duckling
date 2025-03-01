
#include <backends/vm/backend.hpp>


#define TESTER_CLASS DVMBackendTest
#define DVM_BACKEND_TEST
#include "backend_tests.hpp"

void DVMBackendTest::testWithLir(CRef<compiler::lir::Function> lir_function) {
	using namespace compiler::backend_vm;

	auto vm_module = Module(base::StrID("test_module"));
	vm_module.addLirFunction(lir_function);
	std::cerr << "Generated code:\n";
	vm_module.buildRepr(std::cerr);
}
