#include "function_lowering_context.hpp"

#include "dvm_value.hpp"
#include "program_lowering_context.hpp"

#include <lir/lir_structure/lir_structure.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>


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
	ProgramLoweringContext&                   program_context,
	base::StrID                               name,
	CRef<tsl::TypeLayout>                     return_type,
	const std::vector<CRef<tsl::TypeLayout>>& parameter_types
):
	  program_context(program_context),
	  function_return_type(program_context.lowerAndKeepTslType(return_type)),
	  function_parameter_types(
		  parameter_types | std::views::transform([&](auto&& layout) {
			  return program_context.lowerAndKeepTslType(layout);
		  })
		  | std::ranges::to<std::vector>()
	  ),
	  function_name(name) {}

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
					[&](auto&& val) -> DVMImmediate {
						using T      = std::decay_t<decltype(val)>;
						u64 arg_bits = 0;

						if constexpr (std::is_integral_v<T>) {
							arg_bits = static_cast<u64>(val);
						} else if (std::is_floating_point_v<T>) {
							f64 val_as_64 = static_cast<f64>(val);
							arg_bits      = std::bit_cast<u64>(val_as_64);
						} else {
							CORE_PANIC("Unsupported NumericValue type for a VM constant operand");
						}
						return DVMImmediate{ arg_bits };
					},
					numeric.getStorage()
				);
			}
			variant_case(bool, value) { return DVMImmediate{ value }; }
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
			return { DVMFunctionName{ function.mangled_name } };
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

	// @TODO: #1659 - When main will be able to accept no parameters, then the first if branch
	// should be removed.
	if (function_name == "main") {
		function.signature.parameters.emplace_back(base::StrID("i64"));
		function.signature.parameters.emplace_back(base::StrID("ptr_argv"));
		function.signature.result_type = vm::code::Identifier(base::StrID("i64"));
	} else {
		for (const auto& param_type: function_parameter_types)
			function.signature.parameters.emplace_back(vm::code::typeName(param_type));
		function.signature.result_type
			= vm::code::Identifier(vm::code::typeName(function_return_type));
	}
	function.body = std::move(function_body);
	return function;
}

DVMLocal FunctionLoweringContext::getFunctionReturnValueLocal() {
	// @TODO: #1659 - When main will be able to accept no parameters, then the first if branch
	// should be removed.
	if (function_name == "main") {
		if (function_return_type
		    != vm::code::TypeOfData{ vm::code::PrimitiveType(base::StrID("i64"), 8) }) {
			CORE_PANIC("Main function must have i64 return type");
		}
		// return DVMLocal{
		// 	.name = base::StrID("ret_val"),
		// 	.type = vm::code::PrimitiveType(base::StrID("i64"), 8),
		// };
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
	auto dvm_local = insertLirLocal(lir_local);
	pushInstruction({
		vm::code::builders::OpKind::init,
		vm::opargs::StackLocalAny(dvm_local.name),
		vm::opargs::Type(typeName(dvm_local.type)),
	});
}
