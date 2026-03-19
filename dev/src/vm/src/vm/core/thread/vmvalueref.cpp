#include "vmvalueref.hpp"

#include <base/extend_cpp/variant_match.hpp>

#include <vm/core/process/vmprocess.hpp>


vm::VMValueRef vm::interpreted_data_variant::Table::get(usize index) {
	vm::Pointer pointer = begin.movedPointer(index * static_cast<i64>(type->getSize()));
	return vm::VMValueRef(*process.get(), type, pointer);
}


base::Optional<vm::InterpretedDataVariant> vm::VMValueRef::readData() {
	variant_match (my_type->getKindVariant()) {
		variant_case_novalue (vm::kind::Primitive) {
			const auto type_name = my_type->getName();

			i64 val = 0;

			if (type_name == base::StrID("void"))
				val = 0;
			else if (type_name == base::StrID("i64"))
				val = readBytes<i64>();
			else if (type_name == base::StrID("i32"))
				val = readBytes<i32>();
			else if (type_name == base::StrID("i16"))
				val = readBytes<i16>();
			else if (type_name == base::StrID("byte"))
				val = readBytes<char>();
			else
				throw "I have no idea...";
			
			return vm::interpreted_data_variant::Primitive { val };
		}

		variant_case (vm::kind::Pointer, pointer_kind) {
			vm::Pointer pointer = readBytes<vm::Pointer>();
			TypeCRef ptr_type = pointer_kind.inner_type;

			if (!pointer)
				return std::nullopt;

			return vm::interpreted_data_variant::Pointer {
				.referenced = VMValueRef(*my_process.get(), ptr_type, pointer),
			};
		}

		variant_case (vm::kind::DynamicTable, dyntable_kind) {
			vm::Pointer tbl_pointer = readBytes<vm::Pointer>();

			auto block_id = memory->requestBlockID(tbl_pointer.getBlock());
			auto block_data = memory->requestBlockData(block_id);

			TypeCRef tbl_type = dyntable_kind.inner_type;
			usize tbl_size = block_data.size() / static_cast<usize>(tbl_type->getSize());
			
			return vm::interpreted_data_variant::Table(my_process, tbl_pointer, tbl_type, tbl_size);
		}

		variant_case (vm::kind::FixedSizeTable, fixtable_kind) {
			vm::Pointer tbl_pointer = readBytes<vm::Pointer>();

			auto block_id = memory->requestBlockID(tbl_pointer.getBlock());
			auto block_data = memory->requestBlockData(block_id);

			TypeCRef tbl_type = fixtable_kind.inner_type;
			usize tbl_size = block_data.size() / static_cast<usize>(tbl_type->getSize());
			
			return vm::interpreted_data_variant::Table(my_process, tbl_pointer, tbl_type, tbl_size);
		}

		variant_case (vm::kind::Data, data_kind) {
			std::vector<vm::interpreted_data_variant::Structure::FieldDesc> fields;

			for (const auto& field_desc: data_kind.fields) {
				fields.push_back(vm::interpreted_data_variant::Structure::FieldDesc {
					.offset = field_desc.offset,
					.value  = VMValueRef(*my_process.get(), field_desc.type, pointed_data.movedPointer(field_desc.offset.asInt()))
				});
			}

			return vm::interpreted_data_variant::Structure {
				.fields = std::move(fields),
				.field_name_map = data_kind.field_name_map,
			};
		}

		variant_case (vm::kind::Variant, variant_kind) {
			u64 alternative_index = 0;
			auto type_tag_data_view = memory->getPointerData(pointed_data, static_cast<usize>(variant_kind.type_tag_size));
			switch (static_cast<usize>(variant_kind.type_tag_size)) {
				case 1: alternative_index = static_cast<u64>(safeReadPointerBytes<u8>(type_tag_data_view.getBegin(), 0)); break;
				case 2: alternative_index = static_cast<u64>(safeReadPointerBytes<u16>(type_tag_data_view.getBegin(), 0)); break;
				case 4: alternative_index = static_cast<u64>(safeReadPointerBytes<u32>(type_tag_data_view.getBegin(), 0)); break;
				case 8: alternative_index = static_cast<u64>(safeReadPointerBytes<u64>(type_tag_data_view.getBegin(), 0)); break;
				default: throw "Invalid variant type tag size!";
			}

			TypeCRef inner_type = variant_kind.alternatives.at(alternative_index);

			auto view_block_ref = memory->getNestedViewBlock(
				pointed_data.movedPointer(static_cast<i64>(variant_kind.type_tag_size)),
				inner_type
			);

			match_optional(view_block_ref.toOpt()) {
				opt_some(view_block) { return vm::interpreted_data_variant::Variant {
					.type_tag = alternative_index,
					.referenced = VMValueRef(*my_process.get(), inner_type, Pointer(view_block, 0)),
				}; }
				opt_none { return std::nullopt; }
			}
		}

		variant_case (vm::kind::Function, function_kind) {
			return vm::interpreted_data_variant::Function {};
		}

		variant_case (vm::kind::Opaque, opaque_kind) {
			return vm::interpreted_data_variant::Opaque {};
		}

		variant_default return std::nullopt;
	}
}


vm::interpreted_data_variant::Table::Table(base::Ref<VMProcess> process, vm::Pointer begin, TypeCRef type, usize size)
	: process(process), begin(begin), type(type), size(size) {}


vm::VMValueRef::VMValueRef(VMProcess& process, TypeCRef type, Pointer pointed_data)
	: my_process(&process), memory(&process.getMemory()), my_type(type), pointed_data(pointed_data) {}
