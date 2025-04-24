#pragma once
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
	code::builders::TypeContextBuilder getBuiltinTypes();
}
