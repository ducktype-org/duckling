#include "llvm_backend.hpp"

#include <llvm_helpers/llvm_helpers.hpp>
LLVM_INCLUDE_BEGIN()

// #include <llvm/ADT/APInt.h>
// #include <llvm/IR/Verifier.h>
// #include <llvm/ExecutionEngine/ExecutionEngine.h>
// #include <llvm/ExecutionEngine/GenericValue.h>
// #include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
// #include <llvm/Support/Casting.h>
#include <llvm/Support/TargetSelect.h>
// #include <llvm/Support/raw_ostream.h>

LLVM_INCLUDE_END()

#include <base/box.hpp>
#include <base/maps.hpp>

// usefull: https://github.com/llvm/llvm-project/tree/main/llvm/examples

namespace compiler::backend::llvm_backend {

	void llvmPrintLir(const lir::Function& lir_function) {
		// Create a new module.
		// std::unique_ptr<Module> TheModule = std::make_unique<Module>("

		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
		llvm::LLVMContext Context;

		Box<llvm::Module> module = box<llvm::Module>("test", Context);

		llvm::FunctionType* void_fun_type = llvm::FunctionType::get(
				llvm::Type::getVoidTy(Context), 
				{ },
				false
		);

		llvm::Function* fun = llvm::Function::Create(
			void_fun_type,
			llvm::Function::ExternalLinkage,
			lir_function.name.strView(),
			*module
		);

		base::Map<lir::BlockRef, u64> block_ids;
		{
			u64 next_id = 0;
			for (auto& block: lir_function.blocks) {
				block_ids.put(block.ref(), next_id++);
			}
		}

		for (auto& block: lir_function.blocks) {
			llvm::BasicBlock* llvm_block = llvm::BasicBlock::Create(
				Context, 
				base::strConcat("block_", block_ids[block.ref()]),
				fun
			);

			for (const auto& instruction: block->instructions) {
				// todo
			}
		}

		fun->print(llvm::errs());

	}

}