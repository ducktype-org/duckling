#pragma once

#include <base/collections/optional.hpp>
#include <base/pointers/shared_box.hpp>

#include <vm/api/data/process_info.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/core/safe/memory/memory.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/utils/interpret.hpp>

namespace vm {
	class SafeVMProcess;
	class SafeVmValueRef;
	class IVmValueRef;

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
	 * @brief Common interface for value references. A value reference gives access to a value from
	 * outside a VM without copying it - only the referenced data is observed.
	 * @note `IVmValueRef`s can only be used within the same process where they were initialized.
	 * They cannot be transferred to different processes.
	 */
	class IVmValueRef {
	public:
		virtual ~IVmValueRef() = default;

		/** @brief Returns the high-level (compiler) type of the referenced value. */
		[[nodiscard]] virtual base::CRef<code::valid_type::ValidType> getType() const = 0;

		/** @brief Interprets the referenced data as a structured, human-inspectable variant. */
		[[nodiscard]] virtual base::Optional<InterpretedDataVariant> readData() const = 0;

		/** @brief Returns a short, human-readable representation of the referenced value. */
		[[nodiscard]] virtual std::string str() const = 0;
	};

	namespace interpreted_data_variant {
		struct Primitive final {
			const u64 value;
		};

		struct Pointer final {
			base::Optional<SharedBox<IVmValueRef>> referenced;
		};

		struct Table final {
			friend class vm::SafeVmValueRef;

		private:
			base::Ref<SafeVMProcess> process;
			vm::Pointer              begin;
			TypeCRef                 type;

		public:
			const usize            size;
			SharedBox<IVmValueRef> get(usize index);

		private:
			Table(base::Ref<SafeVMProcess> process, vm::Pointer begin, TypeCRef type, usize size);
		};

		struct Data final {
			struct FieldDesc {
				Offset                 offset = Offset(0);
				SharedBox<IVmValueRef> value;
			};

			std::vector<FieldDesc>            fields;
			base::HashMap<base::StrID, usize> field_name_map;
		};

		struct Variant final {
			u64                    type_tag = 0;
			SharedBox<IVmValueRef> referenced;
		};

		struct Function final {};

		struct Opaque final {};
	}
}
