#include "ffi_type_builder.hpp"

#include <base/except/exceptions.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/validator/valid_type/finalized_kinds.hpp>

namespace vm::code::ffi_detail {
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
			return &ffi_type_pointer;
		} else if (auto structure = type.maybeGetKindAs<valid_type::finalized::Structure>()) {
			auto elements = makeBox<std::vector<ffi_type*>>();
			for (const auto& field: structure.value()->fields)
				elements->push_back(buildFFIType(*types.at(field.type), types, storage));
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
}
