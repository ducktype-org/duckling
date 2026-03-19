#pragma once

#include <base/collections/optional.hpp>

#include <vm/api/data/process_info.hpp>
#include <vm/core/process/memory/memory.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/utils/interpret.hpp>

namespace vm {
	class VMProcess;
	class VMValueRef;

	namespace interpreted_data_variant {
		struct Primitive;
		struct Pointer;
		struct Table;
		struct Structure;
		struct Variant;
		struct Function;
		struct Opaque;
	}

	using InterpretedDataVariant = std::variant<
		interpreted_data_variant::Primitive,
		interpreted_data_variant::Pointer,
		interpreted_data_variant::Table,
		interpreted_data_variant::Structure,
		interpreted_data_variant::Variant,
		interpreted_data_variant::Function,
		interpreted_data_variant::Opaque>;

	class VMValueRef {
		friend class VMProcess;

	private:
		Ref<VMProcess> my_process;
		Ref<Memory>    memory;
		TypeCRef       my_type;
		Pointer        pointed_data;

	public:
		VMValueRef(VMProcess& process, TypeCRef type, Pointer pointed_data);

		[[nodiscard]] TypeCRef                               getType() const;
		[[nodiscard]] base::Optional<InterpretedDataVariant> readData() const;

		template<class T>
		T readBytes(const usize offset = 0) const {
			CORE_ASSERT(
				my_type->getName() != base::StrID("void"),
				"Interpreting VmValueRef bytes of type void!"
			);
			CORE_ASSERT(
				offset + sizeof(T) <= static_cast<usize>(my_type->getSize()),
				"VmValueRef: Out of bounds read"
			);

			auto view = memory->getPointerData(pointed_data, offset + sizeof(T));
			return vm::safeReadPointerBytes<T>(view.getBegin(), offset);
		}
	};

	namespace interpreted_data_variant {
		struct Primitive {
			const i64 value;
		};

		struct Pointer {
			base::Optional<VMValueRef> referenced;
		};

		struct Table {
			friend class vm::VMValueRef;

		private:
			base::Ref<VMProcess> process;
			vm::Pointer          begin;
			TypeCRef             type;

		public:
			const usize size;
			VMValueRef  get(usize index);

		private:
			Table(base::Ref<VMProcess> process, vm::Pointer begin, TypeCRef type, usize size);
		};

		struct Structure {
			struct FieldDesc {
				Offset     offset;
				VMValueRef value;
			};

			std::vector<FieldDesc>            fields;
			base::HashMap<base::StrID, usize> field_name_map;
		};

		struct Variant {
			u64        type_tag;
			VMValueRef referenced;
		};

		struct Function {};

		struct Opaque {};
	}
}
