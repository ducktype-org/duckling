#include "builtin_types.hpp"

#include <base/collections/maps.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/type_of_data.hpp>

namespace vm::code {
	// Use an ordered map instead of a hash map to preserve deterministic iteration order
	// when the compiler runs in multi-threaded scenarios.
	using BuiltinTypesMap = base::Map<base::StrID, TypeOfData>;

	const SpecialTypes& SpecialTypes::get() {
		static_assert(
			sizeof(void*) == 8, "Sanity assert, that the size of VTablePtr can be equal to 8"
		);
		static const SpecialTypes instance = {
			.vtable_ptr = TypeOfData(OpaqueType(base::StrID("VTablePtr"), 8)),
		};
		return instance;
	}

	const BuiltinTypesMap& rawBuiltins() {
		const static BuiltinTypesMap types = {
			{ base::StrID("byte"), TypeOfData(PrimitiveType(base::StrID("byte"), 1)) },
			{ base::StrID("i8"), TypeOfData(PrimitiveType(base::StrID("i8"), 1)) },
			{ base::StrID("i16"), TypeOfData(PrimitiveType(base::StrID("i16"), 2)) },
			{ base::StrID("i32"), TypeOfData(PrimitiveType(base::StrID("i32"), 4)) },
			{ base::StrID("i64"), TypeOfData(PrimitiveType(base::StrID("i64"), 8)) },
			{ base::StrID("ptr_i16"),
			  TypeOfData(PointerType(base::StrID("ptr_i16"), base::StrID("i16"))) },
			{ base::StrID("ptr_i32"),
			  TypeOfData(PointerType(base::StrID("ptr_i32"), base::StrID("i32"))) },
			{ base::StrID("ptr_i64"),
			  TypeOfData(PointerType(base::StrID("ptr_i64"), base::StrID("i64"))) },
			{ base::StrID("string"),
			  TypeOfData(DynamicTableType(base::StrID("string"), base::StrID("byte"))) },
			{ base::StrID("ptr_string"),
			  TypeOfData(PointerType(base::StrID("ptr_string"), base::StrID("string"))) },
			{ base::StrID("argv"),
			  TypeOfData(DynamicTableType(base::StrID("argv"), base::StrID("ptr_string"))) },
			{ base::StrID("ptr_argv"),
			  TypeOfData(PointerType(base::StrID("ptr_argv"), base::StrID("argv"))) },
			{ base::StrID("opaque_ptr"), TypeOfData(OpaqueType(base::StrID("opaque_ptr"), 8)) },
			{ base::StrID("VTablePtr"), SpecialTypes::get().vtable_ptr },
			{ base::StrID("mutex"), TypeOfData(OpaqueType(base::StrID("mutex"), 8)) },
			{ base::StrID("condition_variable"),
			  TypeOfData(OpaqueType(base::StrID("condition_variable"), 8)) },
		};
		return types;
	}

	const std::vector<TypeOfData>& getBuiltinTypes() {
		static const std::vector<TypeOfData> types
			= [] { return rawBuiltins() | std::views::values | std::ranges::to<std::vector>(); }();
		return types;
	}

	base::Optional<TypeOfData> getBuiltinTypeByName(base::StrID type_name) {
		return rawBuiltins().atMaybeCopy(type_name);
	}
}
