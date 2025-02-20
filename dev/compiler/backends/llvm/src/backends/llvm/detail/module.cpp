#include <iostream>

#include "llvm_includes/ir_verifier.hpp"
#include "llvm_lowering.hpp"
#include "../llvm_backend.hpp"
#include "module_impl.hpp"
#include "compile_llvm.hpp"

namespace base::extend {
	void BoxPtrDeleter<compiler::backend_llvm::ModuleImpl>::del(
		compiler::backend_llvm::ModuleImpl* ptr
	) {
		delete ptr;
	}
}

namespace compiler::backend_llvm {
	Module::Module(base::StrID module_id): impl(initModuleImpl(module_id)) {}

	void Module::addFunctionToModule(CRef<lir::Function> lir_function) {
		addFunctionToModuleImpl(impl.refMut(), lir_function);
	}

	bool Module::verify() const {
		std::cerr << "LLVMVerification: \n";
		bool error_found = llvm::verifyModule(*impl->module, &llvm::errs());
		std::cerr << "\n";
		return not error_found;
	}

	void Module::debugPrint() const { return impl->module->print(llvm::errs(), nullptr); }

	void Module::compile(const ModuleCompilationOptions& options) {
		compileModuleToObject(impl.refMut(), options);
	}

	Module::~Module() = default;
}
