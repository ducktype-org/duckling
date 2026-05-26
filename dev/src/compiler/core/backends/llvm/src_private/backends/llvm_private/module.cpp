#include "compile_llvm.hpp"
#include "llvm_includes/filesystem.hpp"
#include "llvm_includes/ir_verifier.hpp"
#include "llvm_lowering.hpp"
#include "module_impl.hpp"

#include <backends/llvm/llvm_backend.hpp>

#include <base/except/exceptions.hpp>

#include <logger/logger.hpp>

#include <iostream>

namespace compiler::backend_llvm {
	Module::Module(const base::StrID module_id): impl(initModuleImpl(module_id)) {}

	Module Module::fromIRCode(std::string_view llvm_ir_code) {
		return { parseIRCodeToModuleImpl(llvm_ir_code) };
	}

	Module Module::fromLLVMBC(const std::span<unsigned char> llvm_bc_data) {
		return { parseLLVMBCToModuleImpl(llvm_bc_data) };
	}

	void Module::addFunctionToModule(query::Context& ctx, CRef<lir::Function> lir_function) {
		addFunctionToModuleImpl(ctx, impl.refMut(), lir_function);
	}

	void Module::addGlobalToModule(const lir::LIRGlobalData& lir_global) {
		addGlobalToModuleImpl(impl.refMut(), lir_global);
	}

	void Module::addFunctionToModuleCtors(query::Context& ctx, CRef<lir::Function> lir_function) {
		addFunctionToModuleCtorsImpl(ctx, impl.refMut(), lir_function);
	}

	void Module::addFunctionToModuleDtors(query::Context& ctx, CRef<lir::Function> lir_function) {
		addFunctionToModuleDtorsImpl(ctx, impl.refMut(), lir_function);
	}

	base::OkBad Module::verify() const {
		std::string              llvm_verification;
		llvm::raw_string_ostream llvm_verification_stream(llvm_verification);

		bool error_found = llvm::verifyModule(*impl->module, &llvm_verification_stream);

		if (error_found) {
			CORE_DEV_LOG(Backend, dumpLLVMToString());
			CORE_DEV_LOG(Backend, "LLVM Verification Failed!: ", "\n", llvm_verification, "\n");
		}

		return error_found ? base::BAD : base::OK;
	}

	void Module::debugPrint() const { return impl->module->print(llvm::errs(), nullptr); }

	void Module::dumpLLVMToFile(base::StrID output_file) const {
		std::error_code error_code;
		llvm::raw_fd_ostream ir_output_stream(output_file.str(), error_code, llvm::sys::fs::OF_None);
		if (error_code) CORE_PANIC("LLVM error: unable to create file: " + error_code.message());

		impl->module->print(ir_output_stream, nullptr);
	}

	std::string Module::dumpLLVMToString() const {
		std::string              buffer;
		llvm::raw_string_ostream stream(buffer);

		impl->module->print(stream, nullptr);
		stream.flush();
		return buffer;
	}

	void Module::compile(
		const std::filesystem::path& output_file, const CompilationOutputType output_type
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

DEFAULT_BOX_PTR_DELETER_DEFINITION(compiler::backend_llvm::ModuleImpl)
