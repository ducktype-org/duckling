// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
	struct SpecialTypes final {
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
	base::Optional<CRef<TypeOfData>> getBuiltinTypeByName(base::StrID type_name);

	/// @brief `dyntable{byte}`, which the bytecode can also write as `string`.
	TypeExpr stringTypeExpr();

	/// @brief `dyntable{ptr{string}}`, the program arguments, which the bytecode can also write
	/// as `argv`.
	TypeExpr argvTypeExpr();

	/**
	 * @brief What a builtin alias such as `string` stands for.
	 * @note An alias is expanded where it is read, so it never names a type of its own.
	 */
	base::Optional<TypeExpr> getBuiltinAlias(base::StrID name);
}
