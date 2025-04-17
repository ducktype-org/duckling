#include <base/box.hpp>
#include <base/string_id.hpp>

#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/builders/builders.hpp>


namespace vm::code::builtin_types {
	Box<code::builders::TypeContextBuilder> getBuiltinTypes() {
		const std::array BUILTIN_TYPES = {
			TypeOfData(PrimitiveType(base::StrID("i16"), 2)),
			TypeOfData(PrimitiveType(base::StrID("i32"), 4)),
			TypeOfData(PrimitiveType(base::StrID("i64"), 8)),
			TypeOfData(PointerType(base::StrID("ptr_i16"), base::StrID("i16"))),
			TypeOfData(PointerType(base::StrID("ptr_i32"), base::StrID("i32"))),
			TypeOfData(PointerType(base::StrID("ptr_i64"), base::StrID("i64"))),

			// @todo: void size is a thing to discuss. This will probably change after:
			// https://github.com/ducktype-org/duckling/issues/656
			TypeOfData(PrimitiveType(base::StrID("void"), 0)),
		};
		auto type_context_builder = makeBox<code::builders::TypeContextBuilder>();

		for (const auto& tp: BUILTIN_TYPES) type_context_builder->addType(tp);

		return type_context_builder;
	}
}
