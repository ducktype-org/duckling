#include "function_lowering_context.hpp"

#include "common.hpp"
#include "debug_info_utils.hpp"
#include "dvm_value.hpp"
#include "program_lowering_context.hpp"

#include <lir/lir_structure/lir_structure.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/builtin_types.hpp>
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
	constexpr DVMImmediate lirConstantToImmediate(
		const compiler::lir::LIRConstant& constant, const vm::code::TypeOfData& type
	) {
		variant_match(constant.value.getStorage()) {
			variant_case(compiler::numeric_value::NumericValue, numeric) {
				return std::visit(
					[&](auto&& val) -> DVMImmediate {
						return DVMImmediate{ translateToU64(val), type };
					},
					numeric.getStorage()
				);
			}
			variant_case(char, value) { return DVMImmediate{ translateToU64(value), type }; }
			variant_case(bool, value) { return DVMImmediate{ translateToU64(value), type }; }
			variant_case(compiler::tsh::SymbolType<>, type_val) {
				// @TODO: #1728 remove this evil bit_cast
				// Representation of a meta type in DVM is a pointer to the symbol type.
				return DVMImmediate{ std::bit_cast<u64>(&type_val), type };
			}
			variant_default {
				CORE_PANIC("Unsupported CompileTimeValue type for a VM constant operand");
			}
		}
		CORE_UNREACHABLE();
	}
}

DVMLocal FunctionLoweringContext::forceToLocal(
	const DVMValue& value, base::Optional<const char*> name_hint
) {
	if (value.is<DVMLocal>()) return value.get<DVMLocal>();

	DVMLocal temp = pushTempLocal(value.getType(), name_hint);
	pushInstruction({ vm::code::builders::OpKind::mov, temp.asArgument(), value.asArgument() });
	return temp;
}

void FunctionLoweringContext::storeResult(const DVMPlace& dest_place, const DVMValue& src_value) {
	// If a place is direct we just move the value into it.
	if (dest_place.isDirect()) {
		pushInstruction(
			{ vm::code::builders::OpKind::mov, dest_place.asArgument(), src_value.asArgument() }
		);
		return;
	}

	// Otherwise, we store the result in the memory pointed by the pointer.
	// If the src_value is immediate we have to store it in a temp first, as store requires a
	// place as source.
	DVMValue src_arg = [&]() -> DVMValue {
		if (src_value.is<DVMImmediate>()) {
			DVMLocal temp_local = pushTempLocal(src_value.getType(), "store_tmp");
			pushInstruction(
				{ vm::code::builders::OpKind::mov, temp_local.asArgument(), src_value.asArgument() }
			);
			return { temp_local, DVMPlace::AccessKind::Direct };
		} else {
			CORE_ASSERT(
				!(src_value.is<DVMPlace>() && !src_value.get<DVMPlace>().isDirect()),
				"Indirect DVMValue in storeResult"
			);
			return src_value;
		}
	}();

	// Store the value in memory.
	pushInstruction(
		{ vm::code::builders::OpKind::store, dest_place.asArgument(), src_arg.asAnyArgument() }
	);
}

DVMPlace FunctionLoweringContext::resolveLirPlace(const lir::LIRPlace& place) {
	// First get the base place.
	DVMPlace base_place = [&]() -> DVMPlace {
		variant_match(place.base) {
			variant_case(lir::LIRLocalRef, lir_local) {
				return { getLirLocal(lir_local), DVMPlace::AccessKind::Direct };
			}
			variant_case(lir::LIRGlobal, lir_global) {
				return { program_context.getLirGlobal(&lir_global), DVMPlace::AccessKind::Direct };
			}
		}
		CORE_UNREACHABLE();
	}();

	// If a place has no projections we access the global/local directly, not through a pointer.
	if (not place.hasProjections()) return base_place;

	CRef<tsl::TypeLayout> current_layout = place.getBaseLayout();
	DVMPlace              current_place  = base_place;
	// Go through all the projections and perform appropriate loads to get to the final destination
	// place.
	for (usize i{ 0 }; i < place.projection_chain.size(); i++) {
		const auto& projection = place.projection_chain[i];
		// Skip the last projection to not perform an unnecessary load on the last projection is a
		// dereference. DVMPlace now stores a pointer to the final place after all projections have
		// been applied.
		if (i == place.projection_chain.size() - 1) {
			current_place.setAccessKind(DVMPlace::AccessKind::Pointer);
			break;
		}

		variant_match(projection.storage) {
			variant_case(lir::LIRPlace::DerefProjection, deref) {
				// Update types after the projection has been applied.
				const auto& current_pointer_layout
					= std::get<tsl::PointerTypeLayout>(current_layout->getVariant());
				auto pointee_type
					= program_context.lowerAndKeepTslType(current_pointer_layout.getPointee());
				auto next_ptr = pushTempLocal(pointee_type, "deref_tmp_");
				pushInstruction({ vm::code::builders::OpKind::load,
				                  next_ptr.asAnyArgument(),
				                  current_place.asArgument() });
				current_place  = { next_ptr, DVMPlace::AccessKind::Pointer };
				current_layout = current_pointer_layout.getPointee();
			}
			variant_case(lir::LIRPlace::FieldProjection, field) {
				// @TODO: #1560 handle access into fields.
				throw base::NotYetImplemented("Field Projection in DVM backend");
			}
			variant_case(lir::LIRPlace::IndexProjection, index) {
				throw base::NotYetImplemented("Index Projection in DVM backend");
			}
		}
	}

	return current_place;
}

DVMValue FunctionLoweringContext::lowerLirValue(const lir::LIRValue& lir_value) {
	variant_match(lir_value.getVariant()) {
		variant_case(lir::LIRConstant, value) {
			auto dvm_type = program_context.lowerAndKeepTslType(value.layout);
			return { lirConstantToImmediate(value, dvm_type) };
		}
		variant_case(lir::LIRPlace, place) {
			DVMPlace resolved = resolveLirPlace(place);
			if (resolved.isDirect()) {
				// If the resolved place is a direct value (it's stored in a stack variable or a
				// global) we return it directly.
				return { resolved };
			} else {
				// Otherwise it's indirect. We have to load it from memory into a stack variable.
				auto val_type = program_context.lowerAndKeepTslType(place.layout);
				auto tmp      = pushTempLocal(val_type, "deref_load");
				pushInstruction(
					{ vm::code::builders::OpKind::load, tmp.asAnyArgument(), resolved.asArgument() }
				);
				// Now mark the place as direct as the value was loaded from the pointer.
				return { tmp, DVMPlace::AccessKind::Direct };
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
	function.signature.result_type = vm::code::Identifier(vm::code::typeName(function_return_type));
	function.body                  = std::move(function_body);

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
	pushInstruction({
		vm::code::builders::OpKind::init,
		dvm_local.asAnyArgument(),
		vm::opargs::Type(typeName(dvm_local.type)),
	});
}

FunctionLoweringContext::FunctionCallInfo FunctionLoweringContext::FunctionCallInfo::fromLirFunction(
	const lir::FunctionLiteral& func_literal, ProgramLoweringContext& program_context
) {
	base::Optional<vm::code::TypeOfData> called_result_type = {};
	if (func_literal.return_type_layout->getSize() != Bits{ 0 })
		called_result_type = program_context.lowerAndKeepTslType(func_literal.return_type_layout);

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

	base::Optional<vm::code::TypeOfData> called_result_type = {};
	if (ext_func.signature.result_type.str != base::StrID("void"))
		called_result_type = vm::code::getBuiltinTypeByName(ext_func.signature.result_type);

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
