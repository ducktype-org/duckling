#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()

// #include <llvm/ADT/APInt.h>
#include <llvm/IR/Verifier.h>
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

// #include <llvm/Support/TargetSelect.h>
// #include <llvm/Support/Host.h>

// #include <llvm/Target/TargetMachine.h>
// #include <llvm/Target/TargetOptions.h>
// #include <llvm/ADT/Optional.h>

LLVM_INCLUDE_END()


#include "llvm_backend.hpp"
#include <lir/lir_structure/lir_structure.hpp>
#include <base/box.hpp>
#include <base/maps.hpp>

// usefull: https://github.com/llvm/llvm-project/tree/main/llvm/examples


namespace compiler::backend_llvm {

	auto voidType(llvm::LLVMContext& context) {
		return llvm::Type::getVoidTy(context);
	};

	auto i64Type(llvm::LLVMContext& context) {
		return llvm::Type::getInt64Ty(context);
	};

	auto i32Type(llvm::LLVMContext& context) {
		return llvm::Type::getInt32Ty(context);
	};

	auto voidFunType(llvm::LLVMContext& context) {
		return llvm::FunctionType::get(
			voidType(context),
			{ },
			false
		);
	};

	auto intFunType(llvm::LLVMContext& context) {
		return llvm::FunctionType::get(
			i32Type(context),
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
			// llvm prints in reverse... eh:
			for (auto& block: lir_function.blocks | std::views::reverse) {
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

		base::Map<lir::LocalRef, Ref<llvm::Instruction>> register_map;
		void generateLocalVars(llvm::Function* fun) {
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
				auto reg = locals_builder.CreateAlloca(
					i64Type(context), 
					nullptr,
					llvmLocalName(var.ref())
				);
				register_map.put(var.ref(), reg);
			}

			// @TODO: change function names lol:
			generateBlockMapping(fun);

			// here we assume that first block in block order is the entry block
			// it might be wrong, but it's good enough for now
			locals_builder.CreateBr(block_mapping[lir_function.block_order.at(0)].get());
		}

		auto lir2LLVMLocation(const lir::LirLocation& lir_location) -> llvm::Value* {
			variant_match(lir_location.getVariant()) {
				variant_case(i64, value) {
					return llvm::ConstantInt::get(i64Type(context), value);
				}
				variant_case(lir::LocalRef, lir_local) {
					return register_map[lir_local].get();
				}
				variant_case(lir::BlockRef, lir_block) {
					return block_mapping[lir_block].get();
				}
				variant_default {
					CORE_PANIC("unknown lir location type");
				}
			}
			CORE_PANIC("unreachable");
		}

		auto lir2LLVMLocationList(const std::vector<lir::LirLocation>& lir_locations) -> std::vector<llvm::Value*> {
			std::vector<llvm::Value*> llvm_locations;
			llvm_locations.reserve(lir_locations.size());
			for (const auto& lir_location: lir_locations) {
				llvm_locations.push_back(lir2LLVMLocation(lir_location));
			}
			return llvm_locations;
		}

		void lir2LLVMInstuction(const lir::Instruction& lir_instruction, llvm::IRBuilder<>& builder) {
			switch (lir_instruction.operation) {
			case lir::LirOperation::ReturnVoid: {
				builder.CreateRetVoid();
				break;
			}
			case lir::LirOperation::ReturnValue: {
				builder.CreateRet(lir2LLVMLocation(lir_instruction.arguments.at(0)));
				break;
			}
			case lir::LirOperation::Jump: {
				builder.CreateBr(block_mapping[lir_instruction.arguments.at(0).get<lir::BlockRef>()].get());
				break;
			}
			case lir::LirOperation::Branch: {
				// here for lir locals we need more stuff:
				auto cond = lir2LLVMLocation(lir_instruction.arguments.at(0));
				auto true_block = block_mapping[lir_instruction.arguments.at(1).get<lir::BlockRef>()];
				auto false_block = block_mapping[lir_instruction.arguments.at(2).get<lir::BlockRef>()];
				builder.CreateCondBr(cond, true_block.get(), false_block.get());
				break;
			}
			default:
				std::cerr << "unknown lir operation (skip): " 
					<< base::enumToStr(lir_instruction.operation).strView()
					<< "\n";
				// throw base::NotYetImplemented("some lir operation in llvm backend");
			}
		}

		llvm::Function* createFunction() {
			llvm::Function* fun = llvm::Function::Create(
				voidFunType(context),
				llvm::Function::ExternalLinkage,
				lir_function.name.strView(),
				*module
			);

			generateLocalVars(fun);
			// generateBlockMapping(fun);


			for (auto& block: lir_function.block_order) {
				auto llvm_block = block_mapping[block];
				llvm::IRBuilder<> builder(llvm_block.get());
				for (const auto& instruction: block->instructions) {
					lir2LLVMInstuction(instruction, builder);
				}
				lir2LLVMInstuction(block->terminator, builder);
			}

			return fun;
		}

	};


	void llvmPrintLir(const lir::Function& lir_function) {


		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
		llvm::LLVMContext Context;
		Box<llvm::Module> module = box<llvm::Module>("test", Context);

		LIR2LLVMFunction lir2llvm{Context, lir_function, module.refMut()};

		auto fun = lir2llvm.createFunction();
		

		std::cerr << "\n\nVerification: \n";
		bool error_found = llvm::verifyFunction(*fun, &llvm::errs());
		std::cerr << "\n\n\n";

		// making obj files from api is for some reason not trivial,
		// lacking docs for new api

		if (error_found) {
			std::cerr << "Errors, aborting!\n";
		}
		else {
			std::cerr << "OK\n";
			// so we will do this:...
			fun->print(llvm::outs());
		}

	}

}