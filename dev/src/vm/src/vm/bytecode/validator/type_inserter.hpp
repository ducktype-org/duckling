// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/collections/maps.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/valid_type/type_context.hpp>

#include <vector>

namespace vm::code::detail {
	/**
	 * @brief Adds declarations to `context` as validated types.
	 *
	 * The declarations are stored and validated against each other, then `TypeResolver` builds
	 * the type each one names.
	 *
	 * @throws DuplicatedTypeError if a name is declared twice with different contents, and
	 * whichever validation error the declarations warrant.
	 * @note `context` is left in an unspecified state if this throws.
	 */
	void insertTypes(
		TypeContext&                                     context,
		const std::vector<TypeOfData>&                   new_types,
		const base::HashMap<base::StrID, FuncSignature>& function_signatures
	);
}
