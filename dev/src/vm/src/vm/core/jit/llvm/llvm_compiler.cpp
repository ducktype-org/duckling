#include "../jit_compiler.hpp"
#include "jit_data.hpp"
#include "jit_utils.hpp"
#include "llvm_lowering.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/safe/low_program/cfg/cf_analysis.hpp>
#include <vm/core/safe/low_program/instruction.hpp>

LLVM_INCLUDE_BEGIN()
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
LLVM_INCLUDE_END()

#include <string>

namespace vm::jit {

	/**
	 * @brief Optimize Module with O3, inlining all calls to opfunctions.
	 */
	void optimizeModule(llvm::Module& m) {
		for (auto& fun: m) {
			if (fun.isDeclaration()) continue;

			fun.removeFnAttr(llvm::Attribute::NoInline);
			fun.addFnAttr(llvm::Attribute::AlwaysInline);

			for (auto& arg: fun.args()) {
				if (arg.getType()->isPointerTy()) {
					arg.addAttr(llvm::Attribute::NoAlias);
					arg.addAttr(llvm::Attribute::NoCapture);
					arg.addAttr(llvm::Attribute::NonNull);
				}
			}
		}


		llvm::PassBuilder             pb;
		llvm::LoopAnalysisManager     lam;
		llvm::FunctionAnalysisManager fam;
		llvm::CGSCCAnalysisManager    cgam;
		llvm::ModuleAnalysisManager   mam;

		// For maximum optimization:
		// Register all available module analyses passes.
		pb.registerModuleAnalyses(mam);
		// Registers all available CGSCC (Call Graph Strongly Connected Component) passes.
		pb.registerCGSCCAnalyses(cgam);
		// Register all available function analysis passes.
		pb.registerFunctionAnalyses(fam);
		// Register all available loop analysis passes.
		pb.registerLoopAnalyses(lam);
		// Connects all passes together, so they are not independent and can share analysis.
		pb.crossRegisterProxies(lam, fam, cgam, mam);
		// Register all O3 optimizations.
		auto mpm = pb.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3);
		mpm.run(m, mam);
	}

	static void printModule(Ref<llvm::Module> module, std::string_view filename) {
		std::error_code      error_code;
		llvm::raw_fd_ostream file(filename, error_code, llvm::sys::fs::OF_Text);

		if (error_code)
			llvm::errs() << "Error opening file: " << error_code.message() << "\n";
		else
			module->print(file, nullptr);
	}

	MRef<JitLLVMFunc> compileLLVM(
		const low::cf::ControlFlowGraph& cfg, const low::MicroBytecode& bc, const base::StrID& name
	) {
		auto&                         llvm_data = llvmData();
		llvm::orc::ThreadSafeContext& tsctx     = *llvm_data.g_context;

		// Generate a unique symbol name for the function to compile.
		// This is necessary to avoid duplicate definitions.
		static u64 compile_serial = 0;

		const std::string symbol_name = name.str() + "." + std::to_string(compile_serial++);
		std::unique_ptr<llvm::Module> new_module;

		llvm::LLVMContext& ctx = *tsctx.getContext();

		new_module = setupModule(symbol_name, ctx);

		LLVMBuilder(new_module.get(), ctx).lowerCFG(cfg, bc, name);

		// Deliberately inside CORE_ASSERT: verification runs only in DEV builds, so the JIT
		// hot path in release builds doesn't pay for it.
		CORE_ASSERT(
			!llvm::verifyModule(*new_module, &llvm::errs()), "Module invalid BEFORE optimization"
		);

		CORE_DEV_LOG(
			DVMDetails,
			(printModule(new_module.get(), "compiled_function-before.llvm"),
		     "Compiled function dumped")
		);

		optimizeModule(*new_module);

		CORE_ASSERT(
			!llvm::verifyModule(*new_module, &llvm::errs()), "Module invalid AFTER optimization"
		);

		CORE_DEV_LOG(
			DVMDetails,
			(printModule(new_module.get(), "compiled_function-after.llvm"),
		     "Compiled function dumped")
		);

		auto&                       lljit = *llvm_data.lljit_instance;
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

		auto compiled_fn = addr.toPtr<vm::jit::JitLLVMFunc>();

		return compiled_fn;
	}
}
