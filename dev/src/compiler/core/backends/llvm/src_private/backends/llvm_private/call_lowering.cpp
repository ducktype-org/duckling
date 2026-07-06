#include "call_lowering.hpp"

#include "type_from_layout.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/int_conv.hpp>

namespace compiler::backend_llvm {

	llvm::FunctionType* getFunType(
		const Ref<llvm::Module>                   module,
		const std::vector<CRef<tsl::TypeLayout>>& parameters,
		const CRef<tsl::TypeLayout>               return_type,
		const lir::LIRAbi&                        abi
	) {
		const bool is_c_abi = std::holds_alternative<lir::LIRAbi::CAbi>(abi.value);

		std::vector<llvm::Type*> llvm_parameters;
		llvm_parameters.reserve(parameters.size());

		// Prepare parameter types.
		for (const auto& param: parameters)
			if (is_c_abi and param->is<tsl::StringTypeLayout>())
				llvm_parameters.push_back(llvm::PointerType::getUnqual(module->getContext()));
			else
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
			if (function_literal.link_once)
				function->setLinkage(llvm::GlobalValue::LinkOnceODRLinkage);

			// If the function uses C ABI, we need to pass structs by pointer with `byval` attribute.
			if (std::holds_alternative<lir::LIRAbi::CAbi>(function_literal.abi.value)) {
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
}
