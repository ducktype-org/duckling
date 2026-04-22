#include "../cf_analyzer.hpp"
#include "../jit_compiler.hpp"
#include "jit_utils.hpp"
#include "llvm_lowering.hpp"
#include "opcodes_bitcode_source.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/safe/low_program/instruction.hpp>

LLVM_INCLUDE_BEGIN()

#include <llvm/Demangle/Demangle.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>

LLVM_INCLUDE_END()

#include <string>

namespace vm::jit {

	MRef<JitOpFun> compileLLVM(const low::LowFuncData& function_to_compile) {
		llvm::orc::ThreadSafeContext& tsctx          = *llvmGetTSCtx();
		static u64                    compile_serial = 0;
		const std::string             symbol_name
			= std::to_string(function_to_compile.name.getInnerID().asInt()) + "."
		    + std::to_string(compile_serial++);
		std::unique_ptr<llvm::Module> new_module;

		llvm::LLVMContext& ctx = *tsctx.getContext();

		new_module = setupModule(symbol_name, ctx);

		LLVMBuilder(new_module.get(), ctx).lowerFunction(function_to_compile);

		auto&                       lljit = *llvmGetLljit();
		llvm::orc::ThreadSafeModule tsm(std::move(new_module), tsctx);
		if (auto err = lljit.addIRModule(std::move(tsm)))
			llvm::logAllUnhandledErrors(
				std::move(err), llvm::errs(), "Error adding module to JIT: "
			);

		auto addr_or_err = lljit.lookup(symbol_name);
		if (!addr_or_err) {
			llvm::handleAllErrors(addr_or_err.takeError(), [&](const llvm::ErrorInfoBase& eib) {
				llvm::errs() << "JIT lookup failed: " << eib.message() << '\n';
			});
			return nullptr;
		}

		llvm::orc::ExecutorAddr addr = *addr_or_err;

		auto compiled_fn = addr.toPtr<vm::jit::JitOpFun>();

		return compiled_fn;
	}
}
