#include "vmvalueref.hpp"

#include <base/extend_cpp/variant_match.hpp>

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

		variant_default return std::nullopt;
	}
}

vm::interpreted_data_variant::Table::Table(base::Ref<VMProcess> process, vm::Pointer begin, TypeCRef type, usize size)
	: process(process), begin(begin), type(type), size(size) {}
