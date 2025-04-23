#pragma once

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/type_of_data.hpp>

namespace vm::code {
	/*
	 * @brief Types treated in a special way by the machine.
	 *
	 * These types are singled out as there is custom logic for handling them.
	 * For example, by declaring the VTablePtr as special,
	 * we can check that each class has the VTable pointer
	 * as its first member without relying on type names.
	 */
	struct SpecialTypes {
		TypeOfData vtable_ptr;

		static const SpecialTypes& get();
	};

	/*
	 * @brief Types defined by default in all programs.
	 */
	builders::TypeContextBuilder getBuiltinTypes();
}
