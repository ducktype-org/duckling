#include <base/box.hpp>
#include <base/string_id.hpp>

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/builtin_functions.hpp>

namespace vm::code {
	/**
	 * @brief Create a TypeContextBuilder with builtin types.
	 * @note The types defined here are used by the builtin functions.
	 */
	code::builders::TypeContextBuilder getBuiltinTypes() {
		static const std::array builtin_types = {
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

			// @todo: The approach with a static table of size 10 is temporary.
			// It should be changed to a dynamic_table of strings or bytes once those are
			// implemented. https://github.com/ducktype-org/duckling/issues/725
			TypeOfData(StaticTableType(base::StrID("argv"), base::StrID("i64"), 10)),
			TypeOfData(PointerType(base::StrID("ptr_argv"), base::StrID("argv"))),
			TypeOfData(FunctionType(
				base::StrID("main"),
				{ base::StrID("i64"), base::StrID("ptr_argv") },
				base::StrID("i64")
			)),
		};
		builders::TypeContextBuilder type_context_builder;

		for (const auto& tp: builtin_types) type_context_builder.addType(tp);
		for (const auto& func: builtins::getBuiltinFunctions())
			type_context_builder.addType(func.type);

		return type_context_builder;
	}
}
