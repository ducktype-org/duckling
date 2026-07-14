#include "ffi_type_builder.hpp"

#include <base/except/exceptions.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/validator/valid_type/finalized_kinds.hpp>

#include <limits>

namespace vm::code::ffi_detail {
	namespace {
		/**
		 * @brief Appends the libffi types describing one structure field. A fixed-size table has
		 * no libffi counterpart, so it is flattened: the inner type repeated `element_count`
		 * times.
		 */
		void appendFieldFFITypes(
			const valid_type::ValidType&    type,
			const valid_type::ValidTypeMap& types,
			FFITypeStorage&                 storage,
			std::vector<ffi_type*>&         elements
		) {
			if (auto table = type.maybeGetKindAs<valid_type::finalized::FixedSizeTable>()) {
				std::vector<ffi_type*> element;
				appendFieldFFITypes(*types.at(table.value()->inner), types, storage, element);
				elements.reserve(elements.size() + element.size() * table.value()->element_count);
				for (usize i = 0; i < table.value()->element_count; ++i)
					elements.insert(elements.end(), element.begin(), element.end());
				return;
			}
			elements.push_back(buildFFIType(type, types, storage));
		}
	}

	ffi_type* buildFFIType(
		const valid_type::ValidType&    type,
		const valid_type::ValidTypeMap& types,
		FFITypeStorage&                 storage
	) {
		if (auto primitive = type.maybeGetKindAs<valid_type::finalized::Primitive>()) {
			auto size = usize(primitive.value()->size);
			if (type.getName() == base::StrID("f32") && size == 4) return &ffi_type_float;
			if (type.getName() == base::StrID("f64") && size == 8) return &ffi_type_double;
			switch (size) {
			case 1:
				return &ffi_type_sint8;
			case 2:
				return &ffi_type_sint16;
			case 4:
				return &ffi_type_sint32;
			case 8:
				return &ffi_type_sint64;
			default:
				break;
			}
		} else if (type.isKind<valid_type::finalized::Opaque>()) {
			// Only the builtin `cptr` opaque maps to a C pointer.
			if (type.getName() == base::StrID("cptr")) return &ffi_type_pointer;
		} else if (auto structure = type.maybeGetKindAs<valid_type::finalized::Structure>()) {
			auto elements = makeBox<std::vector<ffi_type*>>();
			for (const auto& field: structure.value()->fields)
				appendFieldFFITypes(*types.at(field.type), types, storage, *elements);
			elements->push_back(nullptr);

			auto struct_type       = makeBox<ffi_type>();
			struct_type->size      = 0;
			struct_type->alignment = 0;
			struct_type->type      = FFI_TYPE_STRUCT;
			struct_type->elements  = elements->data();

			ffi_type* result = struct_type.get();
			storage.struct_elements.push_back(std::move(elements));
			storage.struct_types.push_back(std::move(struct_type));
			return result;
		}
		CORE_PANIC("Type is not FFI-compatible");
	}

	usize flattenedFFIElementCount(
		const valid_type::ValidType& type, const valid_type::ValidTypeMap& types
	) {
		if (auto table = type.maybeGetKindAs<valid_type::finalized::FixedSizeTable>()) {
			const usize inner = flattenedFFIElementCount(*types.at(table.value()->inner), types);
			const usize count = table.value()->element_count;
			// Saturate instead of wrapping on nested-table multiplication overflow.
			if (inner != 0 && count > std::numeric_limits<usize>::max() / inner)
				return std::numeric_limits<usize>::max();
			return count * inner;
		}
		return 1;
	}

	usize totalFFIDescriptorElementCount(
		const valid_type::ValidType& type, const valid_type::ValidTypeMap& types
	) {
		constexpr usize MAX = std::numeric_limits<usize>::max();
		if (auto table = type.maybeGetKindAs<valid_type::finalized::FixedSizeTable>()) {
			const usize inner
				= totalFFIDescriptorElementCount(*types.at(table.value()->inner), types);
			const usize count = table.value()->element_count;
			if (inner != 0 && count > MAX / inner) return MAX;
			return count * inner;
		}
		if (auto structure = type.maybeGetKindAs<valid_type::finalized::Structure>()) {
			usize total = 0;
			for (const auto& field: structure.value()->fields) {
				const usize field_count
					= totalFFIDescriptorElementCount(*types.at(field.type), types);
				total = total > MAX - field_count ? MAX : total + field_count;
			}
			return total;
		}
		return 1;
	}
}
