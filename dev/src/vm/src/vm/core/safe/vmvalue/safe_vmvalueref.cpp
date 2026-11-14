// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "safe_vmvalueref.hpp"

#include <base/extend_cpp/variant_match.hpp>

#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/safe_vmprocess.hpp>

vm::SafeVMValueBytes::SafeVMValueBytes(usize size): size(size) {
	if (size > INLINE_SIZE) heap_bytes.resize(size);
}

base::TypedModRawView<byte> vm::SafeVMValueBytes::from(u64 offset) {
	if (freed) throw exceptions::VMUseAfterFreeException();
	if (offset > size) throw exceptions::VMOutOfBlockBoundsException();
	return { data() + offset, size - offset };
}

vm::SafeDataLocation vm::movedLocation(const SafeDataLocation& location, u64 bytes) {
	variant_match(location) {
		variant_case(Pointer, pointer) { return pointer.movedPointer(bytes); }
		variant_case(ValueBytesLocation, value_bytes) {
			return ValueBytesLocation{ .bytes  = value_bytes.bytes,
				                       .offset = value_bytes.offset + bytes };
		}
	}
	CORE_UNREACHABLE();
}

SharedBox<vm::IVMValueRef> vm::SafeVMValueRef::makeShared(
	SafeVMProcess& process, TypeCRef type, SafeDataLocation location
) {
	// Build a concrete `SharedBox<SafeVMValueRef>` (its control block deletes as `SafeVMValueRef`)
	// and let it convert to the interface-typed box on return.
	return makeSharedBox<SafeVMValueRef>(process, type, std::move(location));
}

vm::SafeTableElementAccess::SafeTableElementAccess(
	SafeVMProcess& process, TypeCRef element_type, SafeDataLocation begin
):
	  my_process(&process),
	  element_type(element_type),
	  begin(std::move(begin)) {}

SharedBox<vm::IVMValueRef> vm::SafeTableElementAccess::get(usize index) const {
	return SafeVMValueRef::makeShared(
		*my_process.get(),
		element_type,
		movedLocation(begin, index * static_cast<usize>(element_type->getSize()))
	);
}

base::ModRawView vm::SafeTableElementAccess::asBytesView() const {
	variant_match(begin) {
		variant_case(Pointer, pointer) {
			return my_process->getMemory().getRemainingPointerData(pointer);
		}
		variant_case(ValueBytesLocation, value_bytes) {
			auto view = value_bytes.bytes->from(value_bytes.offset);
			return { view.getBegin(), view.size() };
		}
	}
	CORE_UNREACHABLE();
}

namespace vm {
	namespace {
		/** @brief Builds an interpreted table backed by lazy element access to safe VM memory. */
		interpreted_data_variant::Table makeInterpretedTable(
			SafeVMProcess& process, TypeCRef element_type, const SafeDataLocation& begin, usize size
		) {
			return { .elements = makeSharedBox<SafeTableElementAccess>(process, element_type, begin),
				     .size = size };
		}
	}
}

base::Optional<vm::InterpretedDataVariant> vm::SafeVMValueRef::readData() const {
	variant_match(my_type->getKindVariant()) {
		variant_case_novalue(vm::kind::Primitive) {
			const auto type_name = my_type->getName();

			u64 val = 0;

			if (type_name == base::StrID("i64"))
				val = readBytes<u64>();
			else if (type_name == base::StrID("i32"))
				val = readBytes<u32>();
			else if (type_name == base::StrID("i16"))
				val = readBytes<u16>();
			else if (type_name == base::StrID("byte") || type_name == base::StrID("i8"))
				val = static_cast<u64>(readBytes<u8>());

			return vm::interpreted_data_variant::Primitive{ val };
		}

		variant_case(vm::kind::Pointer, pointer_kind) {
			auto     pointer  = readBytes<vm::Pointer>();
			TypeCRef ptr_type = pointer_kind.inner_type;

			if (!pointer) return vm::interpreted_data_variant::Pointer{ std::nullopt };

			return vm::interpreted_data_variant::Pointer{
				makeShared(*my_process.get(), ptr_type, pointer)
			};
		}

		variant_case(vm::kind::DynamicTable, dyntable_kind) {
			// A dynamic table's size is its block's, and only a block can hold one.
			const auto* pointed_data = std::get_if<Pointer>(&location);
			if (pointed_data == nullptr) return std::nullopt;

			auto block_id   = memory->requestBlockID(pointed_data->getBlock());
			auto block_data = memory->requestBlockData(block_id);

			usize tbl_size
				= block_data.size() / static_cast<usize>(dyntable_kind.inner_type->getSize());

			return makeInterpretedTable(
				*my_process.get(), dyntable_kind.inner_type, *pointed_data, tbl_size
			);
		}

		variant_case(vm::kind::FixedSizeTable, fixtable_kind) {
			// Static, even when the table is only a part of its block.
			const usize tbl_size = static_cast<usize>(my_type->getSize())
			                     / static_cast<usize>(fixtable_kind.inner_type->getSize());

			return makeInterpretedTable(
				*my_process.get(), fixtable_kind.inner_type, location, tbl_size
			);
		}

		variant_case(vm::kind::Data, data_kind) {
			std::vector<vm::interpreted_data_variant::Data::FieldDesc> fields;

			fields.reserve(data_kind.fields.size());
			for (const auto& field_desc: data_kind.fields) {
				fields.push_back(vm::interpreted_data_variant::Data::FieldDesc{
					.offset = field_desc.offset,
					.value  = makeShared(
                        *my_process.get(),
                        field_desc.type,
                        movedLocation(location, static_cast<u64>(field_desc.offset.asInt()))
                    ) });
			}

			return vm::interpreted_data_variant::Data{
				.fields         = std::move(fields),
				.field_name_map = data_kind.field_name_map,
			};
		}

		variant_case(vm::kind::Variant, variant_kind) {
			u64  alternative_type_tag = 0;
			auto type_tag_data_view   = dataView(static_cast<usize>(variant_kind.type_tag_size));
			switch (static_cast<usize>(variant_kind.type_tag_size)) {
			case 1:
				alternative_type_tag
					= static_cast<u64>(safeReadPointerBytes<u8>(type_tag_data_view.getBegin(), 0));
				break;
			case 2:
				alternative_type_tag
					= static_cast<u64>(safeReadPointerBytes<u16>(type_tag_data_view.getBegin(), 0));
				break;
			case 4:
				alternative_type_tag
					= static_cast<u64>(safeReadPointerBytes<u32>(type_tag_data_view.getBegin(), 0));
				break;
			case 8:
				alternative_type_tag
					= static_cast<u64>(safeReadPointerBytes<u64>(type_tag_data_view.getBegin(), 0));
				break;
			default:
				throw "Invalid variant type tag size!";
			}

			if (alternative_type_tag == 0) return std::nullopt;

			const u64 alternative_index = alternative_type_tag - 1;
			TypeCRef  inner_type        = variant_kind.alternatives.at(alternative_index);

			// The VM types a set payload with a view block, which plain bytes never have.
			const auto* pointed_data = std::get_if<Pointer>(&location);
			if (pointed_data == nullptr) return std::nullopt;

			auto view_block_ref = memory->getNestedViewBlock(
				pointed_data->getBlock(),
				pointed_data->getOffset() + static_cast<u64>(variant_kind.type_tag_size),
				inner_type
			);

			match_optional(view_block_ref.toOpt()) {
				opt_some(view_block) {
					return vm::interpreted_data_variant::Variant{
						.alternative_index = alternative_index,
						.referenced
						= makeShared(*my_process.get(), inner_type, Pointer(view_block, 0)),
					};
				}
				opt_none { return std::nullopt; }
			}
		}

		variant_case(vm::kind::Function, function_kind) {
			return vm::interpreted_data_variant::Function{};
		}

		variant_case(vm::kind::Opaque, opaque_kind) {
			return vm::interpreted_data_variant::Opaque{};
		}

		variant_case(vm::kind::CPointer, cpointer_kind) {
			// A raw native address is opaque to the VM's value inspection.
			return vm::interpreted_data_variant::Opaque{};
		}
	}

	CORE_UNREACHABLE();
}

vm::SafeVMValueRef::SafeVMValueRef(SafeVMProcess& process, TypeCRef type, SafeDataLocation location):
	  my_process(&process),
	  memory(&process.getMemory()),
	  my_type(type),
	  location(std::move(location)) {}

base::TypedModRawView<byte> vm::SafeVMValueRef::dataView(usize size) const {
	variant_match(location) {
		variant_case(Pointer, pointed_data) { return memory->getPointerData(pointed_data, size); }
		variant_case(ValueBytesLocation, value_bytes) {
			auto view = value_bytes.bytes->from(value_bytes.offset);
			if (size > view.size()) throw exceptions::VMOutOfBlockBoundsException();
			return { view.getBegin(), size };
		}
	}
	CORE_UNREACHABLE();
}

base::CRef<vm::code::valid_type::ValidType> vm::SafeVMValueRef::getType() const {
	auto        type_id = static_cast<code::valid_type::ValidTypeID>(my_type->getID().asInt());
	const auto& types   = my_process->loader.getHighProgram()->types();
	return types.at(type_id);
}

std::string vm::SafeVMValueRef::str() const {
	auto var = readData();
	if (!var.has_value()) return "<none>";

	variant_match(var.value()) {
		variant_case(interpreted_data_variant::Primitive, val) { return std::to_string(val.value); }
		variant_case(interpreted_data_variant::Pointer, val) {
			if (!val.referenced.has_value()) return "null";
			return "<pointer>";
		}
		variant_case_novalue(interpreted_data_variant::Table) { return "<table>"; }
		variant_case_novalue(interpreted_data_variant::Data) { return "<data>"; }
		variant_case_novalue(interpreted_data_variant::Variant) { return "<variant>"; }
		variant_case_novalue(interpreted_data_variant::Function) { return "<function>"; }
		variant_case_novalue(interpreted_data_variant::Opaque) { return "<opaque>"; }
	}
	return "<unknown>";
}

bool vm::SafeVMValueRef::isComplex() const {
	variant_match(my_type->getKindVariant()) {
		variant_case_novalue(std::monostate, kind::Primitive, kind::Function) { return false; }
		variant_case(kind::Pointer, val) {
			auto opt_data = readData().value();
			variant_match(opt_data) {
				variant_case(interpreted_data_variant::Pointer, val) {
					return val.referenced.has_value();
				}
			}
			return false;
		}
	}
	return true;
}
