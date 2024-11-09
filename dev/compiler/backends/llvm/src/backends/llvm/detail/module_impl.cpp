#include "module_impl.hpp"
#include "../llvm_backend.hpp"

#include <iostream>

#include "llvm_includes/ir_verifier.hpp"

namespace base::extend {
	void BoxPtrDeleter<compiler::backend_llvm::ModuleImpl>::del(
		compiler::backend_llvm::ModuleImpl* ptr
	) {
		delete ptr;
	}
}

namespace compiler::backend_llvm {

	bool Module::verify() const { return impl->verify(); }

	void Module::debugPrint() const { return impl->debugPrint(); }

	bool ModuleImpl::verify() const {
		// @TODO: does it verify all functions?
		std::cerr << "LLVMVerification: \n";
		bool error_found = llvm::verifyModule(*module, &llvm::errs());
		std::cerr << "\n";

		return not error_found;
	}

	void ModuleImpl::debugPrint() const {
		module->print(llvm::errs(), nullptr);
	}

	Module::~Module()         = default;
	ModuleImpl::~ModuleImpl() = default;
}
