#include "function_lowering_context.hpp"

#include "debug_info_utils.hpp"
#include "dvm_value.hpp"
#include "program_lowering_context.hpp"

#include <lir/lir_structure/lir_structure.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>

#include <ranges>

using namespace compiler::backend_vm::internal;

#define INVALID_CASE(tp, reason)                                                    \
	variant_case(tp, _) {                                                           \
		CORE_PANIC("During handling of type ", base::typeName<tp>(), ": ", reason); \
	}

#define NOIMPL_CASE(tp, reason)                                              \
	variant_case(tp, _) {                                                    \
		throw base::NotYetImplemented(                                       \
			base::strConcat("Type ", base::typeName<tp>(), " for: ", reason) \
		);                                                                   \
	}

FunctionLoweringContext::FunctionLoweringContext(
	ProgramLoweringContext&                     program_context,
	base::StrID                                 name,
	CRef<tsl::TypeLayout>                       return_type,
	const std::vector<CRef<tsl::TypeLayout>>&   parameter_types,
	base::Optional<debug_info::FunctionBuilder> fun_di_builder_opt
):
	  program_context(program_context),
	  function_return_type(program_context.lowerAndKeepTslType(return_type)),
	  function_parameter_types(
		  parameter_types | std::views::transform([&](auto&& layout) {
			  return program_context.lowerAndKeepTslType(layout);
		  })
		  | std::ranges::to<std::vector>()
	  ),
	  function_name(name),
	  fun_di_builder_opt(std::move(fun_di_builder_opt)) {}

base::StrID FunctionLoweringContext::getBlockLabel(lir::BlockRef block) {
	if (!block_to_label.contains(block)) {
		auto label_name = base::strConcat("label_", block_to_label.size());
		block_to_label.put(block, base::StrID(label_name.data()));
	}

	return block_to_label.at(block);
}

namespace {
	constexpr DVMImmediate lirConstantToImmediate(const compiler::lir::LIRConstant& constant) {
		variant_match(constant.value.getStorage()) {
			variant_case(compiler::numeric_value::NumericValue, numeric) {
				return std::visit(
					[&](auto&& val) -> DVMImmediate { return DVMImmediate{ val }; },
					numeric.getStorage()
				);
			}
			variant_case(char, value) { return DVMImmediate{ value }; }
			variant_case(bool, value) { return DVMImmediate{ value }; }
			variant_case(compiler::tsh::SymbolType<>, type_val) {
				// @TODO: #1728 remove this evil bit_cast
				// Representation of a meta type in DVM is a pointer to the symbol type.
				return DVMImmediate{ std::bit_cast<u64>(&type_val) };
			}
			variant_default {
				CORE_PANIC("Unsupported CompileTimeValue type for a VM constant operand");
			}
		}
		CORE_UNREACHABLE();
	}
}

DVMValue FunctionLoweringContext::lowerLirValue(const lir::LIRValue& lir_value) {
	variant_match(lir_value.getVariant()) {
		variant_case(lir::LIRConstant, value) { return { lirConstantToImmediate(value) }; }
		variant_case(lir::LIRPlace, place) {
			// @TODO: #1560 handle access into fields.
			variant_match(place.base) {
				variant_case(lir::LIRLocalRef, local_ref) { return { getLirLocal(local_ref) }; }
				variant_case(lir::LIRGlobal, global) {
					return { program_context.getLirGlobal(&global) };
				}
			}
		}
		variant_case(lir::BlockRef, block_ref) { return { DVMLabel{ getBlockLabel(block_ref) } }; }
		variant_case(lir::FunctionLiteral, function) {
			return { DVMFunctionName{ .name = function.mangled_name } };
		}
		variant_default { CORE_PANIC("Unhandled value case"); }
	}
	CORE_UNREACHABLE();
}

const DVMLocal& FunctionLoweringContext::insertLirLocal(lir::LIRLocalRef local) {
	if (!lir_local_to_dvm.contains(local)) auto new_local = createLirLocalToDVMMapping(local);
	return lir_local_to_dvm.at(local);
}

const DVMLocal& FunctionLoweringContext::createLirLocalToDVMMapping(lir::LIRLocalRef lir_local) {
	auto var_name = [&] {
		match_optional(lir_local->parameter_index) {
			opt_some(index) return base::strConcat("arg", index);
			opt_none return base::strConcat("var", lir_local_to_dvm.size());
		}
		CORE_UNREACHABLE();
	}();

	auto var_name_str_id = base::StrID(var_name.data());

	auto var_type = program_context.lowerAndKeepTslType(lir_local->layout);
	lir_local_to_dvm.put(lir_local, DVMLocal{ .name = var_name_str_id, .type = var_type });
	return lir_local_to_dvm.at(lir_local);
}

void compiler::backend_vm::internal::FunctionLoweringContext::registerFunctionParameter(
	lir::LIRLocalRef lir_func_param
) {
	createLirLocalToDVMMapping(lir_func_param);
	if_opt_some(fun_di_builder_opt, builder) {
		if_opt_some(lir_func_param->metadata.source_code_name, param_name) {
			builder.addParameter(
				lir_func_param->parameter_index.value(),
				param_name.str(),
				lir_func_param->metadata.position.map(mapDIPosition)
			);
		}
	}
}

void compiler::backend_vm::internal::FunctionLoweringContext::beginBlock(lir::BlockRef block) {
	pushInstruction(vm::code::instructions::Op_label(getBlockLabel(block)));
}

void compiler::backend_vm::internal::FunctionLoweringContext::pushInstruction(
	const vm::code::Instruction& instruction
) {
	function_body.push_back(instruction);
}

void compiler::backend_vm::internal::FunctionLoweringContext::pushInstruction(
	const vm::code::builders::InstructionBuilder& instruction
) {
	pushInstruction(instruction.build());
}

vm::code::Function compiler::backend_vm::internal::FunctionLoweringContext::finish() && {
	vm::code::Function function;
	function.name = function_name;
	for (const auto& param_type: function_parameter_types)
		function.signature.parameters.emplace_back(vm::code::typeName(param_type));
	function.signature.result_types = {};
	if (auto type_name = vm::code::typeName(function_return_type); type_name != "void")
		function.signature.result_types.emplace_back(type_name);
	function.body = std::move(function_body);

	if_opt_some(fun_di_builder_opt, builder) { builder.end(); }

	return function;
}

DVMLocal FunctionLoweringContext::getFunctionReturnValueLocal() {
	if (function_name == "main") {
		CORE_ASSERT(
			function_return_type
				== vm::code::TypeOfData{ vm::code::PrimitiveType(base::StrID("i64"), 8) },
			"Main function must have i64 return type"
		);
	}
	return DVMLocal{
		.name = base::StrID("ret_val"),
		.type = function_return_type,
	};
}

[[nodiscard]] const compiler::backend_vm::internal::DVMLocal& compiler::backend_vm::internal::
	FunctionLoweringContext::getLirLocal(lir::LIRLocalRef local) const {
	CORE_ASSERT(lir_local_to_dvm.contains(local), "LIR local not found");
	return lir_local_to_dvm.at(local);
}

void compiler::backend_vm::internal::FunctionLoweringContext::pushInit(lir::LIRLocalRef lir_local) {
	if_opt_some(fun_di_builder_opt, builder) {
		if_opt_some(lir_local->metadata.source_code_name, var_name) {
			builder.addVariableInit(
				instructionsCount(), var_name.str(), lir_local->metadata.position.map(mapDIPosition)
			);
		}
	}

	auto dvm_local = insertLirLocal(lir_local);
	pushInstruction(
		{
			vm::code::builders::OpKind::init,
			vm::opargs::StackLocalAny(dvm_local.name),
			vm::opargs::Type(typeName(dvm_local.type)),
		}
	);
}

FunctionLoweringContext::FunctionCallInfo FunctionLoweringContext::FunctionCallInfo::fromLirFunction(
	const lir::FunctionLiteral& func_literal, ProgramLoweringContext& program_context
) {
	std::vector<vm::code::TypeOfData> called_result_type = {};
	if (!func_literal.return_type_layout->is<tsl::EmptyTypeLayout>())
		called_result_type.emplace_back(
			program_context.lowerAndKeepTslType(func_literal.return_type_layout)
		);

	std::vector<vm::code::TypeOfData> param_types
		= *func_literal.parameter_layouts | std::views::transform([&](const auto& layout) {
			  return program_context.lowerAndKeepTslType(layout);
		  })
	    | std::ranges::to<std::vector>();

	return FunctionCallInfo{
		.call_target = DVMFunctionName{ .name = func_literal.mangled_name },
		.return_type = called_result_type,
		.param_types = param_types,
		.is_extern_c = false,
	};
}

FunctionLoweringContext::FunctionCallInfo FunctionLoweringContext::FunctionCallInfo::fromExternCFunction(
	const base::StrID& ext_func_name, ProgramLoweringContext& program_context
) {
	const auto& ext_func = program_context.getExternCFunction(ext_func_name);

	std::vector<vm::code::TypeOfData> called_result_type
		= ext_func.signature.result_types | std::views::transform([&](const auto& reslt) {
			  return vm::code::getBuiltinTypeByName(reslt).value();
		  })
	    | std::ranges::to<std::vector>();

	std::vector<vm::code::TypeOfData> param_types
		= ext_func.signature.parameters | std::views::transform([&](const auto& type_name) {
			  return vm::code::getBuiltinTypeByName(type_name).value();
		  })
	    | std::ranges::to<std::vector>();

	return FunctionCallInfo{
		.call_target = DVMExternCFunctionName{ .name = ext_func_name },
		.return_type = called_result_type,
		.param_types = param_types,
		.is_extern_c = true,
	};
}

usize compiler::backend_vm::internal::FunctionLoweringContext::instructionsCount() const {
	return function_body.size();
}
