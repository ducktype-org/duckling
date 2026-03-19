#pragma once

#include <vm/api/data/process_info.hpp>
#include <vm/core/process/memory/memory.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/utils/interpret.hpp>

namespace vm {
	class VMProcess;
	class VMValueRef;

	namespace interpreted_data_variant {
		struct Primitive {
			const i64 value;
		};

		struct Pointer {
			base::Optional<VMValueRef> referenced;
		};

		struct Table {
			friend class vm::VMValueRef;

			const usize size;
			VMValueRef get(usize index);

		private:
			base::Ref<VMProcess> process;
			vm::Pointer begin;
			TypeCRef type;

			Table(base::Ref<VMProcess> process, vm::Pointer begin, TypeCRef type, usize size);
		};

		struct Structure {};

		struct Variant {};

		struct Function {};

		struct Opaque {};
	}

	using InterpretedDataVariant = std::variant<
			interpreted_data_variant::Primitive,
			interpreted_data_variant::Pointer,
			interpreted_data_variant::Table,
			interpreted_data_variant::Structure,
			interpreted_data_variant::Variant,
			interpreted_data_variant::Function,
			interpreted_data_variant::Opaque,
	>;

	class VMValueRef {
		friend class VMProcess;

	private:
		Ref<VMProcess> my_process;
		Ref<Memory> memory;
		TypeCRef my_type;
		Pointer pointed_data;

	public:
		VMValueRef(VMProcess& process, TypeCRef type, Pointer pointed_data);
		VMValueRef(VMProcess& process);

		base::Optional<InterpretedDataVariant> readData();

		template<class T>
		T readBytes(const usize offset = 0) const {
			CORE_ASSERT(
				type->getName() != base::StrID("void"), "Interpreting VmValue bytes of type void!"
			);
			CORE_ASSERT(offset + sizeof(T) <= data.size(), "VmValue: Out of bounds read");
			return vm::safeReadPointerBytes<T>(data.data(), offset);
		}
	};
}