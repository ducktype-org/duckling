#include "call_lowering.hpp"

#include "abi_converter.hpp"
#include "type_from_layout.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/int_conv.hpp>

namespace compiler::backend_llvm {

	namespace {
		/**
		 * @brief The result of lowering a C-ABI function signature: the LLVM function type together
		 * with the call-site/prototype parameter attributes (sret / byval), keyed by the LLVM
		 * argument index.
		 *
		 * @note This must stay in lockstep with the call-site marshalling done in
		 * `lowerCallCAbiInstruction` (llvm_lowering.cpp): the same `FunctionInfo` drives both, so
		 * an sret pointer is prepended when the return is passed as a parameter, `ByValue`
		 * arguments/returns use their coerced type, and `ByPointer` arguments are passed as
		 * pointers (with `byval` when requested).
		 */
		struct LoweredCAbiSignature {
			llvm::FunctionType*                          type;
			std::vector<std::pair<u32, llvm::Attribute>> attributes;
		};

		LoweredCAbiSignature lowerCAbiSignature(
			const Ref<llvm::Module> module, const abi::calling_conv::FunctionInfo& function_info
		) {
			namespace cc = abi::calling_conv;
			auto& ctx    = module->getContext();

			std::vector<llvm::Type*>                     llvm_parameters;
			std::vector<std::pair<u32, llvm::Attribute>> attributes;

			auto&       return_cc   = function_info.return_info;
			llvm::Type* return_type = nullptr;

			if (return_cc.passed_as_param) {
				CORE_ASSERT(
					std::holds_alternative<cc::ArgInfo::ByPointer>(return_cc.info.kind),
					"When passing as param expected calling conv info is ByPointer"
				);
				// The result is returned indirectly: prepend a hidden `sret` pointer parameter and
				// make the function return void.
				llvm::Type* original_type = abiTypeToLLVMType(ctx, *return_cc.original_type);
				attributes.emplace_back(
					u32(llvm_parameters.size()),
					llvm::Attribute::getWithStructRetType(ctx, original_type)
				);
				llvm_parameters.push_back(llvm::PointerType::getUnqual(ctx));
				return_type = llvm::Type::getVoidTy(ctx);
			} else {
				variant_match(return_cc.info.kind) {
					variant_case(cc::ArgInfo::ByValue, data) {
						return_type = abiTypeToLLVMType(ctx, data.coerce_to_type);
					}
					variant_case(cc::ArgInfo::ByPointer, data) {
						return_type = llvm::PointerType::getUnqual(ctx);
					}
				}
			}

			for (const auto& param: function_info.param_info) {
				variant_match(param.info.kind) {
					variant_case(cc::ArgInfo::ByValue, data) {
						llvm_parameters.push_back(abiTypeToLLVMType(ctx, data.coerce_to_type));
					}
					variant_case(cc::ArgInfo::ByPointer, data) {
						if (data.by_val) {
							attributes.emplace_back(
								u32(llvm_parameters.size()),
								llvm::Attribute::getWithByValType(
									ctx, abiTypeToLLVMType(ctx, *param.original_type)
								)
							);
						}
						llvm_parameters.push_back(llvm::PointerType::getUnqual(ctx));
					}
				}
			}

			return LoweredCAbiSignature{
				.type       = llvm::FunctionType::get(return_type, llvm_parameters, false),
				.attributes = std::move(attributes)
			};
		}
	}

	llvm::FunctionType* getFunType(
		const Ref<llvm::Module>                   module,
		const std::vector<CRef<tsl::TypeLayout>>& parameters,
		const CRef<tsl::TypeLayout>               return_type,
		const lir::LIRAbi&                        abi
	) {
		// The C ABI signature is fully driven by the calling-convention library.
		if (const auto c_abi = std::get_if<lir::LIRAbi::CAbi>(&abi.value))
			return lowerCAbiSignature(module, c_abi->function_info).type;

		std::vector<llvm::Type*> llvm_parameters;
		llvm_parameters.reserve(parameters.size());

		// Prepare parameter types.
		for (const auto& param: parameters)
			llvm_parameters.push_back(typeFromLayout(module, param));

		// Prepare function type, including return type.
		return llvm::FunctionType::get(typeFromLayout(module, return_type), llvm_parameters, false);
	}

	llvm::CallingConv::ID getCallingConvFromABI(const lir::LIRAbi& abi) {
		variant_match(abi.value) {
			variant_case(lir::LIRAbi::DefaultAbi, default_abi) { return llvm::CallingConv::C; }
			variant_case(lir::LIRAbi::CAbi, c_abi) { return llvm::CallingConv::C; }
		}
		CORE_UNREACHABLE();
	}

	llvm::FunctionCallee getOrInsertFunctionPrototypeFromLiteral(
		const Ref<llvm::Module> module, const lir::FunctionLiteral& function_literal
	) {
		const auto mangled_name = function_literal.mangled_name;
		// We check if function exist first, to avoid unnecessary construction of types:
		if (const auto func = module->getFunction(mangled_name.strView())) return func;

		std::vector<std::pair<u32, llvm::Attribute>> attributes;

		llvm::FunctionType* fun_type = nullptr;
		if (auto c_abi = std::get_if<lir::LIRAbi::CAbi>(&function_literal.abi.value)) {
			auto lowered = lowerCAbiSignature(module, c_abi->function_info);
			fun_type     = lowered.type;
			attributes   = std::move(lowered.attributes);
		} else {
			fun_type = getFunType(
				module,
				*function_literal.parameter_layouts,
				function_literal.return_type_layout,
				function_literal.abi
			);
		}

		llvm::FunctionCallee callee = module->getOrInsertFunction(mangled_name.strView(), fun_type);

		if (auto* function = llvm::dyn_cast<llvm::Function>(callee.getCallee())) {
			function->setCallingConv(getCallingConvFromABI(function_literal.abi));
			if (function_literal.link_once)
				function->setLinkage(llvm::GlobalValue::LinkOnceODRLinkage);

			for (const auto& [idx, attr]: attributes) function->addParamAttr(idx, attr);
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
}
