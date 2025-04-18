#include <base/box.hpp>
#include <base/string_id.hpp>

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/type_of_data.hpp>

namespace vm::code::builtin_types {
	code::builders::TypeContextBuilder getBuiltinTypes() {
		const std::array builtin_types = {
			TypeOfData(PrimitiveType(base::StrID("byte"), 1)),
			TypeOfData(PrimitiveType(base::StrID("i16"), 2)),
			TypeOfData(PrimitiveType(base::StrID("i32"), 4)),
			TypeOfData(PrimitiveType(base::StrID("i64"), 8)),
			TypeOfData(PointerType(base::StrID("ptr_i16"), base::StrID("i16"))),
			TypeOfData(PointerType(base::StrID("ptr_i32"), base::StrID("i32"))),
			TypeOfData(PointerType(base::StrID("ptr_i64"), base::StrID("i64"))),

			// @todo: void size is a thing to discuss. This will probably change after:
			// https://github.com/ducktype-org/duckling/issues/656
			TypeOfData(PrimitiveType(base::StrID("void"), 0)),

			// TODO: This is temporary. Just to see if passing arguments work, since dynamic
			// arrays don't work for now.
			// TypeOfData(DynamicTableType(base::StrID("byte_array"), base::StrID("byte"))),
			TypeOfData(StaticTableType(base::StrID("temp_arg_arr"), base::StrID("i64"), 10)),
			TypeOfData(PointerType(base::StrID("ptr_argv"), base::StrID("temp_arg_arr"))),
			TypeOfData(FunctionType(
				base::StrID("main"),
				{ base::StrID("i64"), base::StrID("ptr_argv") },
				base::StrID("i64")
			)),
		};
		builders::TypeContextBuilder type_context_builder;

		for (const auto& tp: builtin_types) type_context_builder.addType(tp);

		return type_context_builder;
	}
}
