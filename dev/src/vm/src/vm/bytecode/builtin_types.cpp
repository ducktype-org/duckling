#include "builtin_types.hpp"

#include <base/str/string_id.hpp>

#include <vm/bytecode/type_of_data.hpp>

namespace vm::code {
	const SpecialTypes& SpecialTypes::get() {
		static const SpecialTypes instance = {
			.vtable_ptr = TypeOfData(OpaqueType(base::StrID("VTablePtr"), 8)),
		};
		return instance;
	}

	TypeContext getBuiltinTypes() {
		static const std::array builtin_types
			= {
				  // TODOP: Change the name of those. They should be names p16/...(primitive). It's
			      // misleading.
				  TypeOfData(PrimitiveType(base::StrID("byte"), 1)),
				  TypeOfData(PrimitiveType(base::StrID("i16"), 2)),
				  TypeOfData(PrimitiveType(base::StrID("i32"), 4)),
				  TypeOfData(PrimitiveType(base::StrID("i64"), 8)),
				  TypeOfData(PointerType(base::StrID("ptr_i16"), base::StrID("i16"))),
				  TypeOfData(PointerType(base::StrID("ptr_i32"), base::StrID("i32"))),
				  TypeOfData(PointerType(base::StrID("ptr_i64"), base::StrID("i64"))),

				  // @todo: void size is a thing to discuss. This will probably change after:
				  // https://github.com/ducktype-org/duckling/issues/656
				  TypeOfData(PrimitiveType(base::StrID("void"), 1)),

				  TypeOfData(DynamicTableType(base::StrID("string"), base::StrID("byte"))),
				  TypeOfData(PointerType(base::StrID("ptr_string"), base::StrID("string"))),
				  TypeOfData(DynamicTableType(base::StrID("argv"), base::StrID("ptr_string"))),
				  TypeOfData(PointerType(base::StrID("ptr_argv"), base::StrID("argv"))),

				  SpecialTypes::get().vtable_ptr,
			  };
		TypeContext type_context;

		for (const auto& tp: builtin_types) type_context.insertType(tp);

		return type_context;
	}
}
