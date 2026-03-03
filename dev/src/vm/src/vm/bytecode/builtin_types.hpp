#pragma once

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

	/**
	 * @brief Returns a list of builtin type definitions.
	 * @note The types defined here are used by the builtin functions.
	 */
	const std::vector<TypeOfData>& getBuiltinTypes();

	/**
	 * @brief Returns the builtin type of a given name.
	 * @return The type or empty optional if a type with the given name doesn't exist.
	 */
	base::Optional<TypeOfData> getBuiltinTypeByName(base::StrID type_name);
}
