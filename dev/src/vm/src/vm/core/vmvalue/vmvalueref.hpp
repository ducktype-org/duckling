#pragma once

#include <base/collections/optional.hpp>

#include <vm/api/data/process_info.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/core/safe/memory/memory.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/utils/interpret.hpp>

namespace vm {
	class SafeVMProcess;
	class VMValueRef;

	namespace interpreted_data_variant {
		struct Primitive;
		struct Pointer;
		struct Table;
		struct Data;
		struct Variant;
		struct Function;
		struct Opaque;
	}

	using InterpretedDataVariant = std::variant<
		interpreted_data_variant::Primitive,
		interpreted_data_variant::Pointer,
		interpreted_data_variant::Table,
		interpreted_data_variant::Data,
		interpreted_data_variant::Variant,
		interpreted_data_variant::Function,
		interpreted_data_variant::Opaque>;

	/**
	 * @brief Reference for a value. It is meant to give access to the value from outside a VM.
	 * It is NOT meant to be used by the internal memory module.
	 * @note Passed data is NOT copied - only referenced.
	 * @note `VmValueRef`s can only be used within the same process where they were initialized.
	 * They cannot be transferred to different processes.
	 */
	class VMValueRef final {
		friend class VMProcess;

	private:
		const Ref<SafeVMProcess> my_process;
		const Ref<Memory>        memory;
		const TypeCRef           my_type;
		const Pointer            pointed_data;

	public:
		VMValueRef(SafeVMProcess& process, const TypeCRef type, const Pointer pointed_data);

		bool operator==(const VMValueRef&) const = default;

		[[nodiscard]] base::CRef<code::valid_type::ValidType> getType() const;
		[[nodiscard]] base::Optional<InterpretedDataVariant>  readData() const;
		std::string                                           str();

		template<class T>
		requires std::is_trivially_copy_constructible_v<T> T readBytes() const {
			CORE_ASSERT(my_type->getName() != "void", "Interpreting VmValueRef bytes of type void!");
			CORE_ASSERT(
				sizeof(T) <= static_cast<usize>(my_type->getSize()), "VmValueRef: Out of bounds read"
			);

			auto view = memory->getPointerData(pointed_data, sizeof(T));
			return vm::safeReadPointerBytes<T>(view.getBegin());
		}
	};

	namespace interpreted_data_variant {
		struct Primitive final {
			const u64 value;
		};

		struct Pointer final {
			base::Optional<VMValueRef> referenced;
		};

		struct Table final {
			friend class vm::VMValueRef;

		private:
			base::Ref<SafeVMProcess> process;
			vm::Pointer              begin;
			TypeCRef                 type;

		public:
			const usize size;
			VMValueRef  get(usize index);

		private:
			Table(base::Ref<SafeVMProcess> process, vm::Pointer begin, TypeCRef type, usize size);
		};

		struct Data final {
			struct FieldDesc {
				Offset     offset = Offset(0);
				VMValueRef value;
			};

			std::vector<FieldDesc>            fields;
			base::HashMap<base::StrID, usize> field_name_map;
		};

		struct Variant final {
			u64        type_tag = 0;
			VMValueRef referenced;
		};

		struct Function final {};

		struct Opaque final {};
	}
}
