#pragma once
#include <base/box.hpp>
#include <base/string_id.hpp>

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/type_of_data.hpp>

namespace vm::code {
	code::builders::TypeContextBuilder getBuiltinTypes();
}
