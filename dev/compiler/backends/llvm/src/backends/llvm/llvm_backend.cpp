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
#include <llvm/IR/IRBuilder.h>
// #include <llvm/Support/Casting.h>
#include <llvm/Support/TargetSelect.h>
// #include <llvm/Support/raw_ostream.h>

LLVM_INCLUDE_END()

#include <base/box.hpp>
#include <base/maps.hpp>

// usefull: https://github.com/llvm/llvm-project/tree/main/llvm/examples


namespace compiler::backend::llvm_backend {

	auto voidType(llvm::LLVMContext& context) {
		return llvm::Type::getVoidTy(context);
	};

	auto i64Type(llvm::LLVMContext& context) {
		return llvm::Type::getInt64Ty(context);
	};

	auto voidFunType(llvm::LLVMContext& context) {
		return llvm::FunctionType::get(
			voidType(context),
			{ },
			false
		);
	};

	struct LIR2LLVMFunction {
		// @todo: this should probably be static per thread or something like thats:
		llvm::LLVMContext& context;
		const lir::Function& lir_function;
		Ref<llvm::Module> module;

		LIR2LLVMFunction(
			llvm::LLVMContext& context, 
			const lir::Function& lir_function,
			Ref<llvm::Module> module
		):
			context(context),
			lir_function(lir_function),
			module(module)
			 {}

		base::Map<lir::BlockRef, Ref<llvm::BasicBlock>> block_mapping;

		void generateBlockMapping(llvm::Function* fun) {
			u64 block_id = 0;
			for (auto& block: lir_function.blocks) {
				llvm::BasicBlock* llvm_block = llvm::BasicBlock::Create(
					context, 
					base::strConcat("block_", block_id++),
					fun
				);
				block_mapping.put(block.ref(), llvm_block);
			}
		}
		
		base::Map<lir::LocalRef, u64> tmp_id;
		std::string llvmLocalName(lir::LocalRef lir_local) {
			// @TODO.. far from optimal
			if (lir_local->helios_id) {
				return base::strConcat("helios_", base::perfectHash(lir_local->helios_id.value()));
			} else {
				if (not tmp_id.contains(lir_local)) {
					tmp_id.put(lir_local, tmp_id.size());
				}
				return base::strConcat("tmp_", tmp_id[lir_local]);
			}
		}

		// auto lir2LLVMLocation(const lir::LirLocation& lir_location) {
		// 	variant_match(lir_location.getVariant()) {
		// 		variant_case(i64, value) {
		// 			return llvm::ConstantInt::get(i64Type(context), value);
		// 		}

		// 	}
		// }

		// void lir2LLVMInstuction(const lir::Instruction& lir_instruction, Ref<llvm::BasicBlock> llvm_block) {
		// 	switch (lir_instruction.operation) {
		// 	case lir::LirOperation::Assign: {
		// 		auto& output = lir_instruction.output.value();
		// 		auto& argument = lir_instruction.arguments.at(0);
		// 		auto llvm_output = llvm::cast<llvm::AllocaInst>(argument.getVariant().get<llvm::Value*>());
		// 		auto llvm_argument = argument.getVariant().get<llvm::Value*>();
		// 		auto store = new llvm::StoreInst(llvm_argument, llvm_output, llvm_block);
		// 		break;
		// 	}
		// 	default:
		// 		throw base::NotYetImplemented("some lir operation in llvm backend");
		// 	}
		// }

		llvm::Function* createFunction() {
			llvm::Function* fun = llvm::Function::Create(
				voidFunType(context),
				llvm::Function::ExternalLinkage,
				lir_function.name.strView(),
				*module
			);

			generateBlockMapping(fun);

			// allocate all local variables:
			// first block:
			llvm::BasicBlock* locals_block = llvm::BasicBlock::Create(
				context, 
				"local_variables",
				fun
			);
			llvm::IRBuilder<> locals_builder(locals_block);
			for (auto& var: lir_function.local_list) {
				// @TODO: llvm types:
				locals_builder.CreateAlloca(
					i64Type(context), 
					nullptr,
					llvmLocalName(var.ref())
				);
			}
			// here we assume that first block in block order is the entry block
			// it might be wrong, but it's good enough for now
			locals_builder.CreateBr(block_mapping[lir_function.block_order.at(0)].get());


			// llvm prints blocks in reverse, eh:
			for (auto& block: lir_function.block_order | std::views::reverse) {
				auto llvm_block = block_mapping[block];
				llvm::IRBuilder<> builder(llvm_block.get());
				for (const auto& instruction: block->instructions) {
					// lir2LLVMInstuction(instruction, llvm_block);
				}
				// lir2LLVMInstuction(block->terminator, llvm_block);
			}

			return fun;
		}

	};



	void llvmPrintLir(const lir::Function& lir_function) {
		// Create a new module.
		// std::unique_ptr<Module> TheModule = std::make_unique<Module>("

		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
		llvm::LLVMContext Context;
		Box<llvm::Module> module = box<llvm::Module>("test", Context);

		LIR2LLVMFunction lir2llvm{Context, lir_function, module.refMut()};

		auto fun = lir2llvm.createFunction();
		fun->print(llvm::errs());
	}

}