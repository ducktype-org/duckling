#include "module_impl.hpp"

#include <iostream>

#include "llvm_includes/ir_verifier.hpp"

namespace compiler::backend_llvm {
	bool ModuleImpl::verify() const {
		// @TODO: does it verify all functions?
		std::cerr << "LLVMVerification: \n";
		bool error_found = llvm::verifyModule(*module, &llvm::errs());
		std::cerr << "\n";

		// bool error_found = llvm::verifyFunction(*fun, &llvm::errs());

		return not error_found;
	}

	void ModuleImpl::debugPrint() const {
	
		module->print(llvm::errs(), nullptr);
		// fun->print(llvm::outs());
	}

}
