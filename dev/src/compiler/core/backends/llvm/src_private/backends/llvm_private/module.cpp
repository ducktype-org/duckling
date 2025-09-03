#include "compile_llvm.hpp"
#include "llvm_includes/filesystem.hpp"
#include "llvm_includes/ir_verifier.hpp"
#include "llvm_lowering.hpp"
#include "module_impl.hpp"

#include <backends/llvm/llvm_backend.hpp>

#include <base/exceptions.hpp>

#include <iostream>

namespace compiler::backend_llvm {
	void deleteModuleImpl(ModuleImpl* ptr) {
		delete ptr;
	}
}

namespace compiler::backend_llvm {
	Module::Module(base::StrID module_id): impl(initModuleImpl(module_id)) {}

	Module Module::fromIRCode(std::string_view llvm_ir_code) {
		return { parseIRCodeToModuleImpl(llvm_ir_code) };
	}

	void Module::addFunctionToModule(query::Context& ctx, CRef<lir::Function> lir_function) {
		addFunctionToModuleImpl(ctx, impl.refMut(), lir_function);
	}

	void Module::addGlobalToModule(const lir::LirGlobal& lir_global) {
		addGlobalToModuleImpl(impl.refMut(), lir_global);
	}

	void Module::addFunctionToModuleCtors(query::Context& ctx, CRef<lir::Function> lir_function) {
		addFunctionToModuleCtorsImpl(ctx, impl.refMut(), lir_function);
	}

	void Module::addFunctionToModuleDtors(query::Context& ctx, CRef<lir::Function> lir_function) {
		addFunctionToModuleDtorsImpl(ctx, impl.refMut(), lir_function);
	}

	base::OkBad Module::verify() const {
		std::cerr << "LLVMVerification: \n";
		bool error_found = llvm::verifyModule(*impl->module, &llvm::errs());
		std::cerr << "\n";
		return error_found ? base::BAD : base::OK;
	}

	void Module::debugPrint() const { return impl->module->print(llvm::errs(), nullptr); }

	void Module::debugDumpToFile(base::StrID output_file) const {
		std::error_code error_code;
		llvm::raw_fd_ostream ir_output_stream(output_file.str(), error_code, llvm::sys::fs::OF_None);
		if (error_code) CORE_PANIC("LLVM error: unable to create file: " + error_code.message());

		impl->module->print(ir_output_stream, nullptr);
	}

	void Module::compile(
		const std::filesystem::path& output_file, CompilationOutputType output_type
	) {
		compileModuleToObject(impl.refMut(), output_file, output_type);
		CORE_ASSERT(std::filesystem::exists(output_file), "LLVM compilation to file failed!");
	}

	u64 Module::getFunctionCount(bool including_prototypes) const {
		const auto& func_list = impl->module->getFunctionList();
		u64         count     = 0;
		for (auto& func: func_list) {
			if (func.isDeclaration() and (not including_prototypes)) continue;
			count++;
		}
		return count;
	}

	Module::~Module() = default;
}
