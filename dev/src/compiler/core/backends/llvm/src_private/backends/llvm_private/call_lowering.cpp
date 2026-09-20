#include "call_lowering.hpp"

#include "abi_converter.hpp"
#include "type_from_layout.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/int_conv.hpp>

#include <optional>

namespace compiler::backend_llvm {

	namespace {
		constexpr u64 MAX_DIRECT_AGGREGATE_WORDS = 2;

		bool shouldPassDefaultAbiIndirectly(
			const Ref<llvm::Module> module, const CRef<tsl::TypeLayout> layout
		) {
			llvm::Type* type = typeFromLayout(module, layout);
			return type->isAggregateType()
			    && module->getDataLayout().getTypeAllocSize(type).getFixedValue()
			           > module->getDataLayout().getPointerSize() * MAX_DIRECT_AGGREGATE_WORDS;
		}

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
			// Attributes applied to the return value (e.g. signext/zeroext for sub-word ints).
			std::vector<llvm::Attribute> return_attributes;
		};

		std::optional<llvm::Attribute::AttrKind> extAttrKind(
			const abi::calling_conv::ArgInfo::ByValue& data
		) {
			if (data.sign_ext) return llvm::Attribute::SExt;
			if (data.zero_ext) return llvm::Attribute::ZExt;
			return std::nullopt;
		}

		LoweredCAbiSignature lowerCAbiSignature(
			const Ref<llvm::Module>                   module,
			const abi::calling_conv::FunctionInfo&    function_info,
			const std::vector<CRef<tsl::TypeLayout>>& parameter_layouts,
			const CRef<tsl::TypeLayout>               return_layout
		) {
			namespace cc = abi::calling_conv;
			auto& ctx    = module->getContext();

			std::vector<llvm::Type*>                     llvm_parameters;
			std::vector<std::pair<u32, llvm::Attribute>> attributes;
			std::vector<llvm::Attribute>                 return_attributes;

			// This is because the original type can have aliased structure name like
			// "%MyStructure", where ABI type is a structure type literal like "{i32, i32}" and in
			// LLVM those types are not compatible, even though they are the same.
			// So if the coerced type is the same as original, we also use original LLVM type.
			auto by_value_type = [&](const cc::ArgInfo::ByValue& data,
			                         const CRef<tsl::TypeLayout> layout,
			                         abi::types::AbiTypeCRef     original_type) -> llvm::Type* {
				if (data.coerce_to_type == *original_type) return typeFromLayout(module, layout);
				return abiTypeToLLVMType(ctx, data.coerce_to_type);
			};

			auto&       return_cc   = function_info.return_info;
			llvm::Type* return_type = nullptr;

			if (return_cc && return_cc->passed_as_param) {
				CORE_ASSERT(
					return_cc->info.getKind<cc::ArgInfo::ByPointer>().has_value(),
					"When passing as param expected calling conv info is ByPointer"
				);
				// The result is returned indirectly: prepend a hidden `sret` pointer parameter and
				// make the function return void.
				llvm::Type* original_type = abiTypeToLLVMType(ctx, *return_cc->original_type);
				attributes.emplace_back(
					u32(llvm_parameters.size()),
					llvm::Attribute::getWithStructRetType(ctx, original_type)
				);
				llvm_parameters.push_back(llvm::PointerType::getUnqual(ctx));
				return_type = llvm::Type::getVoidTy(ctx);
			} else if (return_cc) {
				variant_match(return_cc->info.kind) {
					variant_case(cc::ArgInfo::ByValue, data) {
						return_type = by_value_type(data, return_layout, return_cc->original_type);
						if (auto ext = extAttrKind(data))
							return_attributes.push_back(llvm::Attribute::get(ctx, *ext));
					}
					variant_case(cc::ArgInfo::ByPointer, data) {
						return_type = llvm::PointerType::getUnqual(ctx);
					}
				}
			} else {
				return_type = llvm::Type::getVoidTy(ctx);
			}

			const usize declared_params = function_info.num_fixed_params.has_value()
			                                ? usize(function_info.num_fixed_params.value())
			                                : function_info.param_info.size();

			for (usize i = 0; i < declared_params; i++) {
				const auto& param = function_info.param_info.at(i);
				variant_match(param.info.kind) {
					variant_case(cc::ArgInfo::ByValue, data) {
						if (auto ext = extAttrKind(data))
							attributes.emplace_back(
								u32(llvm_parameters.size()), llvm::Attribute::get(ctx, *ext)
							);
						llvm_parameters.push_back(
							by_value_type(data, parameter_layouts.at(i), param.original_type)
						);
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
				.type = llvm::FunctionType::get(
					return_type, llvm_parameters, function_info.num_fixed_params.has_value()
				),
				.attributes        = std::move(attributes),
				.return_attributes = std::move(return_attributes)
			};
		}
	}

	LoweredDefaultAbiSignature lowerDefaultAbiSignature(
		const Ref<llvm::Module>                   module,
		const std::vector<CRef<tsl::TypeLayout>>& parameters,
		const CRef<tsl::TypeLayout>               return_type
	) {
		auto& ctx = module->getContext();

		std::vector<llvm::Type*>                     llvm_parameters;
		std::vector<bool>                            parameter_is_indirect;
		std::vector<std::pair<u32, llvm::Attribute>> parameter_attributes;

		const bool  return_indirect  = shouldPassDefaultAbiIndirectly(module, return_type);
		llvm::Type* llvm_return_type = typeFromLayout(module, return_type);

		if (return_indirect) {
			parameter_attributes.emplace_back(
				0, llvm::Attribute::getWithStructRetType(ctx, llvm_return_type)
			);
			llvm_parameters.push_back(llvm::PointerType::getUnqual(ctx));
			llvm_return_type = llvm::Type::getVoidTy(ctx);
		}

		parameter_is_indirect.reserve(parameters.size());
		llvm_parameters.reserve(llvm_parameters.size() + parameters.size());

		for (const auto& parameter: parameters) {
			llvm::Type* parameter_type = typeFromLayout(module, parameter);
			const bool  indirect       = shouldPassDefaultAbiIndirectly(module, parameter);

			parameter_is_indirect.push_back(indirect);
			if (indirect) {
				const auto llvm_index = base::safeIntConv<u32>(llvm_parameters.size());
				parameter_attributes.emplace_back(
					llvm_index, llvm::Attribute::getWithByValType(ctx, parameter_type)
				);
				llvm_parameters.push_back(llvm::PointerType::getUnqual(ctx));
			} else {
				llvm_parameters.push_back(parameter_type);
			}
		}

		return LoweredDefaultAbiSignature{
			.type            = llvm::FunctionType::get(llvm_return_type, llvm_parameters, false),
			.return_indirect = return_indirect,
			.parameter_is_indirect = std::move(parameter_is_indirect),
			.attributes            = std::move(parameter_attributes)
		};
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
		std::vector<llvm::Attribute>                 return_attributes;

		llvm::FunctionType* fun_type = nullptr;
		if (auto c_abi = std::get_if<lir::LIRAbi::CAbi>(&function_literal.abi.value)) {
			auto lowered = lowerCAbiSignature(
				module,
				c_abi->function_info,
				*function_literal.parameter_layouts,
				function_literal.return_type_layout
			);
			fun_type          = lowered.type;
			attributes        = std::move(lowered.attributes);
			return_attributes = std::move(lowered.return_attributes);
		} else {
			auto lowered = lowerDefaultAbiSignature(
				module, *function_literal.parameter_layouts, function_literal.return_type_layout
			);
			fun_type   = lowered.type;
			attributes = std::move(lowered.attributes);
		}

		llvm::FunctionCallee callee = module->getOrInsertFunction(mangled_name.strView(), fun_type);

		if (auto* function = llvm::dyn_cast<llvm::Function>(callee.getCallee())) {
			function->setCallingConv(getCallingConvFromABI(function_literal.abi));
			if (function_literal.link_once)
				function->setLinkage(llvm::GlobalValue::LinkOnceODRLinkage);

			for (const auto& [idx, attr]: attributes) function->addParamAttr(idx, attr);
			for (const auto& attr: return_attributes) function->addRetAttr(attr);
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
