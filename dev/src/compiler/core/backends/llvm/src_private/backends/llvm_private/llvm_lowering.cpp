
#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

LLVM_INCLUDE_END()

#include "module_impl.hpp"

#include <ctv/numeric_value.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <typesystem/lower/type_layout.hpp>

#include <base/collections/maps.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/int_conv.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <init/init.hpp>

#include <iostream>

// useful: https://github.com/llvm/llvm-project/tree/main/llvm/examples
namespace {
	/**
	 * @brief Converts a CTV into its corresponding llvm::Constant representation.
	 * @param ctv The CTV to convert.
	 * @param llvm_type The expected type.
	 * @return The created llvm::Constant*.
	 */
	auto ctvToLLVMConstant(const compiler::ctv::CompileTimeValue& ctv, llvm::Type* llvm_type) {
		variant_match(ctv.getStorage()) {
			variant_case(compiler::numeric_value::NumericValue, numeric) {
				if (!llvm_type->isIntegerTy()) {
					CORE_PANIC("LLVM lowering : Type mismatch. CTV is numeric, but LLVM type is not"
					);
				}

				// TODO: #1499. For now every numeric value is casted to i64 (including floating
				// point literals).
				i64 coerced_value = numeric.coerceTo<i64>().value();
				return llvm::ConstantInt::getSigned(llvm_type, coerced_value);
			}
			variant_case(bool, val) {
				if (!llvm_type->isIntegerTy(1)) {
					CORE_PANIC(
						"LLVM lowering : Type mismatch. CTV is a boolean, but LLVM type is not"
					);
				}
				return llvm::ConstantInt::get(llvm_type, val ? 1 : 0, false);
			}
			variant_default {
				throw base::NotYetImplemented("Conversion from CTV to LLVM constant for this type.");
			}
		}
		CORE_UNREACHABLE();
	}
}

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
		thread_local llvm::LLVMContext context;
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
				CORE_PANIC(base::strConcat("Type not handled yet: ", layout.toStringIdentification())
				);
			}
		}
		CORE_UNREACHABLE();
	}

	auto getFunType(
		llvm::LLVMContext&                  context,
		const std::vector<tsl::TypeLayout>& parameters,
		const tsl::TypeLayout&              return_type
	) {
		std::vector<llvm::Type*> llvm_parameters;
		llvm_parameters.reserve(parameters.size());
		for (const auto& param: parameters)
			llvm_parameters.push_back(typeFromLayout(context, param));

		return llvm::FunctionType::get(typeFromLayout(context, return_type), llvm_parameters, false);
	}

	llvm::CallingConv::ID getCallingConvFromABI(const helios::SymbolABI& abi) {
		variant_match(abi) {
			variant_case(helios::DefaultAbi, name) { return llvm::CallingConv::C; }
			variant_case(helios::CAbi, name) { return llvm::CallingConv::C; }
		}
		CORE_UNREACHABLE();
	}

	/**
	 * Gets a function from a module by the function literal (using a mangle_name field).
	 *
	 * If the function doesn't exits it adds a function prototype with
	 * external linkage to the module based on provided lir_functions.
	 *
	 * @note We use it to add all functions to the module currently.
	 * This will have to change in the future, but it will require some restructuring
	 * of how we are creating llvm modules, as we need to know what function in local to which
	 * module.
	 */
	llvm::FunctionCallee getOrInsertFunctionPrototypeFromLiteral(
		Ref<llvm::Module> module, const lir::FunctionLiteral& function_literal
	) {
		auto mangled_name = function_literal.mangled_name;
		// We check if function exist first, to avoid unnecessary construction of types:
		if (auto func = module->getFunction(mangled_name.strView())) return func;

		auto&                context = module->getContext();
		llvm::FunctionCallee callee  = module->getOrInsertFunction(
            mangled_name.strView(),
            getFunType(
                context, *function_literal.parameter_layouts, *function_literal.return_type_layout
            )
        );

		if (auto* function = llvm::dyn_cast<llvm::Function>(callee.getCallee()))
			function->setCallingConv(getCallingConvFromABI(function_literal.abi));

		return callee;
	}

	llvm::FunctionCallee getOrInsertFunctionPrototypeFromLirFunction(
		Ref<llvm::Module> module, const lir::Function& lir_function
	) {
		return getOrInsertFunctionPrototypeFromLiteral(
			module, lir::FunctionLiteral::fromFunction(lir_function)
		);
	}

	Ref<llvm::Constant> getOrInsertGlobalVariable(
		Ref<llvm::Module> module, const lir::LirGlobal& lir_global
	) {
		auto mangled_name = lir_global.mangled_name.strView();

		if (auto global = module->getGlobalVariable(mangled_name)) return global;

		auto& context = module->getContext();

		auto global_type = typeFromLayout(context, *lir_global.layout);

		return module->getOrInsertGlobal(mangled_name, global_type);
	}

	Ref<llvm::GlobalVariable> addGlobalVariable(
		Ref<llvm::Module> module, const lir::LirGlobal& lir_global
	) {
		getOrInsertGlobalVariable(module, lir_global);
		Ref<llvm::GlobalVariable> global
			= module->getNamedGlobal(lir_global.mangled_name.strView());

		CORE_ASSERT(global->isDeclaration(), "Global is not the declaration");

		global->setLinkage(llvm::GlobalValue::ExternalLinkage);
		global->setConstant(lir_global.type == lir::LirGlobalType::Constant);
		// Initialise the global variable to null, sice it will be initialised in the constructor
		if (lir_global.initial_value.has_value()) {
			global->setInitializer(
				ctvToLLVMConstant(lir_global.initial_value.value(), global->getValueType())
			);
		} else {
			global->setInitializer(llvm::Constant::getNullValue(global->getValueType()));
		}

		return global;
	}

	/**
	 * @brief This struct should be treated as a function,
	 * that takes LLVMContext, LIRFunction and LLVM Module,
	 * and generates LLVM function in given module based
	 * on provided LIRFunction.
	 */
	struct LirFunction2LLVM {
		llvm::LLVMContext&  context;
		query::Context&     ctx;
		CRef<lir::Function> lir_function;
		Ref<llvm::Module>   module;

		LirFunction2LLVM(
			llvm::LLVMContext&  context,
			query::Context&     ctx,
			CRef<lir::Function> lir_function,
			Ref<llvm::Module>   module
		):
			  context(context),
			  ctx(ctx),
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

		/**
		 * Fills local_register_map and block_mapping.
		 * @note It creates a IR Block that initialized all of local-values registers,
		 * and fill the ones representing function parameters with appropriate values.
		 */
		void generateMainBlocksAndLocals(llvm::Function* fun) {
			// allocate all local variables:

			// set local id map:
			lir_local_ids = lir_function->getLocalVariableIDs();

			// first block:
			llvm::BasicBlock* locals_block
				= llvm::BasicBlock::Create(context, "local_variables", fun);
			llvm::IRBuilder<> locals_builder(locals_block);
			for (auto& var: lir_function->local_list) {
				CORE_ASSERT(
					var.layout.getSize() > Bits(0),
					"local variable with size 0 is not allowed in LLVM"
				);
				auto reg = locals_builder.CreateAlloca(
					typeFromLayout(context, var.layout), nullptr, llvmLocalName(&var)
				);

				// If local is a parameter we initialize it from
				// llvm parameter:
				if_opt_some(var.parameter_index, parameter_index) {
					locals_builder.CreateStore(
						fun->getArg(base::safeIntConv<unsigned>(parameter_index)), reg
					);
				}
				local_register_map.put(&var, reg);
			}

			generateMainBlocks(fun);

			// here we assume that first block in block order is the entry block
			// it might be wrong, but it's good enough for now
			locals_builder.CreateBr(block_mapping[lir_function->block_order.at(0)].get());
		}

		/**
		 * @brief Maps LIRValue to LLVM Value.
		 *
		 * This function may generate new LLVM instructions if necessary. For example,
		 * when loading the value of a local variable (which we store behind a pointer
		 * to the stack), we need to generate a load instruction.
		 * Moreover, this load instruction has to be generated with each use, because
		 * the value of the variable may change between uses.
		 *
		 * @note In llvm a lot of things can be treated as values, and
		 * it's based on inheritance.
		 * @param lir_location The LIRValue to convert into an LLVM Value.
		 * @param builder The LLVM IRBuilder to use for loading the value, if necessary.
		 * @return llvm::Value*
		 */
		auto lirValue2LLVM(const lir::LIRValue& lir_location, llvm::IRBuilder<>& builder)
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
						typeFromLayout(builder.getContext(), lir_local->layout), local_ptr
					);
				}
				variant_case(lir::BlockRef, lir_block) { return block_mapping[lir_block].get(); }
				variant_case(lir::LirGlobal, lir_global) {
					auto global_ptr = getOrInsertGlobalVariable(module, lir_global);
					return builder.CreateLoad(
						typeFromLayout(builder.getContext(), *lir_global.layout), global_ptr.get()
					);
				}
				variant_default { CORE_PANIC("unknown lir location type"); }
			}
			CORE_UNREACHABLE();
		}

		auto lirValueList2LLVM(
			const std::vector<lir::LIRValue>& lir_locations, llvm::IRBuilder<>& builder
		) -> std::vector<llvm::Value*> {
			std::vector<llvm::Value*> llvm_locations;
			llvm_locations.reserve(lir_locations.size());
			for (const auto& lir_location: lir_locations)
				llvm_locations.push_back(lirValue2LLVM(lir_location, builder));
			return llvm_locations;
		}

		void storeOutput(
			std::variant<lir::LocalRef, lir::LirGlobal> output,
			Ref<llvm::Value>                            value,
			llvm::IRBuilder<>&                          builder
		) {
			variant_match(output) {
				variant_case(lir::LocalRef, lir_local) {
					builder.CreateStore(value.get(), local_register_map[lir_local].get());
				}
				variant_case(lir::LirGlobal, global_lir) {
					auto global = getOrInsertGlobalVariable(module, global_lir);
					builder.CreateStore(value.get(), global.get());
				}
				variant_default { CORE_PANIC("unknown lir output type"); }
			}
		}

#define LIR_2_LLVM_BINARY_OPERATION_CASE(op)                                        \
	{                                                                               \
		const auto lhs   = lirValue2LLVM(lir_instruction.arguments.at(0), builder); \
		const auto rhs   = lirValue2LLVM(lir_instruction.arguments.at(1), builder); \
		const auto value = builder.Create##op(lhs, rhs);                            \
		storeOutput(lir_instruction.output.value(), value, builder);                \
		break;                                                                      \
	}

		/**
		 * @brief Lowers LIRInstruction to LLVM instructions and appends them
		 * to the end of the block given by @p builder.
		 */
		void lirInstruction2LLVM(
			const lir::Instruction& lir_instruction, llvm::IRBuilder<>& builder
		) {
			using enum lir::Operation;
			switch (lir_instruction.operation) {
			case ReturnVoid: {
				builder.CreateRetVoid();
				break;
			}
			case ReturnValue: {
				builder.CreateRet(lirValue2LLVM(lir_instruction.arguments.at(0), builder));
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
				const auto cond = lirValue2LLVM(lir_instruction.arguments.at(0), builder);
				const auto true_block
					= block_mapping[lir_instruction.arguments.at(1).get<lir::BlockRef>()];
				const auto false_block
					= block_mapping[lir_instruction.arguments.at(2).get<lir::BlockRef>()];
				builder.CreateCondBr(cond, true_block.get(), false_block.get());
				break;
			}
			case Assign: {
				const auto value = lirValue2LLVM(lir_instruction.arguments.at(0), builder);
				storeOutput(lir_instruction.output.value(), value, builder);
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
			case IntegerULteq:
				LIR_2_LLVM_BINARY_OPERATION_CASE(ICmpULE)
			case IntegerSLteq:
				LIR_2_LLVM_BINARY_OPERATION_CASE(ICmpSLE)
			case IntegerUGt:
				LIR_2_LLVM_BINARY_OPERATION_CASE(ICmpUGT)
			case IntegerSGt:
				LIR_2_LLVM_BINARY_OPERATION_CASE(ICmpSGT)
			case IntegerUGteq:
				LIR_2_LLVM_BINARY_OPERATION_CASE(ICmpUGE)
			case IntegerSGteq:
				LIR_2_LLVM_BINARY_OPERATION_CASE(ICmpSGE)
			case IntegerEq:
				LIR_2_LLVM_BINARY_OPERATION_CASE(ICmpEQ)
			case IntegerNeq:
				LIR_2_LLVM_BINARY_OPERATION_CASE(ICmpNE)
			case IntegerNeg: {
				const auto output   = lir_instruction.output.value();
				const auto argument = lirValue2LLVM(lir_instruction.arguments.at(0), builder);
				const auto value    = builder.CreateNeg(argument);
				storeOutput(output, value, builder);
				break;
			}
			case BooleanAnd:
				LIR_2_LLVM_BINARY_OPERATION_CASE(LogicalAnd)
			case BooleanOr:
				LIR_2_LLVM_BINARY_OPERATION_CASE(LogicalOr)
			case BooleanNot: {
				const auto argument = lirValue2LLVM(lir_instruction.arguments.at(0), builder);
				const auto value    = builder.CreateNot(argument);
				storeOutput(lir_instruction.output.value(), value, builder);
				break;
			}
			case Call: {
				CORE_ASSERT(lir_instruction.arguments.size() > 0, "call instruction without callee");

				auto callee = getOrInsertFunctionPrototypeFromLiteral(
					module, lir_instruction.arguments.at(0).get<lir::FunctionLiteral>()
				);

				const auto args = lirValueList2LLVM(
					std::vector(
						lir_instruction.arguments.begin() + 1, lir_instruction.arguments.end()
					),
					builder
				);

				if (lir_instruction.output.has_value()) {
					const auto output = lir_instruction.output.value();
					const auto value  = builder.CreateCall(callee, args);
					storeOutput(output, value, builder);
				} else {
					CORE_ASSERT(
						callee.getFunctionType()->getReturnType()->isVoidTy(),
						"call to non void function without output – this may be valid, feel free "
						"to remove assertion if the compiler internals change."
					);
					builder.CreateCall(callee, args);
				}

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
			Ref fun = llvm::cast<llvm::Function>(
				getOrInsertFunctionPrototypeFromLirFunction(module, *lir_function).getCallee()
			);
			CORE_ASSERT(fun->isDeclaration(), "function is not a declaration");

			generateMainBlocksAndLocals(fun.get());

			for (auto& block: lir_function->block_order) {
				auto              llvm_block = block_mapping[block];
				llvm::IRBuilder<> builder(llvm_block.get());
				for (const auto& instruction: block->instructions)
					lirInstruction2LLVM(instruction, builder);
				lirInstruction2LLVM(block->terminator, builder);
			}

			return fun.get();
		}
	};

	Box<ModuleImpl> initModuleImpl(base::StrID module_id) {
		init();
		llvm::LLVMContext& context = getLLVMContext();

		Box<llvm::Module> llvm_module = makeBox<llvm::Module>(module_id.str(), context);
		return makeBox<ModuleImpl>(std::move(llvm_module));
	}

	Box<ModuleImpl> parseIRCodeToModuleImpl(std::string_view llvm_ir_code) {
		auto memory_buffer = llvm::MemoryBuffer::getMemBuffer(llvm::StringRef(llvm_ir_code));
		if (!memory_buffer) CORE_PANIC("failed to create memory buffer");
		llvm::SMDiagnostic error;
		auto               m = llvm::parseIR(*memory_buffer.get(), error, getLLVMContext());
		if (!m) {
			std::string              error_message;
			llvm::raw_string_ostream error_stream(error_message);
			error.print("LLVM IR parsing error", error_stream);
			CORE_PANIC(error_message);
		}

		auto llvm_module = Box<llvm::Module>::fromPointer(m.release());
		return makeBox<ModuleImpl>(std::move(llvm_module));
	}

	llvm::Function* addFunctionToModuleInternal(
		query::Context& ctx, Ref<ModuleImpl> module, CRef<lir::Function> lir_function
	) {
		LirFunction2LLVM lir2llvm{ getLLVMContext(), ctx, lir_function, module->module.refMut() };
		return lir2llvm.createFunction();
	}

	void addFunctionToModuleImpl(
		query::Context& ctx, Ref<ModuleImpl> module, CRef<lir::Function> lir_function
	) {
		addFunctionToModuleInternal(ctx, module, lir_function);
	}

	void addFunctionToModuleCtorsImpl(
		query::Context& ctx, Ref<ModuleImpl> module, CRef<lir::Function> lir_function
	) {
		auto fun = addFunctionToModuleInternal(ctx, module, lir_function);
		// 65535 is the default priority for global constructors in LLVM.
		// There is also a 4-parameter Constant* Data = nullptr, which is the pointer to the global
		// variable associated with the constructor. However, the problem is that the order of
		// functions with the same priority is not defined. Therefore, we probably want to create
		// one global constructor that calls the constructor of each variable in the module.
		llvm::appendToGlobalCtors(*module->module.refMut(), fun, 65'535);
	}

	void addFunctionToModuleDtorsImpl(
		query::Context& ctx, Ref<ModuleImpl> module, CRef<lir::Function> lir_function
	) {
		auto fun = addFunctionToModuleInternal(ctx, module, lir_function);
		llvm::appendToGlobalDtors(*module->module.refMut(), fun, 65'535);
	}

	void addGlobalToModuleImpl(Ref<ModuleImpl> module, const lir::LirGlobal& lir_global) {
		addGlobalVariable(module->module.refMut(), lir_global);
	}
}
