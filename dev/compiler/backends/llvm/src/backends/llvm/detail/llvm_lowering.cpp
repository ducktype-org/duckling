#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

LLVM_INCLUDE_END()

#include "../llvm_backend.hpp"
#include "module_impl.hpp"

#include <typesystem/lower/type_layout.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <init/init.hpp>
#include <base/box.hpp>
#include <base/ref.hpp>
#include <base/maps.hpp>
#include <base/int_conv.hpp>

// useful: https://github.com/llvm/llvm-project/tree/main/llvm/exampless

namespace compiler::backend_llvm {

	/**
	 * @brief Initializes some llvm components.
	 * @note it *should* be safe to call it multiple times
	 */
	void init() {
		bool v1 = llvm::InitializeNativeTarget();
		bool v2 = llvm::InitializeNativeTargetAsmPrinter();

		CORE_ASSERT(not v1, "failed to initialize llvm (1)");
		CORE_ASSERT(not v2, "failed to initialize llvm (2)");
	}

	void llvmDeinit() {
		// I'm not sure if this is a proper/stable
		// way to clean up llvm, but it works.
		// If it ever breaks, a quick-fix is just to comment it out
		// and let memory leak.
		// @note: There is also llvm_shutdown_obj helper object,
		// but we don't use it here in favor of deinit module.
		//
		// Note from LLVM docs:
		// IMPORTANT: it's only safe to call llvm_shutdown() in single thread, without any other
		// threads executing LLVM APIs. llvm_shutdown() should be the last use of LLVM APIs.
		llvm::llvm_shutdown();
	}

	RUN_BEFORE_MAIN(init::registerForDeinit(llvmDeinit));

	/**
	 * @brief Returns reference to the llvm context.
	 * @note as per https://llvm.org/doxygen/classllvm_1_1LLVMContext.html#details
	 * single context can't be used my multiple threads.
	 * @note as of 9.11.2024 I did not find the reason, to have more than one context per thread,
	 * hence this function.
	 *
	 * @return llvm::LLVMContext&
	 */
	llvm::LLVMContext& getLLVMContext() {
		static llvm::LLVMContext context;
		return context;
	}

	auto voidType(llvm::LLVMContext& context) { return llvm::Type::getVoidTy(context); }

	auto i64Type(llvm::LLVMContext& context) { return llvm::Type::getInt64Ty(context); }

	auto i32Type(llvm::LLVMContext& context) { return llvm::Type::getInt32Ty(context); }

	/**
	 * @note bools in LLVM are just i1 (i8 when stored in memory)
	 */
	auto i1Type(llvm::LLVMContext& context) { return llvm::Type::getInt1Ty(context); }

	auto typeFromLayout(llvm::LLVMContext& context, const tsl::TypeLayout& layout) -> llvm::Type* {
		variant_match(layout()) {
			variant_case_novalue(tsl::EmptyTypeLayout) { return llvm::Type::getVoidTy(context); }
			variant_case_novalue(tsl::IntegralTypeLayout) {
				return llvm::Type::getIntNTy(
					context, base::safeIntConv<unsigned>(static_cast<usize>(layout.getSize()))
				);
			}
			variant_case_novalue(tsl::FloatTypeLayout) {
				// see https://llvm.org/docs/LangRef.html#floating-point-types for docs on LLVM
				// floating point types
				switch (static_cast<usize>(layout.getSize())) {
				case 32:
					return llvm::Type::getFloatTy(context);
				case 64:
					return llvm::Type::getDoubleTy(context);
				default:
					CORE_PANIC("Float size different than 32 or 64 not implemented yet.");
				}
			}
			variant_default {
				CORE_PANIC(
					base::strConcat("Type not handled yet: ", layout.toStringIdentification())
				);
			}
		}
		CORE_UNREACHABLE();
	}

	auto getFunType(llvm::LLVMContext& context, const tsl::TypeLayout& return_type) {
		// @TODO: #486 - process function argument types
		return llvm::FunctionType::get(typeFromLayout(context, return_type), {}, false);
	}

	/**
	 * @brief This struct should be treated as a function,
	 * that takes LLVMContext, LIRFunction and LLVM Module,
	 * and generates LLVM function in given module based
	 * on provided LIRFunction.
	 */
	struct LIR2LLVMFunction {
		llvm::LLVMContext&  context;
		CRef<lir::Function> lir_function;
		Ref<llvm::Module>   module;

		LIR2LLVMFunction(
			llvm::LLVMContext& context, CRef<lir::Function> lir_function, Ref<llvm::Module> module
		):
			  context(context),
			  lir_function(lir_function),
			  module(module) {}

	private:
		/**
		 * Maps LIR blocks to LLVM blocks.
		 * @note: Not all LLVM blocks will be here
		 */
		base::Map<lir::BlockRef, Ref<llvm::BasicBlock>> block_mapping;

		void generateMainBlocks(llvm::Function* fun) {
			auto block_ids = lir_function->getBlockIDs();
			// llvm prints in reverse... eh:
			for (auto block: lir_function->block_order) {
				llvm::BasicBlock* llvm_block = llvm::BasicBlock::Create(
					context, base::strConcat("lir_block_", block_ids[block]), fun
				);
				block_mapping.put(block, llvm_block);
			}
		}

		base::Map<lir::LocalRef, u64> lir_local_ids;

		std::string llvmLocalName(lir::LocalRef lir_local) {
			// @TODO.. this might have to change in the future
			if (lir_local->helios_id)
				return base::strConcat("helios_", lir_local_ids[lir_local]);
			else
				return base::strConcat("tmp_", lir_local_ids[lir_local]);
		}

		/**
		 * @brief Maps lir locals to LLVM registers storing
		 * pointers to them.
		 */
		base::Map<lir::LocalRef, Ref<llvm::Instruction>> local_register_map;

		void generateMainBlocksAndLocals(llvm::Function* fun) {
			// allocate all local variables:

			// set local id map:
			lir_local_ids = lir_function->getLocalVariableIDs();

			// first block:
			llvm::BasicBlock* locals_block
				= llvm::BasicBlock::Create(context, "local_variables", fun);
			llvm::IRBuilder<> locals_builder(locals_block);
			for (auto& var: lir_function->local_list) {
				// @TODO: add llvm types:
				auto reg = locals_builder.CreateAlloca(
					typeFromLayout(context, var->layout), nullptr, llvmLocalName(var.ref())
				);
				local_register_map.put(var.ref(), reg);
			}

			generateMainBlocks(fun);

			// here we assume that first block in block order is the entry block
			// it might be wrong, but it's good enough for now
			locals_builder.CreateBr(block_mapping[lir_function->block_order.at(0)].get());
		}

		/**
		 * @brief Maps LIRLocation to LLVM Value.
		 *
		 * This function may generate new LLVM instructions if necessary. For example,
		 * when loading the value of a local variable (which we store behind a pointer
		 * to the stack), we need to generate a load instruction.
		 * Moreover, this load instruction has to be generated with each use, because
		 * the value of the variable may change between uses.
		 *
		 * @note In llvm a lot of things can be treated as values, and
		 * it's based on inheritance.
		 * @param lir_location The LirLocation to convert into an LLVM Value.
		 * @param builder The LLVM IRBuilder to use for loading the value, if necessary.
		 * @return llvm::Value*
		 */
		auto lirLocation2LLVM(const lir::LirLocation& lir_location, llvm::IRBuilder<>& builder)
			-> llvm::Value* {
			variant_match(lir_location.getVariant()) {
				variant_case(i64, value) {
					return llvm::ConstantInt::getSigned(i64Type(context), value);
				}
				variant_case(bool, value) { return llvm::ConstantInt::get(i1Type(context), value); }
				variant_case(lir::LocalRef, lir_local) {
					// We store local values behind pointers to stack-allocated memory.
					// We need to load them before using them.
					const auto local_ptr = local_register_map[lir_local].get();
					return builder.CreateLoad(
						typeFromLayout(getLLVMContext(), lir_local->layout), local_ptr
					);
				}
				variant_case(lir::BlockRef, lir_block) { return block_mapping[lir_block].get(); }
				variant_default { CORE_PANIC("unknown lir location type"); }
			}
			CORE_UNREACHABLE();
		}

		auto lirLocationList2LLVM(
			const std::vector<lir::LirLocation>& lir_locations, llvm::IRBuilder<>& builder
		) -> std::vector<llvm::Value*> {
			std::vector<llvm::Value*> llvm_locations;
			llvm_locations.reserve(lir_locations.size());
			for (const auto& lir_location: lir_locations)
				llvm_locations.push_back(lirLocation2LLVM(lir_location, builder));
			return llvm_locations;
		}

#define LIR_2_LLVM_BINARY_OPERATION_CASE(op)                                            \
	{                                                                                   \
		const auto output = lir_instruction.output.value();                             \
		const auto lhs    = lirLocation2LLVM(lir_instruction.arguments.at(0), builder); \
		const auto rhs    = lirLocation2LLVM(lir_instruction.arguments.at(1), builder); \
		const auto value  = builder.Create##op(lhs, rhs);                               \
		builder.CreateStore(value, local_register_map[output].get());                   \
		break;                                                                          \
	}

		/**
		 * @brief Lowers LIRInstruction to LLVM instructions and appends them
		 * to the end of the block given by @p builder.
		 */
		void lir2LLVMInstruction(
			const lir::Instruction& lir_instruction, llvm::IRBuilder<>& builder
		) {
			using enum lir::Operation;
			switch (lir_instruction.operation) {
			case ReturnVoid: {
				builder.CreateRetVoid();
				break;
			}
			case ReturnValue: {
				builder.CreateRet(lirLocation2LLVM(lir_instruction.arguments.at(0), builder));
				break;
			}
			case Jump: {
				builder.CreateBr(
					block_mapping[lir_instruction.arguments.at(0).get<lir::BlockRef>()].get()
				);
				break;
			}
			case Branch: {
				// here for lir locals we need more stuff:
				const auto cond = lirLocation2LLVM(lir_instruction.arguments.at(0), builder);
				const auto true_block
					= block_mapping[lir_instruction.arguments.at(1).get<lir::BlockRef>()];
				const auto false_block
					= block_mapping[lir_instruction.arguments.at(2).get<lir::BlockRef>()];
				builder.CreateCondBr(cond, true_block.get(), false_block.get());
				break;
			}
			case Assign: {
				const auto output = lir_instruction.output.value();
				const auto value  = lirLocation2LLVM(lir_instruction.arguments.at(0), builder);
				builder.CreateStore(value, local_register_map[output].get());
				break;
			}
			case IntegerAdd:
				LIR_2_LLVM_BINARY_OPERATION_CASE(Add)
			case IntegerSub:
				LIR_2_LLVM_BINARY_OPERATION_CASE(Sub)
			case IntegerMul:
				LIR_2_LLVM_BINARY_OPERATION_CASE(Mul)
			case IntegerUDiv:
				LIR_2_LLVM_BINARY_OPERATION_CASE(UDiv)
			case IntegerSDiv:
				LIR_2_LLVM_BINARY_OPERATION_CASE(SDiv)
			case IntegerUMod:
				LIR_2_LLVM_BINARY_OPERATION_CASE(URem)
			case IntegerSMod:
				LIR_2_LLVM_BINARY_OPERATION_CASE(SRem)
			case IntegerULt:
				LIR_2_LLVM_BINARY_OPERATION_CASE(ICmpULT)
			case IntegerSLt:
				LIR_2_LLVM_BINARY_OPERATION_CASE(ICmpSLT)
			case IntegerNeg: {
				const auto output   = lir_instruction.output.value();
				const auto argument = lirLocation2LLVM(lir_instruction.arguments.at(0), builder);
				const auto value    = builder.CreateNeg(argument);
				builder.CreateStore(value, local_register_map[output].get());
				break;
			}
			default:
				std::cerr << "unknown lir operation (skip): "
						  << base::enumToStr(lir_instruction.operation).strView() << "\n";
				// throw base::NotYetImplemented("some lir operation in llvm backend");
			}
		}

	public:
		/**
		 * @brief lowers LIRFunction to LLVM Function and adds
		 * it to the llvm module.
		 *
		 * @return llvm::Function*
		 */
		llvm::Function* createFunction() {
			// this also adds the function to the module:
			llvm::Function* fun = llvm::Function::Create(
				getFunType(context, lir_function->return_type_layout),
				llvm::Function::ExternalLinkage,
				lir_function->name.strView(),
				*module
			);

			generateMainBlocksAndLocals(fun);

			for (auto& block: lir_function->block_order) {
				auto              llvm_block = block_mapping[block];
				llvm::IRBuilder<> builder(llvm_block.get());
				for (const auto& instruction: block->instructions)
					lir2LLVMInstruction(instruction, builder);
				lir2LLVMInstruction(block->terminator, builder);
			}

			EliminateUnreachableBlocks(*fun);

			return fun;
		}
	};

	Box<ModuleImpl> initModuleImpl(base::StrID module_id) {
		init();
		llvm::LLVMContext& context = getLLVMContext();

		Box<llvm::Module> llvm_module = makeBox<llvm::Module>(module_id.str(), context);
		return makeBox<ModuleImpl>(std::move(llvm_module));
	}

	void addFunctionToModuleImpl(Ref<ModuleImpl> module, CRef<lir::Function> lir_function) {
		LIR2LLVMFunction lir2llvm{ getLLVMContext(), lir_function, module->module.refMut() };
		lir2llvm.createFunction();
	}

	Module lirFunctionToModule(CRef<lir::Function> lir_function) {
		auto module_impl = initModuleImpl(base::StrID("test"));
		addFunctionToModuleImpl(module_impl.refMut(), lir_function);
		return Module{ std::move(module_impl) };
	}
}
