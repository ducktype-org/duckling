#include <llvm_helpers/llvm_helpers.hpp>

#include <type_traits>

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
#include <helios/symbols/symbol_id_utils.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <tsl/type_layout.hpp>

#include <base/collections/maps.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/int_conv.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <init/init.hpp>
#include <logger/logger.hpp>

// useful: https://github.com/llvm/llvm-project/tree/main/llvm/examples
namespace {
	/**
	 * @brief Converts a NumericValue into its corresponding llvm::Constant representation.
	 * @param numeric The NumericValue to convert.
	 * @param llvm_type The expected LLVM type.
	 * @return The created llvm::Constant*.
	 */
	auto numericValueToLLVMConstant(
		const compiler::numeric_value::NumericValue& numeric, const Ref<llvm::Type> llvm_type
	) {
		return std::visit(
			[&](auto&& val) -> llvm::Constant* {
				using T = std::decay_t<decltype(val)>;

				if constexpr (std::is_integral_v<T>) {
					if (!llvm_type->isIntegerTy()) {
						CORE_PANIC(
							"LLVM lowering: Type mismatch. NumericValue is integer, but LLVM type "
							"is not"
						);
					}

					if constexpr (std::is_signed_v<T>)
						return llvm::ConstantInt::getSigned(llvm_type.get(), static_cast<i64>(val));
					else
						return llvm::ConstantInt::get(llvm_type.get(), static_cast<u64>(val), false);
				} else if constexpr (std::is_same_v<T, f32>) {
					if (!llvm_type->isFloatTy()) {
						CORE_PANIC(
							"LLVM lowering: Type mismatch. NumericValue is f32, but LLVM type is "
							"not"
						);
					}
					return llvm::ConstantFP::get(llvm_type.get(), static_cast<f64>(val));
				} else if constexpr (std::is_same_v<T, f64>) {
					if (!llvm_type->isDoubleTy()) {
						CORE_PANIC(
							"LLVM lowering: Type mismatch. NumericValue is f64, but LLVM type is "
							"not"
						);
					}
					return llvm::ConstantFP::get(llvm_type.get(), val);
				} else {
					CORE_PANIC("LLVM lowering: Unsupported numeric type in NumericValue");
				}
			},
			numeric.getStorage()
		);
	}

	/**
	 * @brief Converts a CTV into its corresponding llvm::Constant representation.
	 * @param ctv The CTV to convert.
	 * @param llvm_type The expected type.
	 * @param llvm_module The LLVM module into which global constants should be injected.
	 * @return The created llvm::Constant*.
	 */
	llvm::Constant* ctvToLLVMConstant(
		const compiler::ctv::CompileTimeValue& ctv,
		Ref<llvm::Type>                        llvm_type,
		Ref<llvm::Module>                      llvm_module
	) {
		variant_match(ctv.getStorage()) {
			variant_case(compiler::numeric_value::NumericValue, numeric) {
				return numericValueToLLVMConstant(numeric, llvm_type);
			}
			variant_case(bool, val) {
				if (!llvm_type->isIntegerTy(1)) {
					CORE_PANIC(
						"LLVM lowering: Type mismatch. CTV is a boolean, but LLVM type is not"
					);
				}
				return llvm::ConstantInt::get(llvm_type.get(), val ? 1 : 0, false);
			}
			variant_case_novalue(compiler::tsh::SymbolType<>) {
				// @TODO: #1709 This is a stub representation of meta types in LLVM for the code
				// using compile time operations on types to compile. This should never be used in
				// runtime.
				return llvm::ConstantInt::get(llvm_type.get(), 0, false);
			}
			variant_case(char, c) {
				return llvm::ConstantInt::get(llvm_type.get(), u64((unsigned char) c));
			}
			variant_case(base::StrID, str) {
				// First, create a global constant for the string data
				const auto string_constant = llvm::ConstantDataArray::getString(
					llvm_type->getContext(), str.strView(), /*AddNull=*/false
				);
				const auto string_global = new llvm::GlobalVariable(
					*llvm_module,
					string_constant->getType(),
					/*isConstant=*/true,
					llvm::GlobalValue::PrivateLinkage,
					string_constant
				);

				// Prepare the global struct
				const u64                          length = str.strView().size();
				const std::vector<llvm::Constant*> fields{
					string_global,
					// length is length
					llvm::ConstantInt::get(llvm_module->getContext(), llvm::APInt(64, length)),
					// memory_begin_offset (wrt. data pointer) is 0
					llvm::ConstantInt::get(llvm_module->getContext(), llvm::APInt(64, 0)),
					// memory_end_offset (wrt. data pointer) is equal to length
					llvm::ConstantInt::get(llvm_module->getContext(), llvm::APInt(64, length))
				};
				const auto struct_type     = llvm::cast<llvm::StructType>(llvm_type.get());
				const auto struct_constant = llvm::ConstantStruct::get(struct_type, fields);
				return struct_constant;
			}
			variant_case(compiler::ctv::CompileTimeValue::TupleCTV, tuple) {
				// @TODO: #2506 Implement this
				throw base::NotYetImplemented(base::strConcat(
					"Conversion from CTV to LLVM constant for tuples is not implemented yet"
				));
			}
			variant_default {
				throw base::NotYetImplemented(base::strConcat(
					"Conversion from CTV to LLVM constant for this type. Index in CTV "
					"variant: ",
					ctv.getStorage().index()
				));
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
		const bool v1 = llvm::InitializeNativeTarget();
		const bool v2 = llvm::InitializeNativeTargetAsmPrinter();

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

	/**
	 * @brief Converts a TypeLayout into its corresponding llvm::Type representation.
	 *
	 * Simple types usually have direct counterparts, such as integral and empty layouts. Composite
	 * types may require the creation of LLVM struct types, which are checked against the types
	 * already cached in the LLVM context.
	 *
	 * @param module The LLVM module to get the LLVM Context and Module DataLayout.
	 * @param layout The TypeLayout to convert.
	 * @return The created llvm::Type*.
	 */
	auto typeFromLayout(const Ref<llvm::Module> module, const CRef<tsl::TypeLayout> layout)
		-> llvm::Type* {
		auto& llvm_context = module->getContext();

		variant_match(layout->getVariant()) {
			variant_case_novalue(tsl::EmptyTypeLayout) {
				return llvm::Type::getVoidTy(llvm_context);
			}
			variant_case_novalue(tsl::IntegralTypeLayout) {
				return llvm::Type::getIntNTy(
					llvm_context, base::safeIntConv<unsigned>(static_cast<usize>(layout->getSize()))
				);
			}
			variant_case_novalue(tsl::FloatTypeLayout) {
				// see https://llvm.org/docs/LangRef.html#floating-point-types for docs on LLVM
				// floating point types
				switch (static_cast<usize>(layout->getSize())) {
				case 32:
					return llvm::Type::getFloatTy(llvm_context);
				case 64:
					return llvm::Type::getDoubleTy(llvm_context);
				default:
					CORE_PANIC("Float size different than 32 or 64 not implemented yet.");
				}
			}
			variant_case(tsl::StringTypeLayout, string_layout) {
				const auto string_type_name = string_layout.getMangledName().strView();

				// Get the string type from the context, if it has been previously defined.
				if (llvm::StructType* string_type
				    = llvm::StructType::getTypeByName(llvm_context, string_type_name);
				    string_type) {
					return string_type;
				}

				// Otherwise, define the string type in LLVM, in line with the TSL definition.
				llvm::StructType* string_type
					= llvm::StructType::create(llvm_context, string_type_name);
				string_type->setBody(
					{
						llvm::PointerType::getUnqual(llvm_context),
						i64Type(llvm_context),
						i64Type(llvm_context),
						i64Type(llvm_context),
					},
					/*is_packed=*/false
				);

				// @TODO: #1842 Add layout verification, that the LLVM struct layout matches:
				// - the TSL type layout, and
				// - the struct defined in the built-ins module.

				return string_type;
			}
			variant_case(tsl::DynamicArrayTypeLayout, list_layout) {
				const auto list_type_name = list_layout.getMangledName().strView();

				// Get the list type from the context, if it has been previously defined.
				if (llvm::StructType* list_type
				    = llvm::StructType::getTypeByName(llvm_context, list_type_name);
				    list_type) {
					return list_type;
				}

				// Otherwise, define the dynamic list type in LLVM, in line with the TSL definition.
				llvm::StructType* list_type
					= llvm::StructType::create(llvm_context, list_type_name);
				list_type->setBody(
					{
						llvm::PointerType::getUnqual(llvm_context),
						i64Type(llvm_context),
						i64Type(llvm_context),
						i64Type(llvm_context),
					},
					/*is_packed=*/false
				);

				// @TODO: #1842 Add layout verification, that the LLVM struct layout matches:
				// - the TSL type layout, and
				// - the struct defined in the built-ins module.

				return list_type;
			}
			variant_case(tsl::ClassTypeLayout, class_layout) {
				const auto class_name = class_layout.getMangledName().strView();

				// Get the struct from the context, if it has been previously defined.
				if (llvm::StructType* struct_type
				    = llvm::StructType::getTypeByName(llvm_context, class_name);
				    struct_type) {
					return struct_type;
				}

				// Otherwise, define the struct in LLVM.
				// - First, create an opaque type.
				llvm::StructType* struct_type = llvm::StructType::create(llvm_context, class_name);
				// - Then, collect the member types.
				const usize              num_sub_layouts = class_layout.getNumSubLayouts();
				std::vector<llvm::Type*> member_types;
				member_types.reserve(num_sub_layouts);
				for (usize layout_idx = 0; layout_idx < num_sub_layouts; layout_idx++) {
					const CRef<tsl::TypeLayout> field_layout
						= class_layout.getFieldLayoutOfLayoutIndex(layout_idx);
					member_types.push_back(typeFromLayout(module, field_layout));
				}
				// - Finally, set the body of the struct and return it.
				struct_type->setBody(member_types, /*is_packed=*/false);

				// Now, confirm that the LLVM struct layout matches the TSL type layout.
				// - First, get the LLVM struct layout.
				const llvm::DataLayout&   data_layout   = module->getDataLayout();
				const llvm::StructLayout& struct_layout = *data_layout.getStructLayout(struct_type);

				// - Then, check each field's offset.
				for (usize layout_idx = 0; layout_idx < num_sub_layouts; layout_idx++) {
					[[maybe_unused]] const Bytes expected_offset
						= class_layout
					          .getOffsetOfFieldSymbol(
								  class_layout.getFieldSymbolOfLayoutIndex(layout_idx)
							  )
					          .value();
					[[maybe_unused]] const auto actual_offset = Bytes(
						struct_layout.getElementOffset(base::safeIntConv<unsigned>(layout_idx))
					);
					CORE_ASSERT(
						expected_offset == actual_offset,
						base::strConcat(
							"LLVM struct layout mismatch for class '",
							class_name,
							"' at field index ",
							base::toString(layout_idx),
							": expected offset ",
							base::toString(expected_offset),
							", got ",
							base::toString(actual_offset)
						)
					);
				}

				// Finally, return the struct type.
				return struct_type;
			}
			variant_case(tsl::PointerTypeLayout, pointer_layout) {
				return llvm::PointerType::getUnqual(llvm_context);
			}
			variant_case(tsl::StaticArrayTypeLayout, static_array_layout) {
				llvm::Type* element_type
					= typeFromLayout(module, static_array_layout.getElementLayout());
				return llvm::ArrayType::get(element_type, static_array_layout.getElementCount());
			}
			variant_case(tsl::MetaTypeLayout, meta_layout) {
				// @TODO: #1709 This is a stub representation of meta types in LLVM for the code
				// using compile time operations on types to compile. This should never be used in
				// runtime.
				return llvm::Type::getIntNTy(
					llvm_context, base::safeIntConv<unsigned>(static_cast<usize>(layout->getSize()))
				);
			}
			variant_default {
				CORE_PANIC(
					base::strConcat("Type not handled yet: ", layout->toStringIdentification())
				);
			}
		}
		CORE_UNREACHABLE();
	}

	/**
	 * Get the LLVM function type based on the layouts of its parameters and return type.
	 * @note If the function type has to conform to C/C++ ABI, then struct-like parameters
	 * should be passed by pointer and with the `byval` LLVM attribute. See:
	 * https://yorickpeterse.com/articles/the-mess-that-is-handling-structure-arguments-and-returns-in-llvm/.
	 * @param module The LLVM module in which the function type will be used.
	 * @param parameters The layouts of the parameters of the function.
	 * @param return_type The layout of the return type of the function.
	 * @param abi The ABI to conform to.
	 * @return The LLVM function type.
	 */
	auto getFunType(
		const Ref<llvm::Module>                   module,
		const std::vector<CRef<tsl::TypeLayout>>& parameters,
		const CRef<tsl::TypeLayout>               return_type,
		const helios::SymbolABI                   abi = helios::DefaultAbi{}
	) {
		std::vector<llvm::Type*> llvm_parameters;
		llvm_parameters.reserve(parameters.size());

		// Prepare parameter types.
		for (const auto& param: parameters)
			if (std::holds_alternative<helios::CAbi>(abi) and param->is<tsl::StringTypeLayout>())
				llvm_parameters.push_back(llvm::PointerType::getUnqual(module->getContext()));
			else
				llvm_parameters.push_back(typeFromLayout(module, param));

		// Prepare function type, including return type.
		return llvm::FunctionType::get(typeFromLayout(module, return_type), llvm_parameters, false);
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
		const Ref<llvm::Module> module, const lir::FunctionLiteral& function_literal
	) {
		const auto mangled_name = function_literal.mangled_name;
		// We check if function exist first, to avoid unnecessary construction of types:
		if (const auto func = module->getFunction(mangled_name.strView())) return func;

		llvm::FunctionCallee callee = module->getOrInsertFunction(
			mangled_name.strView(),
			getFunType(
				module,
				*function_literal.parameter_layouts,
				function_literal.return_type_layout,
				function_literal.abi
			)
		);

		if (auto* function = llvm::dyn_cast<llvm::Function>(callee.getCallee())) {
			function->setCallingConv(getCallingConvFromABI(function_literal.abi));

			// If the function uses C ABI, we need to pass structs by pointer with `byval` attribute.
			if (std::holds_alternative<helios::CAbi>(function_literal.abi)) {
				for (usize i = 0; i < function_literal.parameter_layouts->size(); i++) {
					if (const auto param_layout = function_literal.parameter_layouts->at(i);
					    param_layout->is<tsl::StringTypeLayout>()) {
						function->addParamAttr(
							u32(i),
							llvm::Attribute::getWithByValType(
								module->getContext(), typeFromLayout(module, param_layout)
							)
						);
					}
				}
			}
		}

		return callee;
	}

	llvm::FunctionCallee getOrInsertFunctionPrototypeFromLIRFunction(
		const Ref<llvm::Module> module, const lir::Function& lir_function
	) {
		return getOrInsertFunctionPrototypeFromLiteral(
			module, lir::FunctionLiteral::fromFunction(lir_function)
		);
	}

	Ref<llvm::Constant> getOrInsertGlobalVariable(
		const Ref<llvm::Module> module, const lir::LIRGlobal& lir_global
	) {
		const auto mangled_name = lir_global.mangled_name.strView();

		if (const auto global = module->getGlobalVariable(mangled_name)) return global;

		const auto global_type = typeFromLayout(module, lir_global.layout);

		return module->getOrInsertGlobal(mangled_name, global_type);
	}

	/**
	 * Adds a global variable to the module based on the LIRGlobal description.
	 * For globals is sets the initial value to null (this function does not handle constructors),
	 * for constants it sets the initial value to the provided constant value.
	 */
	Ref<llvm::GlobalVariable> addGlobalVariable(
		const Ref<llvm::Module> module, const lir::LIRGlobal& lir_global
	) {
		getOrInsertGlobalVariable(module, lir_global);
		const Ref<llvm::GlobalVariable> global
			= module->getNamedGlobal(lir_global.mangled_name.strView());

		CORE_ASSERT(global->isDeclaration(), "Global is not the declaration");

		global->setLinkage(llvm::GlobalValue::ExternalLinkage);
		global->setConstant(lir_global.type == lir::LIRGlobalType::Constant);

		if (lir_global.type == lir::LIRGlobalType::Constant) {
			CORE_ASSERT(
				lir_global.initial_value.has_value(), "Expected initial value for constant global"
			);
			global->setInitializer(
				ctvToLLVMConstant(lir_global.initial_value.value(), global->getValueType(), module)
			);
		} else {
			// Initialise the global variable to null, since it will be initialised in the constructor:
			CORE_ASSERT(
				not lir_global.initial_value.has_value(),
				"Non-constant global should not have initial value set"
			);
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
	struct LIRFunction2LLVM {
		Ref<llvm::Module>   module;
		llvm::LLVMContext&  context;
		query::Context&     ctx;
		CRef<lir::Function> lir_function;

		LIRFunction2LLVM(
			query::Context&           ctx,
			const CRef<lir::Function> lir_function,
			const Ref<llvm::Module>   module
		):
			  module(module),
			  context(module->getContext()),
			  ctx(ctx),
			  lir_function(lir_function) {}

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

		base::Map<lir::LIRLocalRef, u64> lir_local_ids;

		std::string llvmLocalName(const lir::LIRLocalRef lir_local) {
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
		base::Map<lir::LIRLocalRef, Ref<llvm::Instruction>> local_register_map;

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
					var.layout->getSize() > Bits(0),
					"local variable with size 0 is not allowed in LLVM"
				);
				auto reg = locals_builder.CreateAlloca(
					typeFromLayout(module, var.layout), nullptr, llvmLocalName(&var)
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
		 * @brief Maps LIRPlace to an LLVM pointer Value.
		 *
		 * This function may generate new LLVM instructions if necessary. For example,
		 * it may need to generate a GEP instruction to access a field of a struct or a load
		 * if LIRPlace has to be dereferenced.
		 *
		 * The overview of what this does is:
		 * - For a given LIRValue take a pointer to it.
		 * - Iterate through the projection chain which can store `FieldProjection`,
		 * `DerefProjection`, and `IndexProjection`.
		 * and add subsequent arguments to the currently built GEP instruction.
		 * - If a `DerefProjection` is encountered, we have to emit the GEP built up to this point,
		 * perform a load on the address it returned and start building a new GEP.
		 *
		 * @param place The LIRPlace to convert into an LLVM pointer Value.
		 * @param builder The LLVM IRBuilder to use for generating the pointer, if necessary.
		 * @return Pointer to the place described by `lir_place`.
		 */
		auto gepPointerFromLIRPlace(const lir::LIRPlace& place, llvm::IRBuilder<>& builder)
			-> llvm::Value* {
			// First, get the pointer and type of the base value.
			auto current_ptr = [&] -> llvm::Value* {
				variant_match(place.base) {
					variant_case(lir::LIRLocalRef, lir_local) {
						return local_register_map[lir_local].get();
					}
					variant_case(lir::LIRGlobal, lir_global) {
						return getOrInsertGlobalVariable(module, lir_global).get();
					}
				}
				CORE_UNREACHABLE();
			}();

			// Then, perform appropriate pointer modification based on the projection chain.
			// If the projection chain is empty, we can return the base pointer directly.
			if (not place.hasProjections()) return current_ptr;

			// Otherwise, we need to get the layout indices of the accessed fields.
			llvm::Type*               current_type = typeFromLayout(module, place.getBaseLayout());
			CRef<tsl::TypeLayout>     current_layout = place.getBaseLayout();
			std::vector<llvm::Value*> gep_indices;

			auto llvm_i32 = [&](u64 val) {
				return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), val);
			};

			// A GEP constructor invoked when encountering a deref projection or when we went
			// through all projections. Creates a GEP from all projection indices up to this point
			// so `load` can be performed on the calculated address.
			auto flush_gep = [&]() {
				// Skip if GEP has no arguments.
				if (gep_indices.empty()) return;
				// Create a GEP if needed.
				current_ptr  = builder.CreateGEP(current_type, current_ptr, gep_indices);
				current_type = typeFromLayout(module, current_layout);
				gep_indices.clear();
			};

			// In LLVM, the first index of a GEP on a pointer navigates "through" the pointer
			// (treating it as an array). To access a structure's field, the first index
			// must be 0. This helper ensures such a base index exists for projections that need them.
			auto ensure_structural_base = [&]() {
				if (gep_indices.empty()) gep_indices.push_back(llvm_i32(0));
			};

			for (const auto& projection: place.projection_chain) {
				variant_match(projection.storage) {
					variant_case(lir::LIRPlace::FieldProjection, field) {
						ensure_structural_base();

						const auto& current_class_layout
							= std::get<tsl::ClassTypeLayout>(current_layout->getVariant());
						const auto layout_idx
							= current_class_layout.getLayoutIndexOfFieldSymbol(field.field_id)
						          .value();

						gep_indices.push_back(
							llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), layout_idx)
						);
						current_layout
							= current_class_layout.getFieldLayoutOfLayoutIndex(layout_idx);
					}
					variant_case(lir::LIRPlace::IndexProjection, index) {
						// First load the index value.
						llvm::Value* index_value = loadLIRValue(*index.index, builder);
						variant_match(current_layout->getVariant()) {
							variant_case(tsl::StaticArrayTypeLayout, static_array_layout) {
								ensure_structural_base();
								// Then add it as the next argument to the GEP.
								gep_indices.push_back(index_value);
								// Lastly, update the current layout.
								current_layout = static_array_layout.getElementLayout();
							}
							variant_case(tsl::DynamicArrayTypeLayout, dynamic_array_layout) {
								// @TODO: #1970 This branch currently performs an access to a 'data'
								// field of the list, dereferences it and performs an index
								// projection on the pointer to the heap data. If `List[T]` had a
								// proper type interface (including a 'data' field which returns a
								// `ref T` or `T*`), a dynamic array index access could be
								// represented by `Field(Data), Deref, IndexProjection`. Then the
								// whole implementation of this case for the DynamicArrayTypeLayout,
								// would be the same as for static arrays. For now, this
								// programmatically implements the thing described above.
								ensure_structural_base();

								// Add an additional FieldProjection('data') so we access the data
								// field with one GEP. Note that data is at 0 index in the struct.
								gep_indices.push_back(llvm_i32(0));

								// Emit the current GEP to get pointer to the heap data.
								flush_gep();

								// Now load the actual data of the list. This now points directly to
								// the data on the heap.
								current_ptr = builder.CreateLoad(builder.getPtrTy(), current_ptr);

								// Now push the actual index from the IndexProjection. This GEP now
								// operates on the heap memory.
								gep_indices.push_back(index_value);

								// Lastly, update the types and layouts.
								current_layout = dynamic_array_layout.getElementLayout();
								current_type   = typeFromLayout(module, current_layout);
							}
							variant_case(tsl::PointerTypeLayout, pointer_layout) {
								// Finish any struct/array GEP first
								flush_gep();

								// Load the pointer value (because current_ptr points to storage)
								current_ptr = builder.CreateLoad(builder.getPtrTy(), current_ptr);

								// Now build a NEW GEP for pointer arithmetic
								current_ptr = builder.CreateGEP(
									typeFromLayout(module, pointer_layout.getPointee()),
									current_ptr,
									index_value
								);

								// Clear indices because we emitted the GEP directly
								gep_indices.clear();

								// Update layout/type
								current_layout = pointer_layout.getPointee();
								current_type   = typeFromLayout(module, current_layout);
							}
							variant_default { CORE_PANIC("Indexing into a non-array layout"); }
						}
					}
					variant_case_novalue(lir::LIRPlace::DerefProjection) {
						// If deref was encountered, we have to create a GEP which includes all the
						// projections built up to this point and perform a load.
						flush_gep();

						current_ptr = builder.CreateLoad(builder.getPtrTy(), current_ptr);

						const auto& current_pointer_layout
							= std::get<tsl::PointerTypeLayout>(current_layout->getVariant());
						current_layout = current_pointer_layout.getPointee();
						current_type   = typeFromLayout(module, current_layout);
					}
				}
			}

			// After no more projections exist, we create a GEP instruction with all projections up
			// to this point. `current_ptr` stores a value which is the result of GEP.
			flush_gep();
			return current_ptr;
		}

		/**
		 * @brief Maps LIRValue to an LLVM Value.
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
		 * @return The loaded LLVM Value.
		 */
		auto loadLIRValue(const lir::LIRValue& lir_location, llvm::IRBuilder<>& builder)
			-> llvm::Value* {
			variant_match(lir_location.getVariant()) {
				variant_case(lir::LIRConstant, constant) {
					const auto llvm_type = typeFromLayout(module, constant.layout);
					return ctvToLLVMConstant(constant.value, llvm_type, module);
				}
				variant_case(lir::LIRPlace, place) {
					// We store local values behind pointers to stack-allocated memory.
					// We need to load them (or their fields) before using them.
					// Similarly, we need to use global values or their fields before use.
					const auto   llvm_type    = typeFromLayout(module, place.layout);
					llvm::Value* accessed_ptr = gepPointerFromLIRPlace(place, builder);
					return builder.CreateLoad(llvm_type, accessed_ptr);
				}
				variant_case(lir::BlockRef, lir_block) { return block_mapping[lir_block].get(); }
				variant_default { CORE_PANIC("unknown lir location type"); }
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Gets a LLVM pointer to the given LIRValue.
		 *
		 * This is used when a pointer to data is required (e.g., `builtin_list_push(list*, void*,
		 * uint64_t)`, requires a pointer to the inserted value for the implementation to work for
		 * generic list).
		 *
		 * - For `LIRPlace`, it returns the calculated address via `gepPointerFromLIRPlace`.
		 * - For `LIRConstant`, it loads the constant value into a created temporary and returns the
		 * address of the temporary.
		 * - Panics for other LIRValue variants (like BlockRef or FunctionLiteral).
		 *
		 * @param lir_location The LIRValue to obtain a pointer for.
		 * @param builder The LLVM IRBuilder to use for generating instructions.
		 * @return `llvm::Value*` with the pointer to the data.
		 */
		auto loadLIRValueToPointer(const lir::LIRValue& lir_location, llvm::IRBuilder<>& builder)
			-> llvm::Value* {
			variant_match(lir_location.getVariant()) {
				variant_case(lir::LIRPlace, place) {
					return gepPointerFromLIRPlace(place, builder);
				}
				variant_case(lir::LIRConstant, constant) {
					llvm::Value* val = loadLIRValue(lir_location, builder);
					auto* alloca = builder.CreateAlloca(val->getType(), nullptr, "tmp_const_ptr");
					builder.CreateStore(val, alloca);
					return alloca;
				}
				variant_case(lir::FunctionLiteral, func) {
					llvm::Value* val = loadLIRValue(lir_location, builder);
					auto* alloca = builder.CreateAlloca(val->getType(), nullptr, "tmp_func_ptr");
					builder.CreateStore(val, alloca);
					return alloca;
				}
				variant_default { CORE_PANIC("Cannot get pointer to BlockRef"); }
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Maps a collection of LIRValues to a LLVM Values.
		 *
		 * This function may generate new LLVM instructions if necessary, see `loadLIRValue`.
		 *
		 * @param lir_locations The LIRValues to convert into LLVM Values.
		 * @param builder The LLVM IRBuilder to use for loading the values, if necessary.
		 * @return The vector of loaded LLVM Values.
		 */
		auto loadLIRValueList(
			const std::vector<lir::LIRValue>& lir_locations, llvm::IRBuilder<>& builder
		) -> std::vector<llvm::Value*> {
			std::vector<llvm::Value*> llvm_locations;
			llvm_locations.reserve(lir_locations.size());
			for (const auto& lir_location: lir_locations)
				llvm_locations.push_back(loadLIRValue(lir_location, builder));
			return llvm_locations;
		}

		void storeOutput(
			const lir::LIRPlace& output, const Ref<llvm::Value> value, llvm::IRBuilder<>& builder
		) {
			// We store local values behind pointers to stack-allocated memory.
			// We need to load them (or their fields) before using them.
			// Similarly, we need to use global values or their fields before use.
			llvm::Value* accessed_ptr = gepPointerFromLIRPlace(output, builder);
			builder.CreateStore(value.get(), accessed_ptr);
		}

		llvm::Value* castOperation(
			llvm::Value* argument, llvm::IRBuilder<>& builder, const lir::CastParameters& cast_params
		) const {
			const auto  target_layout = cast_params.target_layout;
			const auto  source_layout = cast_params.source_layout;
			const auto& source_type   = cast_params.source_type;
			const auto& target_type   = cast_params.target_type;

			const auto llvm_dst_ty = typeFromLayout(module, target_layout);

			const auto src_bits
				= base::safeIntConv<unsigned>(static_cast<usize>(source_layout->getSize()));
			const auto dst_bits
				= base::safeIntConv<unsigned>(static_cast<usize>(target_layout->getSize()));

			auto is_signed = [](const tsh::SymbolType<>& type) -> bool {
				if (type.getType().getKind() == tsh::Kind::Integral) {
					return (
						tsh::IntegralAbstractType(type.getType()).getSignedness()
						== tsh::IntegralAbstractType::Signedness::Signed
					);
				}
				// Char, bool, etc. are treated as unsigned
				return false;
			};

			variant_match(source_layout->getVariant()) {
				variant_case_novalue(tsl::IntegralTypeLayout) {
					// signedness comes from the symbol-level type information
					const bool src_signed = is_signed(source_type);

					variant_match(target_layout->getVariant()) {
						variant_case_novalue(tsl::IntegralTypeLayout) {
							// ================== Int -> Int ==================
							return builder.CreateIntCast(argument, llvm_dst_ty, src_signed);
						}
						variant_case_novalue(tsl::FloatTypeLayout) {
							// ================== Int -> Float ==================
							return src_signed ? builder.CreateSIToFP(argument, llvm_dst_ty)
							                  : builder.CreateUIToFP(argument, llvm_dst_ty);
						}
						variant_case_novalue(tsl::PointerTypeLayout) {
							// ================== Int -> Pointer ==================
							constexpr auto PTR_BITS
								= static_cast<unsigned>(static_cast<usize>(tsl::POINTER_SIZE));
							const auto ptr_int_ty
								= llvm::Type::getIntNTy(builder.getContext(), PTR_BITS);
							llvm::Value* int_for_ptr
								= builder.CreateIntCast(argument, ptr_int_ty, src_signed);
							return builder.CreateIntToPtr(int_for_ptr, llvm_dst_ty);
						}
						variant_default {
							CORE_PANIC("Unsupported cast from integral-layout to target layout");
						}
					}
				}

				variant_case_novalue(tsl::FloatTypeLayout) {
					variant_match(target_layout->getVariant()) {
						variant_case_novalue(tsl::FloatTypeLayout) {
							// ================== Float -> Float ==================

							if (dst_bits > src_bits)
								return builder.CreateFPExt(argument, llvm_dst_ty);
							else if (dst_bits < src_bits)
								return builder.CreateFPTrunc(argument, llvm_dst_ty);
							else
								return argument;
						}
						variant_case_novalue(tsl::IntegralTypeLayout) {
							// ================== Float -> Int ==================


							// Here is a problem when the float is NaN or out of range of the int
							// then the behavior is undefined.
							const bool to_signed = is_signed(target_type);

							// Some other solution to consider in the future;
							// bool use_saturating_float_casts = true;

							// if (not use_saturating_float_casts) {
							// 	return to_signed ? builder.CreateFPToSI(argument, llvm_dst_ty)
							// 	                 : builder.CreateFPToUI(argument, llvm_dst_ty);
							// }

							// Use LLVM saturating fptosi/fptoui intrinsics when available.
							std::string instr = to_signed ? "fptosi" : "fptoui";

							// scalar
							llvm::FunctionType* func_type = llvm::FunctionType::get(
								llvm_dst_ty, { argument->getType() }, false
							);

							// name =  llvm.{fptosi, fptoui}.sat.i{int_width}.f{float_width}
							const std::string name = base::strConcat(
								"llvm.",
								instr,
								".sat.i",
								std::to_string(dst_bits),
								".f",
								std::to_string(src_bits)
							);

							const llvm::FunctionCallee fdecl
								= module->getOrInsertFunction(name, func_type);
							return builder.CreateCall(fdecl, { argument });
						}
						variant_default {
							CORE_PANIC("Unsupported cast from float-layout to target layout");
						}
					}
				}

				variant_case_novalue(tsl::PointerTypeLayout) {
					variant_match(target_layout->getVariant()) {
						variant_case_novalue(tsl::IntegralTypeLayout) {
							// ================== Pointer -> Int  ==================
							constexpr auto PTR_BITS
								= static_cast<unsigned>(static_cast<usize>(tsl::POINTER_SIZE));
							const auto ptr_int_ty
								= llvm::Type::getIntNTy(builder.getContext(), PTR_BITS);
							const auto int_from_ptr = builder.CreatePtrToInt(argument, ptr_int_ty);
							return builder.CreateIntCast(int_from_ptr, llvm_dst_ty, false);
						}
						variant_case_novalue(tsl::PointerTypeLayout) {
							// =================== Pointer -> Pointer ==================
							return builder.CreateBitCast(argument, llvm_dst_ty);
						}
						variant_case_novalue(tsl::FloatTypeLayout) {
							// ================== Pointer -> Float ==================
							constexpr auto PTR_BITS
								= static_cast<unsigned>(static_cast<usize>(tsl::POINTER_SIZE));
							const auto ptr_int_ty
								= llvm::Type::getIntNTy(builder.getContext(), PTR_BITS);
							const auto int_from_ptr = builder.CreatePtrToInt(argument, ptr_int_ty);
							return builder.CreateUIToFP(int_from_ptr, llvm_dst_ty);
						}
						variant_default {
							CORE_PANIC("Unsupported cast from pointer-layout to target layout");
						}
					}
				}
				// For other layouts (variant, class, tuple, etc.) fallback to symbol-kind
				// based panic
				variant_default { CORE_PANIC("Unsupported cast source layout in LLVM lowering"); }
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Retrieves or inserts a built-in function prototype in the LLVM module.
		 */
		auto loadBuiltin(
			const std::string_view             name,
			llvm::Type*                        ret_type,
			std::initializer_list<llvm::Type*> args
		) -> llvm::FunctionCallee {
			return module->getOrInsertFunction(name, llvm::FunctionType::get(ret_type, args, false));
		}

#define LIR_2_LLVM_BINARY_OPERATION_CASE(op)                                       \
	{                                                                              \
		const auto lhs   = loadLIRValue(lir_instruction.arguments.at(0), builder); \
		const auto rhs   = loadLIRValue(lir_instruction.arguments.at(1), builder); \
		const auto value = builder.Create##op(lhs, rhs);                           \
		storeOutput(lir_instruction.output.value(), value, builder);               \
		break;                                                                     \
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
				builder.CreateRet(loadLIRValue(lir_instruction.arguments.at(0), builder));
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
				const auto cond = loadLIRValue(lir_instruction.arguments.at(0), builder);
				const auto true_block
					= block_mapping[lir_instruction.arguments.at(1).get<lir::BlockRef>()];
				const auto false_block
					= block_mapping[lir_instruction.arguments.at(2).get<lir::BlockRef>()];
				builder.CreateCondBr(cond, true_block.get(), false_block.get());
				break;
			}
			case Assign: {
				const auto value = loadLIRValue(lir_instruction.arguments.at(0), builder);
				storeOutput(lir_instruction.output.value(), value, builder);
				break;
			}
			case AddressOf: {
				const auto& src_place
					= std::get<lir::LIRPlace>(lir_instruction.arguments.at(0).getVariant());
				llvm::Value* address = gepPointerFromLIRPlace(src_place, builder);
				storeOutput(lir_instruction.output.value(), address, builder);
				break;
			}
			case ZeroInitialize: {
				const auto&  output = lir_instruction.output.value();
				llvm::Value* ptr    = gepPointerFromLIRPlace(output, builder);
				llvm::Type*  type   = typeFromLayout(module, output.layout);

				builder.CreateStore(llvm::Constant::getNullValue(type), ptr);
				break;
			}
			case BoxAlloc: {
				// First, get the value to box.
				const auto  value_to_box = loadLIRValue(lir_instruction.arguments.at(0), builder);
				llvm::Type* pointee_type = value_to_box->getType();

				// Calculate the layout size for malloc.
				const llvm::DataLayout& data_layout = module->getDataLayout();
				usize                   size        = data_layout.getTypeAllocSize(pointee_type);

				// Get or insert the allocator.
				auto alloc_func
					= loadBuiltin("builtin_alloc", builder.getPtrTy(), { builder.getInt64Ty() });

				// Actually call the allocator.
				llvm::Value* size_val = builder.getInt64(size);
				llvm::Value* allocated_ptr
					= builder.CreateCall(alloc_func, { size_val }, "box_ptr");

				// Store the value in the allocated memory.
				// @TODO: #1895 This is suboptimal. In the future class constructors should take the
				// allocated memory pointer as a parameter and construct it in-place.
				builder.CreateStore(value_to_box, allocated_ptr);
				storeOutput(lir_instruction.output.value(), allocated_ptr, builder);
				break;
			}
			case BoxFree: {
				// @TODO: #1894 This may change based on the way we handle destructors.
				const auto ptr_to_free = loadLIRValue(lir_instruction.arguments.at(0), builder);

				// Get or insert the free function.
				auto free_func
					= loadBuiltin("builtin_dealloc", builder.getVoidTy(), { builder.getPtrTy() });

				// Actually free the memory.
				builder.CreateCall(free_func, { ptr_to_free });
				break;
			}
			case ListPush:
			case ListPop:
			case ListLen:
			case ListFree: {
				llvm::Value* list_ptr
					= loadLIRValueToPointer(lir_instruction.arguments.at(0), builder);

				// Get the size of the List element. Needed to pass to the generic
				// `builtin_list_push`/'builtin_list_pop' builtins.
				auto get_elem_size = [&]() {
					const auto& params
						= std::get<lir::ListOperationParameters>(lir_instruction.extra_params);
					return builder.getInt64(
						static_cast<u64>(base::bits2bytes(params.element_layout->getSize()))
					);
				};

				switch (lir_instruction.operation) {
				case ListPush: {
					llvm::Value* element_ptr
						= loadLIRValueToPointer(lir_instruction.arguments.at(1), builder);

					auto push_func = loadBuiltin(
						"builtin_list_push",
						builder.getVoidTy(),
						{ builder.getPtrTy(), builder.getPtrTy(), builder.getInt64Ty() }
					);

					builder.CreateCall(push_func, { list_ptr, element_ptr, get_elem_size() });
					break;
				}
				case ListPop: {
					llvm::Value* count_val = loadLIRValue(lir_instruction.arguments.at(1), builder);

					auto pop_func = loadBuiltin(
						"builtin_list_pop",
						builder.getVoidTy(),
						{ builder.getPtrTy(), builder.getInt64Ty(), builder.getInt64Ty() }
					);

					builder.CreateCall(pop_func, { list_ptr, count_val, get_elem_size() });
					break;
				}
				case ListLen: {
					auto len_func = loadBuiltin(
						"builtin_list_len", builder.getInt64Ty(), { builder.getPtrTy() }
					);

					llvm::Value* result = builder.CreateCall(len_func, { list_ptr });
					storeOutput(lir_instruction.output.value(), result, builder);
					break;
				}
				case ListFree: {
					auto free_func = loadBuiltin(
						"builtin_list_free", builder.getVoidTy(), { builder.getPtrTy() }
					);

					builder.CreateCall(free_func, { list_ptr });
					break;
				}
				default:
					CORE_UNREACHABLE();
				}
				break;
			}
			/// Integer arithmetic ///
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
			case IntegerNeg: {
				const auto output   = lir_instruction.output.value();
				const auto argument = loadLIRValue(lir_instruction.arguments.at(0), builder);
				const auto value    = builder.CreateNeg(argument);
				storeOutput(output, value, builder);
				break;
			}

			/// Floatin point arithmetic ///
			case FloatAdd:
				LIR_2_LLVM_BINARY_OPERATION_CASE(FAdd)
			case FloatSub:
				LIR_2_LLVM_BINARY_OPERATION_CASE(FSub)
			case FloatMul:
				LIR_2_LLVM_BINARY_OPERATION_CASE(FMul)
			case FloatDiv:
				LIR_2_LLVM_BINARY_OPERATION_CASE(FDiv)
			case FloatNeg: {
				const auto output   = lir_instruction.output.value();
				const auto argument = loadLIRValue(lir_instruction.arguments.at(0), builder);
				const auto value    = builder.CreateFNeg(argument);
				storeOutput(output, value, builder);
				break;
			}

			/// Integer comparisons ///
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

			/// Floating point comparisons ///
			// @note: We use Oxx instead of Uxx for the comparisons to be "ordered". This basically
			// means if any of the operands is NaN the result of the operation is always false. The
			// `unordered` alternative returns true if any of the operands is NaN.
			case FloatLt:
				LIR_2_LLVM_BINARY_OPERATION_CASE(FCmpOLT)
			case FloatGt:
				LIR_2_LLVM_BINARY_OPERATION_CASE(FCmpOGT)
			case FloatLteq:
				LIR_2_LLVM_BINARY_OPERATION_CASE(FCmpOLE)
			case FloatGteq:
				LIR_2_LLVM_BINARY_OPERATION_CASE(FCmpOGE)
			case FloatEq:
				LIR_2_LLVM_BINARY_OPERATION_CASE(FCmpOEQ)
			case FloatNeq:
				LIR_2_LLVM_BINARY_OPERATION_CASE(FCmpONE)

			/// Logic ///
			case BooleanAnd:
				LIR_2_LLVM_BINARY_OPERATION_CASE(LogicalAnd)
			case BooleanOr:
				LIR_2_LLVM_BINARY_OPERATION_CASE(LogicalOr)
			case BooleanNot: {
				const auto argument = loadLIRValue(lir_instruction.arguments.at(0), builder);
				const auto value    = builder.CreateNot(argument);
				storeOutput(lir_instruction.output.value(), value, builder);
				break;
			}
			case Cast: {
				const auto argument = loadLIRValue(lir_instruction.arguments.at(0), builder);
				const auto output   = lir_instruction.output.value();

				// get cast parameters:
				const auto cast_params
					= std::get_if<lir::CastParameters>(&lir_instruction.extra_params);

				CORE_ASSERT(cast_params != nullptr, "Cast instruction without parameters");

				const auto value = castOperation(argument, builder, *cast_params);

				storeOutput(output, value, builder);
				break;
			}
			case Call: {
				CORE_ASSERT(lir_instruction.arguments.size() > 0, "call instruction without callee");

				const auto function_literal
					= lir_instruction.arguments.at(0).get<lir::FunctionLiteral>();
				auto callee = getOrInsertFunctionPrototypeFromLiteral(module, function_literal);

				auto args = loadLIRValueList(
					std::vector(
						lir_instruction.arguments.begin() + 1, lir_instruction.arguments.end()
					),
					builder
				);
				// If the function uses C ABI, we need to pass structs by pointer.
				std::vector<usize> byval_indices{};
				if (std::holds_alternative<helios::CAbi>(function_literal.abi)) {
					for (usize i = 0; i < function_literal.parameter_layouts->size(); i++) {
						if (const auto param_layout = function_literal.parameter_layouts->at(i);
						    param_layout->is<tsl::StringTypeLayout>()) {
							byval_indices.push_back(i);
							const auto str_type = typeFromLayout(module, param_layout);
							const auto arg_ptr  = builder.CreateAlloca(str_type);
							builder.CreateStore(args.at(i), arg_ptr);
							args.at(i) = arg_ptr;
						}
					}
				}

				llvm::CallInst* call_instruction = nullptr;
				if (lir_instruction.output.has_value()) {
					const auto output = lir_instruction.output.value();
					call_instruction  = builder.CreateCall(callee, args);
					storeOutput(output, call_instruction, builder);
				} else {
					CORE_ASSERT(
						callee.getFunctionType()->getReturnType()->isVoidTy(),
						"call to non void function without output "
						"– this may be valid, feel free "
						"to remove assertion if the compiler internals change."
					);
					call_instruction = builder.CreateCall(callee, args);
				}
				for (const auto byval_idx: byval_indices) {
					const auto arg_type
						= typeFromLayout(module, function_literal.parameter_layouts->at(byval_idx));
					call_instruction->addParamAttr(
						u32(byval_idx), llvm::Attribute::getWithByValType(context, arg_type)
					);
				}

				break;
			}
			case Nop:
				// No instruction to generate, just skip.
				break;
			default:
				CORE_DEV_LOG(
					Backend,
					"Unknown LIR operation in LLVM backend, skipping: ",
					base::enumToStr(lir_instruction.operation),
					"\n"
				);
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
			const Ref fun = llvm::cast<llvm::Function>(
				getOrInsertFunctionPrototypeFromLIRFunction(module, *lir_function).getCallee()
			);
			CORE_ASSERT(fun->isDeclaration(), "function is not a declaration");

			generateMainBlocksAndLocals(fun.get());

			for (auto& block: lir_function->block_order) {
				auto            llvm_block = block_mapping[block];
				llvm::IRBuilder builder(llvm_block.get());
				for (const auto& instruction: block->instructions)
					lirInstruction2LLVM(instruction, builder);
				lirInstruction2LLVM(block->terminator, builder);
			}

			return fun.get();
		}
	};

	Box<ModuleImpl> initModuleImpl(const base::StrID module_id) {
		init();
		llvm::LLVMContext& context = getLLVMContext();

		Box<llvm::Module> llvm_module = makeBox<llvm::Module>(module_id.str(), context);
		return makeBox<ModuleImpl>(std::move(llvm_module));
	}

	Box<ModuleImpl> parseIRCodeToModuleImpl(const std::string_view llvm_ir_code) {
		const auto memory_buffer = llvm::MemoryBuffer::getMemBuffer(llvm::StringRef(llvm_ir_code));
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

	Box<ModuleImpl> parseLLVMBCToModuleImpl(const std::span<unsigned char> llvm_bc_data) {
		// Wrap the array in a MemoryBuffer
		auto buffer = llvm::MemoryBuffer::getMemBuffer(
			llvm::StringRef(reinterpret_cast<const char*>(llvm_bc_data.data()), llvm_bc_data.size()),
			/*BufferName=*/"",
			/*RequiresNullTerminator=*/false  // Maybe unnecessary, but BC files may not end with null
		);

		// Parse the bitcode.
		llvm::Expected<std::unique_ptr<llvm::Module>> mod_or_err
			= llvm::parseBitcodeFile(buffer->getMemBufferRef(), getLLVMContext());

		if (!mod_or_err)
			CORE_PANIC("Error parsing bitcode: ", llvm::toString(mod_or_err.takeError()));

		// If bitcode was parsed successfully, wrap the module in ModuleImpl and return it.
		auto result
			= makeBox<ModuleImpl>(Box<llvm::Module>::fromPointer(std::move(*mod_or_err).release()));
		return result;
	}

	llvm::Function* addFunctionToModuleInternal(
		query::Context& ctx, const Ref<ModuleImpl> module, const CRef<lir::Function> lir_function
	) {
		LIRFunction2LLVM lir2llvm{ ctx, lir_function, module->module.refMut() };
		return lir2llvm.createFunction();
	}

	void addFunctionToModuleImpl(
		query::Context& ctx, const Ref<ModuleImpl> module, const CRef<lir::Function> lir_function
	) {
		addFunctionToModuleInternal(ctx, module, lir_function);
	}

	void addFunctionToModuleCtorsImpl(
		query::Context& ctx, const Ref<ModuleImpl> module, const CRef<lir::Function> lir_function
	) {
		const auto fun = addFunctionToModuleInternal(ctx, module, lir_function);
		// 65535 is the default priority for global constructors in LLVM.
		// There is also a 4-parameter Constant* Data = nullptr, which is the pointer to the
		// global variable associated with the constructor. However, the problem is that the
		// order of functions with the same priority is not defined. Therefore, we probably want
		// to create one global constructor that calls the constructor of each variable in the
		// module.
		llvm::appendToGlobalCtors(*module->module.refMut(), fun, 65'535);
	}

	void addFunctionToModuleDtorsImpl(
		query::Context& ctx, const Ref<ModuleImpl> module, const CRef<lir::Function> lir_function
	) {
		const auto fun = addFunctionToModuleInternal(ctx, module, lir_function);
		llvm::appendToGlobalDtors(*module->module.refMut(), fun, 65'535);
	}

	void addGlobalToModuleImpl(const Ref<ModuleImpl> module, const lir::LIRGlobal& lir_global) {
		addGlobalVariable(module->module.refMut(), lir_global);
	}
}
