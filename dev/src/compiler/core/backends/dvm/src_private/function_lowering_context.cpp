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
	const DVMValue& value, base::Optional<std::string_view> name_hint
) {
	if (value.is<DVMLocal>()) return value.get<DVMLocal>();

	DVMLocal temp = pushTempLocal(value.getType(), name_hint);
	pushInstruction({ vm::code::builders::OpKind::mov, temp.asArgument(), value.asArgument() });
	return temp;
}

void FunctionLoweringContext::maybeStoreResult(
	const base::Optional<DVMPlace>& maybe_dest_place, const DVMValue& src_value
) {
	// Do nothing, if the dest_place is empty.
	if_opt_none(maybe_dest_place) return;
	const DVMPlace& dest_place = maybe_dest_place.value();

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
	DVMPlace current_place = [&]() -> DVMPlace {
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
	if (not place.hasProjections()) return current_place;

	// If a place is global, we load a pointer to it to a local.
	if (place.hasProjections() && current_place.is<DVMGlobal>()) {
		auto                        global = current_place.get<DVMGlobal>();
		const vm::code::TypeOfData& ptr_to_global_type
			= program_context.getOrInsertPointerType(global.type);

		DVMLocal addr_tmp = pushTempLocal(ptr_to_global_type, "global_addr_ref");
		pushInstruction({ vm::code::builders::OpKind::ref, addr_tmp, current_place.asAnyArgument() }
		);
		current_place = { addr_tmp, DVMPlace::AccessKind::Pointer };
	}

	CRef<tsl::TypeLayout> current_layout = place.getBaseLayout();
	// Go through all the projections and perform appropriate loads to get to the final destination
	// place.
	for (usize i{ 0 }; i < place.projection_chain.size(); i++) {
		const auto& projection = place.projection_chain[i];
		variant_match(projection.storage) {
			variant_case(lir::LIRPlace::DerefProjection, deref) {
				CORE_ASSERT(
					current_layout->is<tsl::PointerTypeLayout>(),
					"DerefProjection performed on a non pointer layout"
				);
				const auto& current_pointer_layout
					= std::get<tsl::PointerTypeLayout>(current_layout->getVariant());
				auto pointee_layout = current_pointer_layout.getPointee();


				if (current_place.isDirect()) {
					// In this case we have a direct stack variable which stores a pointer.
					// Dereferencing means we now treat the local as a pointer.
					current_place.setAccessKind(DVMPlace::AccessKind::Pointer);
				} else {
					vm::code::TypeOfData vm_loaded_type
						= program_context.lowerAndKeepTslType(current_layout);

					DVMLocal loaded_val_tmp = pushTempLocal(vm_loaded_type, "deref_tmp");

					// Emit the load instruction.
					pushInstruction({ vm::code::builders::OpKind::load,
					                  loaded_val_tmp.asAnyArgument(),
					                  current_place });

					// Update types after the projection has been applied.
					current_place = { loaded_val_tmp, DVMPlace::AccessKind::Pointer };
				}

				current_layout = pointee_layout;
			}
			variant_case(lir::LIRPlace::FieldProjection, field) {
				CORE_ASSERT(
					current_layout->is<tsl::ClassTypeLayout>(), "FieldProjection on non-class layout"
				);
				// @TODO: #2100 Change that to use indexes.

				// Prepare the class type.
				const auto& class_layout
					= std::get<tsl::ClassTypeLayout>(current_layout->getVariant());
				const vm::code::TypeOfData& vm_class_type
					= program_context.lowerAndKeepTslType(current_layout);

				// Prepare the pointer to field type.
				const usize field_index
					= class_layout.getLayoutIndexOfFieldSymbol(field.field_id).value();
				CRef<tsl::TypeLayout> field_layout
					= class_layout.getFieldLayoutOfLayoutIndex(field_index);
				const vm::code::TypeOfData vm_field_type
					= program_context.lowerAndKeepTslType(field_layout);

				auto vm_field_name = base::strConcat("_", field_index);

				const vm::code::TypeOfData& ptr_to_field_type
					= program_context.getOrInsertPointerType(vm_field_type);

				// Create a temporary to the field
				DVMLocal field_ptr_tmp = pushTempLocal(ptr_to_field_type, "field_addr");

				// Emit the pointer move instruction. Based on the `current_place` type,
				// `structLea_pptr_pptr_field` or `structLea_pptr_pste_field` will be picked.
				pushInstruction({ vm::code::builders::OpKind::structLea,
				                  field_ptr_tmp,
				                  current_place,
				                  vm::opargs::Field{ typeName(vm_class_type),
				                                     base::StrID(vm_field_name) } });

				// `field_ptr_tmp` now holds a pointer to the appropriate struct field.
				current_place  = { field_ptr_tmp, DVMPlace::AccessKind::Pointer };
				current_layout = field_layout;
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
				const vm::code::TypeOfData& val_type
					= program_context.lowerAndKeepTslType(place.layout);
				DVMLocal tmp = pushTempLocal(val_type, "deref_load");
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
	function.signature.result_types = {};
	// @TODO: #2499 Make lowerAndKeepTslType return an optional and remove the void type from here
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
		.name = base::StrID("ret0"),
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

usize compiler::backend_vm::internal::FunctionLoweringContext::instructionsCount() const {
	return function_body.size();
}
