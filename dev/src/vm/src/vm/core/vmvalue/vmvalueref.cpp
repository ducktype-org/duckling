#include "vmvalueref.hpp"

#include <base/extend_cpp/variant_match.hpp>

#include <vm/core/safe/safe_vmprocess.hpp>

vm::VMValueRef vm::interpreted_data_variant::Table::get(usize index) {
	if (index >= size) throw std::out_of_range("Table index out of range");

	vm::Pointer pointer
		= begin.movedPointer(static_cast<i64>(index * static_cast<usize>(type->getSize())));

	return { *process.get(), type, pointer };
}

base::Optional<vm::InterpretedDataVariant> vm::VMValueRef::readData() const {
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
				VMValueRef(*my_process.get(), ptr_type, pointer),
			};
		}

		variant_case(vm::kind::DynamicTable, dyntable_kind) {
			auto block_id   = memory->requestBlockID(pointed_data.getBlock());
			auto block_data = memory->requestBlockData(block_id);

			usize tbl_size
				= block_data.size() / static_cast<usize>(dyntable_kind.inner_type->getSize());

			return vm::interpreted_data_variant::Table(
				my_process, pointed_data, dyntable_kind.inner_type, tbl_size
			);
		}

		variant_case(vm::kind::FixedSizeTable, fixtable_kind) {
			auto block_id   = memory->requestBlockID(pointed_data.getBlock());
			auto block_data = memory->requestBlockData(block_id);

			usize tbl_size
				= block_data.size() / static_cast<usize>(fixtable_kind.inner_type->getSize());

			return vm::interpreted_data_variant::Table(
				my_process, pointed_data, fixtable_kind.inner_type, tbl_size
			);
		}

		variant_case(vm::kind::Data, data_kind) {
			std::vector<vm::interpreted_data_variant::Data::FieldDesc> fields;

			fields.reserve(data_kind.fields.size());
			for (const auto& field_desc: data_kind.fields) {
				fields.push_back(vm::interpreted_data_variant::Data::FieldDesc{
					.offset = field_desc.offset,
					.value  = VMValueRef(
                        *my_process.get(),
                        field_desc.type,
                        pointed_data.movedPointer(static_cast<i64>(field_desc.offset.asInt()))
                    ) });
			}

			return vm::interpreted_data_variant::Data{
				.fields         = std::move(fields),
				.field_name_map = data_kind.field_name_map,
			};
		}

		variant_case(vm::kind::Variant, variant_kind) {
			u64  alternative_index  = 0;
			auto type_tag_data_view = memory->getPointerData(
				pointed_data, static_cast<usize>(variant_kind.type_tag_size)
			);
			switch (static_cast<usize>(variant_kind.type_tag_size)) {
			case 1:
				alternative_index
					= static_cast<u64>(safeReadPointerBytes<u8>(type_tag_data_view.getBegin(), 0));
				break;
			case 2:
				alternative_index
					= static_cast<u64>(safeReadPointerBytes<u16>(type_tag_data_view.getBegin(), 0));
				break;
			case 4:
				alternative_index
					= static_cast<u64>(safeReadPointerBytes<u32>(type_tag_data_view.getBegin(), 0));
				break;
			case 8:
				alternative_index
					= static_cast<u64>(safeReadPointerBytes<u64>(type_tag_data_view.getBegin(), 0));
				break;
			default:
				throw "Invalid variant type tag size!";
			}

			TypeCRef inner_type = variant_kind.alternatives.at(alternative_index);

			auto view_block_ref = memory->getNestedViewBlock(
				pointed_data.movedPointer(static_cast<i64>(variant_kind.payload_offset)), inner_type
			);

			match_optional(view_block_ref.toOpt()) {
				opt_some(view_block) {
					return vm::interpreted_data_variant::Variant{
						.type_tag = alternative_index,
						.referenced
						= VMValueRef(*my_process.get(), inner_type, Pointer(view_block, 0)),
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
	}

	CORE_UNREACHABLE();
}

vm::interpreted_data_variant::Table::Table(
	base::Ref<SafeVMProcess> process, vm::Pointer begin, TypeCRef type, usize size
):
	  process(process),
	  begin(begin),
	  type(type),
	  size(size) {}

vm::VMValueRef::VMValueRef(SafeVMProcess& process, TypeCRef type, Pointer pointed_data):
	  my_process(&process),
	  memory(&process.getMemory()),
	  my_type(type),
	  pointed_data(pointed_data) {}

base::CRef<vm::code::valid_type::ValidType> vm::VMValueRef::getType() const {
	auto type_id = static_cast<code::valid_type::ValidTypeID>(my_type->getID().asInt());
	auto types   = my_process->loader.getHighProgram()->types();
	return types.at(type_id);
}

std::string vm::VMValueRef::str() const {
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
