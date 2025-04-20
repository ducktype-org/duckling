#include <base/box.hpp>
#include <base/string_id.hpp>

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/type_of_data.hpp>

namespace vm::code {
	code::builders::TypeContextBuilder getBuiltinTypes() {
		static const std::array builtin_types = {
			TypeOfData(PrimitiveType(base::StrID("i16"), 2)),
			TypeOfData(PrimitiveType(base::StrID("i32"), 4)),
			TypeOfData(PrimitiveType(base::StrID("i64"), 8)),
			TypeOfData(PointerType(base::StrID("ptr_i16"), base::StrID("i16"))),
			TypeOfData(PointerType(base::StrID("ptr_i32"), base::StrID("i32"))),
			TypeOfData(PointerType(base::StrID("ptr_i64"), base::StrID("i64"))),

			// @todo: void size is a thing to discuss. This will probably change after:
			// https://github.com/ducktype-org/duckling/issues/656
			TypeOfData(PrimitiveType(base::StrID("void"), 0)),

			// @TODO this is a placeholder until we add it as a proper type
			// of a new kind.
			TypeOfData(PrimitiveType(base::StrID("VT"), 8)),
		};
		builders::TypeContextBuilder type_context_builder;

		for (const auto& tp: builtin_types) type_context_builder.addType(tp);

		return type_context_builder;
	}
}
