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
#include <vm/bytecode/type_of_data.hpp>

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
	  function_return_type(program_context.lowerAndKeepTslType(return_type)
                               .map([](CRef<vm::code::TypeOfData> ref) { return *ref; })),
	  function_parameter_types(
		  parameter_types | std::views::transform([&](auto&& layout) {
			  return **program_context.lowerAndKeepTslType(layout);
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
	/**
	 * @brief Translate a compile time value in LIR constant to a DVM immediate.
	 * @param constant the LIR constant to translate
	 * @param type the DVM type of the resulting immediate
	 * @param is_comp_time_lowering if true, pointers to compile time values (e.g. symbol types)
	 * will be kept as is in the generated code, otherwise they will be replaced with zero value.
	 * This is useful to have the deterministic codegen (pointers to internal memory is not
	 * deterministic between runs).
	 */
	constexpr DVMImmediate lirConstantToImmediate(
		const compiler::lir::LIRConstant& constant,
		const vm::code::TypeOfData&       type,
		bool                              is_comp_time_lowering = true
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
				// @TODO: #1709 RTTI when the is_comp_time_lowering == false
				u64 type_val_u64 = is_comp_time_lowering ? std::bit_cast<u64>(&type_val) : 0;
				return DVMImmediate{ type_val_u64, type };
			}
			variant_default {
				CORE_PANIC("Unsupported CompileTimeValue type for a VM constant operand");
			}
		}
		CORE_UNREACHABLE();
	}
}

vm::code::Function compiler::backend_vm::internal::createMiniGlobalCtorFromCTV(
	ProgramLoweringContext&      program_context,
	CRef<tsl::TypeLayout>        global_layout,
	const vm::code::TypeOfData&  lowered_global_type,
	const ctv::CompileTimeValue& global_ctv_value,
	base::StrID                  mini_ctor_name,
	const DVMPlace&              dvm_global
) {
	const lir::LIRConstant global_lir_constant{
		.value  = global_ctv_value,
		.layout = global_layout,
	};

	const auto immediate = lirConstantToImmediate(
		global_lir_constant, lowered_global_type, program_context.isCompTimeLowering()
	);

	vm::code::Function mini_ctor;
	mini_ctor.name                   = vm::code::Identifier(mini_ctor_name);
	mini_ctor.signature.result_types = {};
	mini_ctor.body.push_back(
		vm::code::builders::InstructionBuilder(
			vm::code::builders::OpKind::mov, dvm_global.asArgument(), immediate.asArgument()
		)
			.build()
	);
	mini_ctor.body.push_back(
		vm::code::builders::InstructionBuilder(vm::code::builders::OpKind::ret).build()
	);

	return mini_ctor;
}

DVMPlace FunctionLoweringContext::forceToPlace(
	const DVMValue& value, base::Optional<std::string_view> name_hint
) {
	// If the value already is a place, we don't touch it.
	if (value.is<DVMPlace>()) return value.get<DVMPlace>();

	if (value.is<DVMImmediate>()) {
		DVMPlace temp = pushTempLocal(value.getType(), name_hint);
		pushInstruction({
			vm::code::builders::OpKind::mov,
			temp.asArgument(),
			value.asArgument(),
		});
		return temp;
	}
	CORE_UNREACHABLE();
}

void FunctionLoweringContext::maybeStoreResult(
	const base::Optional<DVMPlace>& maybe_dest_place, const DVMValue& src_value
) {
	// Do nothing, if the dest_place is empty.
	if_opt_none(maybe_dest_place) return;
	const DVMPlace& dest_place = maybe_dest_place.value();

	// If the source value is already in the destination place, we don't need to do anything.
	if (DVMValue{ dest_place } == src_value) return;


	// If a place is direct we just move the value into it.
	if (dest_place.isDirect()) {
		pushInstruction(
			{ vm::code::builders::OpKind::mov, dest_place.asArgument(), src_value.asArgument() }
		);
		return;
	} else {
		// Otherwise, we store the result in the memory pointed by the pointer.
		// If the src_value is immediate we have to store it in a temp first, as store requires a
		// place as source.
		DVMValue src_arg = src_value.is<DVMImmediate>()
		                     ? DVMValue{ forceToPlace(src_value, "store_tmp") }
		                     : src_value;

		switch (dest_place.getAccessKind()) {
		case DVMPlace::AccessKind::Pointer:
			// Store the value in memory.
			pushInstruction({ vm::code::builders::OpKind::store,
			                  dest_place.asArgument(),
			                  src_arg.asAnyArgument() });
			break;
		case DVMPlace::AccessKind::DynTablePointer: {
			// For dynamic table pointer we need to use the extended store with index 0, as the
			// pointer points to the first element of the table.
			auto     u64_zero = DVMImmediate::u64(0);
			DVMPlace idx_temp = pushTempLocal(u64_zero.type, "index_tmp");
			pushInstruction({ vm::code::builders::OpKind::mov, idx_temp, u64_zero });
			pushInstruction({ vm::code::builders::OpKind::dynTableStore,
			                  dest_place.asArgument(),
			                  src_arg.asAnyArgument(),
			                  idx_temp });
			break;
		}
		case DVMPlace::AccessKind::CPointer:
			// @TODO: #2745 fix this
			CORE_PANIC("Storing to CPointer is not supported yet");
			break;
		default:
			break;
		}
	}
}

DVMPlace FunctionLoweringContext::loadFromPlace(
	const DVMPlace& place, const vm::code::TypeOfData& pointee_type
) {
	DVMPlace temp = pushTempLocal(pointee_type, "deref_tmp");
	switch (place.getAccessKind()) {
	case DVMPlace::AccessKind::Pointer:
		pushInstruction({ vm::code::builders::OpKind::load, temp.asAnyArgument(), place });
		break;
	case DVMPlace::AccessKind::DynTablePointer: {
		auto     u64_value = DVMImmediate::u64(0);
		DVMPlace idx_temp  = pushTempLocal(u64_value.type, "index_tmp");
		pushInstruction({ vm::code::builders::OpKind::mov, idx_temp, u64_value });
		pushInstruction(
			{ vm::code::builders::OpKind::dynTableLoad, temp.asAnyArgument(), place, idx_temp }
		);
		break;
	}
	case DVMPlace::AccessKind::CPointer:
		// @TODO: #2745 fix this
		CORE_PANIC("Dereferencing CPointer is not supported yet");
	default:
		CORE_PANIC("loadFromPlace called on a place with non-pointer access kind");
	}
	return temp;
}

DVMPlace FunctionLoweringContext::resolveLirPlace(const lir::LIRPlace& place) {
	// First get the base place.
	DVMPlace current_place = [&]() -> DVMPlace {
		variant_match(place.base) {
			variant_case(lir::LIRLocalRef, lir_local) { return getLirLocal(lir_local); }
			variant_case(lir::LIRGlobal, lir_global) {
				return program_context.getLirGlobal(&lir_global);
			}
		}
		CORE_UNREACHABLE();
	}();

	// If a place has no projections we access the global/local directly, not through a pointer.
	if (not place.hasProjections()) return current_place;

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

				auto access_for_layout = [](const tsl::PointerTypeLayout& pointer_layout) {
					switch (pointer_layout.getPointerKind()) {
					case tsl::PointerTypeLayout::PointerKind::SinglePointer:
						return DVMPlace::AccessKind::Pointer;
					case tsl::PointerTypeLayout::PointerKind::ManyPointer:
						return DVMPlace::AccessKind::DynTablePointer;
					case tsl::PointerTypeLayout::PointerKind::CPointer:
						return DVMPlace::AccessKind::CPointer;
					default:
						return DVMPlace::AccessKind::Direct;
					}
				};

				if (current_place.isDirect()) {
					// In this case we have a direct stack variable which stores a pointer.
					// Dereferencing means we now treat the local as a pointer.
					current_place
						= current_place.withAccessKind(access_for_layout(current_pointer_layout));
					current_layout = pointee_layout;
				} else {
					vm::code::TypeOfData vm_loaded_type
						= **program_context.lowerAndKeepTslType(current_layout);

					DVMPlace loaded_val_tmp = loadFromPlace(current_place, vm_loaded_type);

					// Update types after the projection has been applied.
					current_place
						= loaded_val_tmp.withAccessKind(access_for_layout(current_pointer_layout));
					current_layout = pointee_layout;
				}
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
					= **program_context.lowerAndKeepTslType(current_layout);

				// Prepare the pointer to field type.
				const usize field_index
					= class_layout.getLayoutIndexOfFieldSymbol(field.field_id).value();
				CRef<tsl::TypeLayout> field_layout
					= class_layout.getFieldLayoutOfLayoutIndex(field_index);
				const vm::code::TypeOfData vm_field_type
					= **program_context.lowerAndKeepTslType(field_layout);

				auto vm_field_name = base::strConcat("_", field_index);

				const vm::code::TypeOfData& ptr_to_field_type
					= program_context.getOrInsertPointerType(vm_field_type);

				// Create a temporary to the field
				DVMPlace field_ptr_tmp = pushTempLocal(ptr_to_field_type, "field_addr");

				// Emit the pointer move instruction. Based on the `current_place` type,
				// `structLea_pptr_pptr_field` or `structLea_pptr_pste_field` will be picked.
				pushInstruction({ vm::code::builders::OpKind::structLea,
				                  field_ptr_tmp,
				                  current_place,
				                  vm::opargs::Field{ typeName(vm_class_type),
				                                     base::StrID(vm_field_name) } });

				// `field_ptr_tmp` now holds a pointer to the appropriate struct field.
				current_place  = field_ptr_tmp.withAccessKind(DVMPlace::AccessKind::Pointer);
				current_layout = field_layout;
			}
			variant_case(lir::LIRPlace::IndexProjection, index) {
				CORE_ASSERT(
					current_layout->is<tsl::StaticArrayTypeLayout>()
						or current_layout->is<tsl::PointerTypeLayout>(),
					"IndexProjection on non-array layout"
				);

				// Resolve element layout + opcode in one place
				auto [element_layout, op_kind]
					= [&]() -> std::pair<CRef<tsl::TypeLayout>, vm::code::builders::OpKind> {
					if (current_layout->is<tsl::StaticArrayTypeLayout>()) {
						const auto& array_layout
							= std::get<tsl::StaticArrayTypeLayout>(current_layout->getVariant());

						return { array_layout.getElementLayout(),
							     vm::code::builders::OpKind::fixedSizeTableLea };
					}

					const auto& pointer_layout
						= std::get<tsl::PointerTypeLayout>(current_layout->getVariant());

					return { pointer_layout.getPointee(), vm::code::builders::OpKind::dynTableLea };
				}();

				DVMValue index_val = lowerLirValue(*index.index);

				// Prepare VM types
				const vm::code::TypeOfData& vm_element_type
					= **program_context.lowerAndKeepTslType(element_layout);
				const vm::code::TypeOfData& ptr_to_element_type
					= program_context.getOrInsertPointerType(vm_element_type);

				// Create a temporary to the element
				DVMPlace element_ptr_tmp = pushTempLocal(ptr_to_element_type, "index_addr");

				// Emit instruction (now unified)
				pushInstruction({ op_kind, element_ptr_tmp, current_place, index_val.asArgument() });

				current_place  = element_ptr_tmp.withAccessKind(DVMPlace::AccessKind::Pointer);
				current_layout = element_layout;
			}
		}
	}

	return current_place;
}

DVMValue FunctionLoweringContext::lowerLirValue(const lir::LIRValue& lir_value) {
	variant_match(lir_value.getVariant()) {
		variant_case(lir::LIRConstant, value) {
			auto dvm_type = **program_context.lowerAndKeepTslType(value.layout);
			return { lirConstantToImmediate(value, dvm_type, program_context.isCompTimeLowering()) };
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
					= **program_context.lowerAndKeepTslType(place.layout);
				auto tmp = loadFromPlace(resolved, val_type);
				// Now mark the place as direct as the value was loaded from the pointer.
				return { tmp };
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

const DVMPlace& FunctionLoweringContext::getOrInsertLirLocal(lir::LIRLocalRef local) {
	if (!lir_local_to_dvm.contains(local)) return createLirLocalToDVMMapping(local);
	return lir_local_to_dvm.at(local);
}

const DVMPlace& FunctionLoweringContext::createLirLocalToDVMMapping(lir::LIRLocalRef lir_local) {
	DVMPlace result = [&]() -> DVMPlace {
		if_opt_some(lir_local->parameter_index, index) {
			auto var_name = base::StrID(base::strConcat("arg", index));
			auto var_type = program_context.lowerAndKeepTslType(lir_local->layout);
			return { var_name, **var_type, DVMPlace::AccessKind::Direct };
		}

		if (lir_local->special_kind == lir::LIRLocalSpecialKind::ReturnValue)
			return getFunctionReturnValueLocal();

		auto var_name = base::StrID(base::strConcat("var", lir_local_to_dvm.size()));
		auto var_type = **program_context.lowerAndKeepTslType(lir_local->layout);
		return { var_name, var_type, DVMPlace::AccessKind::Direct };
	}();

	lir_local_to_dvm.put(lir_local, result);
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

void compiler::backend_vm::internal::FunctionLoweringContext::registerFunctionLocal(
	lir::LIRLocalRef lir_local
) {
	createLirLocalToDVMMapping(lir_local);
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
	if_opt_some(function_return_type, ret_type) {
		function.signature.result_types.emplace_back(vm::code::typeName(ret_type));
	}
	function.body = std::move(function_body);

	if_opt_some(fun_di_builder_opt, builder) { builder.end(); }

	return function;
}

DVMPlace FunctionLoweringContext::getFunctionReturnValueLocal() {
	CORE_ASSERT(
		function_return_type.has_value(),
		"getFunctionReturnValueLocal() called on a function with no return type"
	);

	const auto& ret_type = *function_return_type;
	if (function_name == "main") {
		CORE_ASSERT(
			function_return_type
				== vm::code::TypeOfData{ vm::code::PrimitiveType(base::StrID("i64"), Bytes{ 8 }) },
			"Main function must have i64 return type"
		);
	}
	return { base::StrID("ret0"),
		     ret_type,
		     DVMPlace::AccessKind::Direct,
		     DVMPlace::SpecialKind::ReturnValue };
}

[[nodiscard]] const compiler::backend_vm::internal::DVMPlace& compiler::backend_vm::internal::
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

	DVMPlace dvm_local = getOrInsertLirLocal(lir_local);
	pushInstruction({
		vm::code::builders::OpKind::init,
		dvm_local.asAnyArgument(),
		vm::opargs::Type(typeName(dvm_local.getType())),
	});
}

void compiler::backend_vm::internal::FunctionLoweringContext::pushDeinit(lir::LIRLocalRef) {
	pushInstruction({ vm::code::builders::OpKind::deinit });
}

usize compiler::backend_vm::internal::FunctionLoweringContext::instructionsCount() const {
	return function_body.size();
}

void compiler::backend_vm::internal::FunctionLoweringContext::pushInitsForInstr(
	const std::vector<lir::ScopeFlag>& scope_flags
) {
	for (const auto& lifetime_flag: scope_flags) {
		if (lifetime_flag.local->parameter_index.has_value())
			continue;  // Parameters are not inited

		if (lifetime_flag.flag == lir::ScopeFlag::Flag::ScopeStart) pushInit(lifetime_flag.local);
	}
}

void compiler::backend_vm::internal::FunctionLoweringContext::pushDeinitsForInstr(
	const std::vector<lir::ScopeFlag>& scope_flags, bool& deinits_pushed
) {
	if (deinits_pushed) return;

	for (const auto& lifetime_flag: scope_flags) {
		if (lifetime_flag.local->parameter_index.has_value())
			continue;  // Parameters are deinited automatically.

		if (lifetime_flag.flag == lir::ScopeFlag::Flag::ScopeEnd) pushDeinit(lifetime_flag.local);
	}
	deinits_pushed = true;
}
