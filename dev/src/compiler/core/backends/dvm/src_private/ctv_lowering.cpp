// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "ctv_lowering.hpp"

#include "common.hpp"
#include "function_lowering_context.hpp"
#include "program_lowering_context.hpp"

#include <ctv/ctv.hpp>
#include <ctv/numeric_value.hpp>
#include <lir/lir_structure/lir_structure.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include "diagnostic/placeholder.hpp"
#include <string_id/string_id.hpp>

#include <vm/bytecode/builders/instruction_builder.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>

namespace compiler::backend_vm::internal {
	namespace {
		using vm::code::builders::OpKind;

		u64 numericValueToU64(compiler::numeric_value::NumericValue numeric) {
			return std::visit(
				[&](auto&& val) -> u64 { return translateToU64(val); }, numeric.getStorage()
			);
		}

		/**
		 * @brief Represents a meta type as the DVM opaque pointer value: a pointer to the symbol
		 * type in the comp-time lowering, zero otherwise.
		 */
		u64 metaTypeToU64(ProgramLoweringContext& pctx, const compiler::tsh::SymbolType<>& type_val) {
			return pctx.isCompTimeLowering() ? std::bit_cast<u64>(&type_val) : 0;
		}

	}  // namespace

	DVMValue CTVLowering::lowerValue(
		FunctionLoweringContext& fctx, const lir::LIRConstant& constant
	) {
		ProgramLoweringContext& pctx = fctx.program_context;
		// We have to save the typename here.
		const vm::code::TypeOfData& type = *pctx.lowerAndKeepTslType(constant.layout);

		variant_match(constant.value.getStorage()) {
			variant_case(compiler::numeric_value::NumericValue, numeric) {
				return DVMValue{ DVMImmediate{ numericValueToU64(numeric), type } };
			}
			variant_case(char, value) {
				return DVMValue{ DVMImmediate{ translateToU64(value), type } };
			}
			variant_case(bool, value) {
				return DVMValue{ DVMImmediate{ translateToU64(value), type } };
			}
			variant_default {
				return DVMValue{ lowerCTVToNewGlobal(pctx, constant.value, type, true) };
			}
		}
	}

	const DVMPlace& CTVLowering::lowerCTVToNewGlobal(
		ProgramLoweringContext&      pctx,
		const ctv::CompileTimeValue& constant,
		const vm::code::TypeOfData&  type,
		bool                         is_constant,
		base::Optional<base::StrID>  lowered_global_name
	) {
		// copyValueOrElse and not copyValueOr: getAnonymousGlobalName bumps a counter, so asking
		// for a name we already have would burn one for nothing.
		base::StrID global_name = lowered_global_name.copyValueOrElse([&pctx] {
			return pctx.getAnonymousGlobalName(base::StrID("ctv_value"));
		});

		vm::code::GlobalData global_data{};
		global_data.name        = global_name;
		global_data.type        = typeName(type);
		global_data.is_constant = is_constant;

		const DVMPlace& inserted_global_place = pctx.declareGlobal(global_name, type);

		variant_match(constant.getStorage()) {
			variant_case(compiler::numeric_value::NumericValue, numeric) {
				// Numeric values are representable as a single immediate, so we can just store the
				// value directly in the global's `initial_value`.
				global_data.initial_value = vm::code::ConstantValue::fromU64AndSize(
					numericValueToU64(numeric), std::get<vm::code::PrimitiveType>(type).size
				);
			}
			variant_case(bool, value) {
				global_data.initial_value = vm::code::ConstantValue::fromU64AndSize(
					translateToU64(value), std::get<vm::code::PrimitiveType>(type).size
				);
			}
			variant_case(char, value) {
				global_data.initial_value = vm::code::ConstantValue::fromU64AndSize(
					translateToU64(value), std::get<vm::code::PrimitiveType>(type).size
				);
			}
			variant_case(compiler::tsh::SymbolType<>, type_val) {
				// @TODO: #1709 RTTI when the is_comp_time_lowering == false
				// Representation of a meta type in DVM is a pointer to the symbol type.
				global_data.initial_value = vm::code::ConstantValue::fromU64AndSize(
					metaTypeToU64(pctx, type_val), v_get(type, vm::code::OpaqueType).size
				);
			}
			variant_case(ctv::CompileTimeValue::TypeTuple, tuple) {
				global_data.initial_value = vm::code::ConstantValue::fromData(
					lowerTypeTupleToConstant(pctx, tuple, v_get(type, vm::code::DataType))
				);
			}
			variant_case(ctv::CompileTimeValue::CharSliceValue, str) {
				auto ctor
					= lowerStringLiteral(pctx, global_name, inserted_global_place, str.value, false);
				global_data.ctor_name = ctor.ctor.name;
				pctx.extra_bytecode_functions.push_back(std::move(ctor.ctor));
			}
			variant_case(ctv::CompileTimeValue::StringClassValue, value) {
				auto ctor = lowerStringLiteral(
					pctx, global_name, inserted_global_place, value.value, true
				);
				global_data.ctor_name = ctor.ctor.name;
				pctx.extra_bytecode_functions.push_back(std::move(ctor.ctor));
			}
			variant_case(ctv::CompileTimeValue::VMValue, vm_value) {
				if (pctx.isCompTimeLowering()) {
					auto ctor = lowerVMValue(pctx, global_name, inserted_global_place, vm_value);
					global_data.ctor_name = ctor.ctor.name;
					pctx.extra_bytecode_functions.push_back(std::move(ctor.ctor));
				} else {
					pctx.query_ctx.value()->log<dia::NotYetImplementedCodeError>(
						"VMValue based comp-time values to file lowering."
					);
					query::throwFailed();
				}
			}
		}

		pctx.defineGlobal(std::move(global_data));
		return inserted_global_place;
	}

	CTVLowering::CtorLoweringResult CTVLowering::lowerStringLiteral(
		ProgramLoweringContext& pctx,
		base::StrID             global_name,
		const DVMPlace&         inserted_global_place,
		base::StrID             content,
		bool                    is_string_class
	) {
		// Both char-backed types start with a pointer to the characters followed by their length:
		// a char slice is `{ _0: ptr-to-dynamic-table, _1: i64 length }` and a `String` is
		// `{ pointer, len, cap, offset }`. We derive every type we need from the first field, so
		// the static byte array, the dynamic-table pointer and the `fstToDynTable` cast all agree
		// on the element type.
		const auto& struct_data   = std::get<vm::code::DataType>(inserted_global_place.getType());
		base::StrID ptr_type_name = struct_data.fields.at(0).type;
		const auto& dyn_ptr_type  = pctx.type_storage.dvm_types.at(ptr_type_name);

		// ================ Part 1: insert fixed-size table with string bytes =================
		auto element_type = DVMImmediate::character('\0').type;

		const auto  bytes  = content.strView();
		const usize length = bytes.size();

		// Static fixed-size-table global holding the string bytes.
		auto array_type_name = base::strConcat("arr_", typeName(element_type), "_", length);
		vm::code::FixedSizeTableType array_type{ base::StrID(array_type_name),
			                                     typeName(element_type),
			                                     length };
		pctx.keepVMType(array_type);

		auto string_table = makeBox<vm::code::ConstantFixedSizeTable>();
		string_table->elements.reserve(length);
		for (char byte: bytes)
			string_table->elements.emplace_back(
				makeBox<vm::code::ConstantImmediate>(vm::code::ConstantImmediate::fromU64AndSize(
					translateToU64((unsigned char) byte), Bytes{ 1 }
				))
			);

		const DVMPlace& array_global = pctx.insertStaticDataGlobal(
			base::StrID("str_data"),
			array_type,
			vm::code::ConstantValue::fromData(std::move(string_table))
		);

		// ================ Part 2: build constructor to assemble the slice ================
		auto ctor_name = base::StrID(base::strConcat(global_name.strView(), "_ctor"));
		FunctionLoweringContext ctor_ctx
			= FunctionLoweringContext::getVoidParameterLessFunctionContext(pctx, ctor_name);

		// ref -> pointer to the static fixed-size table, then reinterpret as a
		// dynamic-table pointer (the slice's `_0`).
		const vm::code::TypeOfData& ptr_to_array_type
			= pctx.getOrInsertPointerType(typeName(array_type));
		DVMPlace fst_ptr = ctor_ctx.pushTempLocal(ptr_to_array_type, "str_fst_ptr");
		ctor_ctx.pushInstruction({ OpKind::ref, fst_ptr.asArgument(), array_global.asAnyArgument() }
		);
		DVMPlace dyn_ptr = ctor_ctx.pushTempLocal(dyn_ptr_type, "str_dyn_ptr");
		ctor_ctx.pushInstruction(
			{ OpKind::fstToDynTable, dyn_ptr.asArgument(), fst_ptr.asArgument() }
		);

		if (not is_string_class) {
			// `str` is `{ pointer, len }`.
			constructStructureFromValues(
				ctor_ctx,
				inserted_global_place,
				struct_data,
				{ DVMValue{ dyn_ptr }, DVMValue{ DVMImmediate::u64(length) } }
			);
		} else {
			// `String` is `{ pointer, len, cap, offset }`. The buffer is a static global exactly
			// as long as its content, so the capacity is the length and the offset is zero.
			constructStructureFromValues(
				ctor_ctx,
				inserted_global_place,
				struct_data,
				{ DVMValue{ dyn_ptr },
			      DVMValue{ DVMImmediate::u64(length) },
			      DVMValue{ DVMImmediate::u64(length) },
			      DVMValue{ DVMImmediate::u64(0) } }
			);
		}

		ctor_ctx.cleanUpRegisteredTemps();
		ctor_ctx.pushInstruction(vm::code::builders::InstructionBuilder(OpKind::ret).build());

		return CtorLoweringResult{ .ctor = std::move(ctor_ctx).finish() };
	}

	CTVLowering::CtorLoweringResult CTVLowering::lowerVMValue(
		ProgramLoweringContext&               pctx,
		base::StrID                           global_name,
		const DVMPlace&                       inserted_global_place,
		const ctv::CompileTimeValue::VMValue& vm_value
	) {
		auto ctor_name = base::StrID(base::strConcat(global_name.strView(), "_ctor"));
		FunctionLoweringContext ctor_ctx
			= FunctionLoweringContext::getVoidParameterLessFunctionContext(pctx, ctor_name);

		const auto& type = inserted_global_place.getType();
		DVMPlace    local(base::StrID("vm_value"), type, DVMPlace::AccessKind::Direct);
		ctor_ctx.pushInstruction({ OpKind::initFromVMValue,
		                           local.asAnyArgument(),
		                           vm::opargs::Type(typeName(type)),
		                           vm::opargs::Immediate{ std::bit_cast<u64>(vm_value.val.get()) } }
		);
		ctor_ctx.maybeStoreResult(inserted_global_place, DVMValue{ local });
		ctor_ctx.pushInstruction({ OpKind::deinit });
		ctor_ctx.pushInstruction(vm::code::builders::InstructionBuilder(OpKind::ret).build());

		return CtorLoweringResult{ .ctor = std::move(ctor_ctx).finish() };
	}

	void CTVLowering::constructStructureFromValues(
		FunctionLoweringContext&     ctor_ctx,
		const DVMPlace&              destination,
		const vm::code::DataType&    structure_type,
		const std::vector<DVMValue>& values
	) {
		CORE_ASSERT(
			structure_type.fields.size() == values.size(),
			"Number of values does not match number of fields in structure"
		);
		for (usize i = 0; i < structure_type.fields.size(); ++i) {
			auto& field       = structure_type.fields[i];
			auto& field_value = values[i];

			auto ptr_to_field_type = ctor_ctx.program_context.getOrInsertPointerType(field.type);
			DVMPlace field_ptr     = ctor_ctx.pushTempLocal(ptr_to_field_type, "str_slice_field");
			ctor_ctx.pushInstruction({ OpKind::structLea,
			                           field_ptr,
			                           destination,
			                           vm::opargs::Field{ structure_type.name, field.name } });

			ctor_ctx.maybeStoreResult(
				field_ptr.withAccessKind(DVMPlace::AccessKind::Pointer), field_value
			);
		}
	}

	Box<vm::code::ConstantClass> CTVLowering::lowerTypeTupleToConstant(
		ProgramLoweringContext&                 pctx,
		const ctv::CompileTimeValue::TypeTuple& tuple,
		const vm::code::DataType&               data_type
	) {
		auto result = makeBox<vm::code::ConstantClass>();
		auto field  = data_type.fields.begin();

		for (const auto& element: tuple.getElements()) {
			variant_match(element) {
				variant_case_novalue(ctv::CompileTimeValue::UnitCTV) {}
				variant_case(compiler::tsh::SymbolType<>, type_val) {
					CORE_ASSERT(field != data_type.fields.end(), "Too few tuple fields.");
					const auto& field_type = pctx.type_storage.dvm_types.at(field->type);
					result->fields.emplace_back(
						field->name,
						makeBox<vm::code::ConstantImmediate>(
							vm::code::ConstantImmediate::fromU64AndSize(
								metaTypeToU64(pctx, type_val),
								v_get(field_type, vm::code::OpaqueType).size
							)
						)
					);
					++field;
				}
				variant_case(ctv::CompileTimeValue::TypeTuple, nested) {
					CORE_ASSERT(field != data_type.fields.end(), "Too few tuple fields.");
					const auto& field_type = pctx.type_storage.dvm_types.at(field->type);
					result->fields.emplace_back(
						field->name,
						lowerTypeTupleToConstant(pctx, nested, v_get(field_type, vm::code::DataType))
					);
					++field;
				}
			}
		}

		CORE_ASSERT(field == data_type.fields.end(), "Too many tuple fields.");
		return result;
	}
}  // namespace compiler::backend_vm::internal
